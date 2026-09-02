#pragma once

#include "domain/ScientificTypes.h"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace aquatrack::analytics {

struct TimeBin {
    domain::TimestampMs startMs = 0;
    domain::TimestampMs endMs = 0;
    std::size_t observations = 0;
    std::size_t uniqueTracks = 0;
};

struct SurveySummary {
    std::size_t totalObservations = 0;
    std::size_t uniqueLocalTracks = 0;
    std::size_t uniqueGlobalTracks = 0;
    double meanConfidence = 0.0;
    domain::TimestampMs firstObservationMs = 0;
    domain::TimestampMs lastObservationMs = 0;
    std::map<std::string, std::size_t> observationsByClass;
};

// The lifetime of one local track within a video.
struct TrackSpan {
    domain::VideoId videoId;
    domain::LocalTrackId trackId = 0;
    std::int64_t firstFrame = 0;
    std::int64_t lastFrame = 0;
    domain::TimestampMs firstMs = 0;
    domain::TimestampMs lastMs = 0;
    std::size_t observations = 0;
    std::string className;

    [[nodiscard]] domain::TimestampMs durationMs() const { return lastMs - firstMs; }
};

// A track that ends just as another begins nearby is a likely identity switch:
// the same animal continuing under a new id. Reported as a candidate for a
// reviewer to confirm, never applied automatically.
struct IdSwitchCandidate {
    domain::VideoId videoId;
    domain::LocalTrackId endingTrack = 0;
    domain::LocalTrackId startingTrack = 0;
    domain::TimestampMs atMs = 0;
    domain::TimestampMs gapMs = 0;
    double similarity = 0.0;  // 0..1, from box geometry across the gap
};

enum class EventSeverity { Info, Warning, Alert };

struct SurveyEvent {
    domain::TimestampMs atMs = 0;
    domain::TimestampMs untilMs = 0;
    std::string description;
    EventSeverity severity = EventSeverity::Info;
};

// Per-species rollup. `summarize` only counts observations per class, which
// cannot answer "how many individuals" or "how confident were we".
struct SpeciesStats {
    std::string className;
    std::size_t observations = 0;
    std::size_t uniqueTracks = 0;
    double meanConfidence = 0.0;
    domain::TimestampMs firstSeenMs = 0;
    domain::TimestampMs lastSeenMs = 0;
};

// One bucket of the track-duration distribution.
struct DurationBucket {
    domain::TimestampMs lowerMs = 0;
    domain::TimestampMs upperMs = 0;
    std::size_t tracks = 0;
};

[[nodiscard]] SurveySummary summarize(std::span<const domain::Observation> observations);

// Ordered by observation count, descending.
[[nodiscard]] std::vector<SpeciesStats> speciesBreakdown(
    std::span<const domain::Observation> observations);

// Distribution of how long tracks persist, which is how a reviewer tells a
// solid detection record from a stream of one-frame noise.
[[nodiscard]] std::vector<DurationBucket> trackDurationHistogram(
    std::span<const domain::Observation> observations, std::size_t buckets = 6);

// Bins are capped at `maxBins`; a bin width that would exceed it is widened
// so a caller cannot request an allocation proportional to the time span.
[[nodiscard]] std::vector<TimeBin> binObservations(
    std::span<const domain::Observation> observations, domain::TimestampMs binWidthMs,
    std::size_t maxBins = 4096);

[[nodiscard]] std::vector<TrackSpan> trackSpans(std::span<const domain::Observation> observations);

// Highest number of distinct tracks visible in any single frame.
[[nodiscard]] std::size_t peakConcurrentTracks(
    std::span<const domain::Observation> observations);

[[nodiscard]] std::vector<IdSwitchCandidate> idSwitchCandidates(
    std::span<const domain::Observation> observations, domain::TimestampMs maxGapMs = 2000);

// Derived, human-readable moments: activity peaks, crowding, likely identity
// switches, and low-confidence stretches.
[[nodiscard]] std::vector<SurveyEvent> notableEvents(
    std::span<const domain::Observation> observations, domain::TimestampMs binWidthMs);

}  // namespace aquatrack::analytics
