#include "analytics/SurveyAnalytics.h"
#include "TestSupport.h"

#include <cmath>
#include <vector>

int main()
{
    using aquatrack::domain::Observation;
    std::vector<Observation> observations{
        {.id="1", .videoId="a", .timestampMs=100, .localTrackId=1, .globalTrackId=10,
         .box={0,0,2,2}, .className="pollock", .confidence=0.8F},
        {.id="2", .videoId="a", .timestampMs=400, .localTrackId=1, .globalTrackId=10,
         .box={1,1,3,3}, .className="pollock", .confidence=0.6F},
        {.id="3", .videoId="b", .timestampMs=1200, .localTrackId=1,
         .box={0,0,4,4}, .className="rockfish", .confidence=0.7F},
    };
    const auto summary = aquatrack::analytics::summarize(observations);
    requireTest(summary.totalObservations == 3, "observation count");
    requireTest(summary.uniqueLocalTracks == 2, "local ids scoped by video");
    requireTest(summary.uniqueGlobalTracks == 1, "global track count");
    requireTest(std::abs(summary.meanConfidence - 0.7) < 0.0001, "mean confidence");
    requireTest(summary.observationsByClass.at("pollock") == 2, "class count");
    const auto bins = aquatrack::analytics::binObservations(observations, 1000);
    requireTest(bins.size() == 2 && bins[0].observations == 2 && bins[1].observations == 1,
                "time bins");

    // A bin width far smaller than the span must not allocate one bin per
    // millisecond; the width is widened to respect the cap instead.
    const auto capped = aquatrack::analytics::binObservations(observations, 1, 8);
    requireTest(capped.size() <= 8, "bin count is capped");

    const auto species = aquatrack::analytics::speciesBreakdown(observations);
    requireTest(species.size() == 2, "two species");
    requireTest(species[0].className == "pollock", "ordered by observation count");
    requireTest(species[0].observations == 2 && species[0].uniqueTracks == 1,
                "species rollup counts");
    requireTest(std::abs(species[0].meanConfidence - 0.7) < 0.0001, "species mean confidence");
    requireTest(species[0].firstSeenMs == 100 && species[0].lastSeenMs == 400,
                "species first and last seen");

    // Track 1 in video "a" lasts 300 ms; track 1 in video "b" is instantaneous.
    const auto histogram = aquatrack::analytics::trackDurationHistogram(observations, 3);
    requireTest(histogram.size() == 3, "requested bucket count");
    std::size_t counted = 0;
    for (const auto &bucket : histogram)
        counted += bucket.tracks;
    requireTest(counted == 2, "every track lands in a bucket");
    requireTest(histogram.front().tracks >= 1, "instantaneous track in the first bucket");

    requireTest(aquatrack::analytics::peakConcurrentTracks(observations) == 1,
                "peak concurrency");
    requireTest(aquatrack::analytics::trackDurationHistogram({}, 4).empty(),
                "empty input yields no buckets");
    return 0;
}
