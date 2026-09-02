#include "analytics/SurveyAnalytics.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aquatrack::analytics {

SurveySummary summarize(std::span<const domain::Observation> observations)
{
    SurveySummary result;
    if (observations.empty())
        return result;
    std::set<std::pair<domain::VideoId, domain::LocalTrackId>> localTracks;
    std::set<domain::GlobalTrackId> globalTracks;
    double confidenceSum = 0.0;
    result.firstObservationMs = observations.front().timestampMs;
    result.lastObservationMs = observations.front().timestampMs;
    for (const auto& observation : observations) {
        ++result.totalObservations;
        localTracks.emplace(observation.videoId, observation.localTrackId);
        if (observation.globalTrackId)
            globalTracks.insert(*observation.globalTrackId);
        confidenceSum += observation.confidence;
        ++result.observationsByClass[observation.className];
        result.firstObservationMs = std::min(result.firstObservationMs, observation.timestampMs);
        result.lastObservationMs = std::max(result.lastObservationMs, observation.timestampMs);
    }
    result.uniqueLocalTracks = localTracks.size();
    result.uniqueGlobalTracks = globalTracks.size();
    result.meanConfidence = confidenceSum / static_cast<double>(observations.size());
    return result;
}

std::vector<TimeBin> binObservations(std::span<const domain::Observation> observations,
                                     domain::TimestampMs binWidthMs, std::size_t maxBins)
{
    if (binWidthMs <= 0)
        throw std::invalid_argument("bin width must be positive");
    if (maxBins == 0)
        throw std::invalid_argument("bin cap must be positive");
    if (observations.empty())
        return {};
    const auto first = std::min_element(observations.begin(), observations.end(),
        [](const auto& left, const auto& right) { return left.timestampMs < right.timestampMs; })->timestampMs;
    const auto last = std::max_element(observations.begin(), observations.end(),
        [](const auto& left, const auto& right) { return left.timestampMs < right.timestampMs; })->timestampMs;
    // Widen the bin rather than allocating a vector proportional to the span:
    // a caller asking for 1 ms bins over an hour would otherwise request
    // millions of bins and their per-bin track sets.
    const auto span = last - first;
    if (span / binWidthMs + 1 > static_cast<domain::TimestampMs>(maxBins))
        binWidthMs = span / static_cast<domain::TimestampMs>(maxBins) + 1;
    const auto count = static_cast<std::size_t>(span / binWidthMs) + 1;
    std::vector<TimeBin> bins(count);
    std::vector<std::set<std::pair<domain::VideoId, domain::LocalTrackId>>> tracks(count);
    for (std::size_t index = 0; index < count; ++index) {
        bins[index].startMs = first + static_cast<domain::TimestampMs>(index) * binWidthMs;
        bins[index].endMs = bins[index].startMs + binWidthMs;
    }
    for (const auto& observation : observations) {
        const auto index = static_cast<std::size_t>((observation.timestampMs - first) / binWidthMs);
        ++bins[index].observations;
        tracks[index].emplace(observation.videoId, observation.localTrackId);
    }
    for (std::size_t index = 0; index < count; ++index)
        bins[index].uniqueTracks = tracks[index].size();
    return bins;
}

std::vector<SpeciesStats> speciesBreakdown(std::span<const domain::Observation> observations)
{
    struct Accumulator {
        std::size_t observations = 0;
        double confidenceSum = 0.0;
        domain::TimestampMs firstMs = 0;
        domain::TimestampMs lastMs = 0;
        std::set<std::pair<domain::VideoId, domain::LocalTrackId>> tracks;
    };
    std::map<std::string, Accumulator> byClass;

    for (const auto& observation : observations) {
        auto& entry = byClass[observation.className];
        if (entry.observations == 0) {
            entry.firstMs = observation.timestampMs;
            entry.lastMs = observation.timestampMs;
        }
        ++entry.observations;
        entry.confidenceSum += observation.confidence;
        entry.firstMs = std::min(entry.firstMs, observation.timestampMs);
        entry.lastMs = std::max(entry.lastMs, observation.timestampMs);
        entry.tracks.emplace(observation.videoId, observation.localTrackId);
    }

    std::vector<SpeciesStats> result;
    result.reserve(byClass.size());
    for (const auto& [className, entry] : byClass) {
        SpeciesStats stats;
        stats.className = className;
        stats.observations = entry.observations;
        stats.uniqueTracks = entry.tracks.size();
        stats.meanConfidence =
            entry.observations == 0 ? 0.0 : entry.confidenceSum / double(entry.observations);
        stats.firstSeenMs = entry.firstMs;
        stats.lastSeenMs = entry.lastMs;
        result.push_back(std::move(stats));
    }
    std::sort(result.begin(), result.end(),
              [](const SpeciesStats& left, const SpeciesStats& right) {
                  if (left.observations != right.observations)
                      return left.observations > right.observations;
                  return left.className < right.className;
              });
    return result;
}

std::vector<DurationBucket> trackDurationHistogram(std::span<const domain::Observation> observations,
                                                   std::size_t buckets)
{
    if (buckets == 0)
        throw std::invalid_argument("bucket count must be positive");
    const auto spans = trackSpans(observations);
    if (spans.empty())
        return {};

    domain::TimestampMs longest = 0;
    for (const auto& span : spans)
        longest = std::max(longest, span.durationMs());
    // Every track being instantaneous is a real answer, not a divide by zero.
    const auto width = std::max<domain::TimestampMs>(1, longest / domain::TimestampMs(buckets) + 1);

    std::vector<DurationBucket> result(buckets);
    for (std::size_t index = 0; index < buckets; ++index) {
        result[index].lowerMs = domain::TimestampMs(index) * width;
        result[index].upperMs = result[index].lowerMs + width;
    }
    for (const auto& span : spans) {
        const auto index =
            std::min(buckets - 1, static_cast<std::size_t>(span.durationMs() / width));
        ++result[index].tracks;
    }
    return result;
}

std::vector<TrackSpan> trackSpans(std::span<const domain::Observation> observations)
{
    std::map<std::pair<domain::VideoId, domain::LocalTrackId>, TrackSpan> spans;
    for (const auto& observation : observations) {
        const auto key = std::make_pair(observation.videoId, observation.localTrackId);
        auto found = spans.find(key);
        if (found == spans.end()) {
            TrackSpan span;
            span.videoId = observation.videoId;
            span.trackId = observation.localTrackId;
            span.firstFrame = span.lastFrame = observation.frameIndex;
            span.firstMs = span.lastMs = observation.timestampMs;
            span.observations = 1;
            span.className = observation.className;
            spans.emplace(key, std::move(span));
            continue;
        }
        auto& span = found->second;
        span.firstFrame = std::min(span.firstFrame, observation.frameIndex);
        span.lastFrame = std::max(span.lastFrame, observation.frameIndex);
        span.firstMs = std::min(span.firstMs, observation.timestampMs);
        span.lastMs = std::max(span.lastMs, observation.timestampMs);
        ++span.observations;
    }
    std::vector<TrackSpan> result;
    result.reserve(spans.size());
    for (auto& [key, span] : spans)
        result.push_back(std::move(span));
    std::sort(result.begin(), result.end(), [](const TrackSpan& left, const TrackSpan& right) {
        if (left.durationMs() != right.durationMs())
            return left.durationMs() > right.durationMs();
        return left.trackId < right.trackId;
    });
    return result;
}

std::size_t peakConcurrentTracks(std::span<const domain::Observation> observations)
{
    std::map<std::pair<domain::VideoId, std::int64_t>, std::set<domain::LocalTrackId>> perFrame;
    for (const auto& observation : observations)
        perFrame[{observation.videoId, observation.frameIndex}].insert(observation.localTrackId);
    std::size_t peak = 0;
    for (const auto& [key, tracks] : perFrame)
        peak = std::max(peak, tracks.size());
    return peak;
}

std::vector<IdSwitchCandidate> idSwitchCandidates(std::span<const domain::Observation> observations,
                                                  domain::TimestampMs maxGapMs)
{
    if (maxGapMs <= 0)
        return {};
    const auto spans = trackSpans(observations);

    // Last and first box of every track, to compare geometry across the gap.
    std::map<std::pair<domain::VideoId, domain::LocalTrackId>, BoundingBox> lastBox;
    std::map<std::pair<domain::VideoId, domain::LocalTrackId>, BoundingBox> firstBox;
    std::map<std::pair<domain::VideoId, domain::LocalTrackId>, std::int64_t> lastFrame;
    std::map<std::pair<domain::VideoId, domain::LocalTrackId>, std::int64_t> firstFrame;
    for (const auto& observation : observations) {
        const auto key = std::make_pair(observation.videoId, observation.localTrackId);
        auto lastAt = lastFrame.find(key);
        if (lastAt == lastFrame.end() || observation.frameIndex >= lastAt->second) {
            lastFrame[key] = observation.frameIndex;
            lastBox[key] = observation.box;
        }
        auto firstAt = firstFrame.find(key);
        if (firstAt == firstFrame.end() || observation.frameIndex <= firstAt->second) {
            firstFrame[key] = observation.frameIndex;
            firstBox[key] = observation.box;
        }
    }

    std::vector<IdSwitchCandidate> candidates;
    for (const auto& ending : spans) {
        for (const auto& starting : spans) {
            if (ending.videoId != starting.videoId || ending.trackId == starting.trackId)
                continue;
            const auto gap = starting.firstMs - ending.lastMs;
            if (gap < 0 || gap > maxGapMs)
                continue;

            const auto& before = lastBox[{ending.videoId, ending.trackId}];
            const auto& after = firstBox[{starting.videoId, starting.trackId}];
            if (before.area() <= 0.0F || after.area() <= 0.0F)
                continue;

            // Similarity blends centre proximity and size agreement, both
            // normalised by the boxes themselves so it is resolution free.
            const float scale = std::sqrt(std::max(before.area(), after.area()));
            const float dx = before.centerX() - after.centerX();
            const float dy = before.centerY() - after.centerY();
            const float distance = std::sqrt(dx * dx + dy * dy) / std::max(scale, 1.0F);
            const float sizeRatio = std::min(before.area(), after.area())
                / std::max(before.area(), after.area());
            const double similarity =
                std::clamp(double(sizeRatio) * std::exp(-double(distance)), 0.0, 1.0);
            if (similarity < 0.5)
                continue;

            candidates.push_back({ending.videoId, ending.trackId, starting.trackId,
                                  starting.firstMs, gap, similarity});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const IdSwitchCandidate& left, const IdSwitchCandidate& right) {
                  return left.similarity > right.similarity;
              });
    return candidates;
}

std::vector<SurveyEvent> notableEvents(std::span<const domain::Observation> observations,
                                       domain::TimestampMs binWidthMs)
{
    std::vector<SurveyEvent> events;
    if (observations.empty() || binWidthMs <= 0)
        return events;

    const auto bins = binObservations(observations, binWidthMs);
    if (bins.empty())
        return events;

    // Sustained activity: bins whose unique-track count is well above the mean.
    double meanTracks = 0.0;
    for (const auto& bin : bins)
        meanTracks += double(bin.uniqueTracks);
    meanTracks /= double(bins.size());
    const auto busiest = std::max_element(
        bins.cbegin(), bins.cend(),
        [](const TimeBin& left, const TimeBin& right) { return left.uniqueTracks < right.uniqueTracks; });
    if (busiest != bins.cend() && double(busiest->uniqueTracks) > meanTracks * 1.5
        && busiest->uniqueTracks > 1) {
        events.push_back({busiest->startMs, busiest->endMs,
                          "High activity period (" + std::to_string(busiest->uniqueTracks)
                              + " tracks)",
                          EventSeverity::Info});
    }

    const auto peak = peakConcurrentTracks(observations);
    if (peak >= 5) {
        events.push_back({0, 0, "Crowded frames detected (peak " + std::to_string(peak)
                                    + " simultaneous)",
                          EventSeverity::Info});
    }

    for (const auto& candidate : idSwitchCandidates(observations)) {
        events.push_back({candidate.atMs, candidate.atMs,
                          "Possible identity switch: track "
                              + std::to_string(candidate.endingTrack) + " to "
                              + std::to_string(candidate.startingTrack),
                          EventSeverity::Alert});
        if (events.size() > 12)
            break;
    }

    // Short tracks are usually detector noise or a missed re-association.
    std::size_t fleeting = 0;
    for (const auto& span : trackSpans(observations))
        if (span.observations <= 2)
            ++fleeting;
    if (fleeting > 0) {
        events.push_back({0, 0,
                          std::to_string(fleeting) + " very short track(s) worth review",
                          EventSeverity::Warning});
    }

    std::sort(events.begin(), events.end(),
              [](const SurveyEvent& left, const SurveyEvent& right) { return left.atMs < right.atMs; });
    return events;
}

}  // namespace aquatrack::analytics
