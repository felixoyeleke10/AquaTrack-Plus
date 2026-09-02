#pragma once

#include "domain/DetectionTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aquatrack::domain {

using TimestampMs = std::int64_t;
using SurveyId = std::string;
using VideoId = std::string;
using ObservationId = std::string;
using LocalTrackId = std::int64_t;
using GlobalTrackId = std::int64_t;

struct Survey {
    SurveyId id;
    std::string name;
    TimestampMs createdAtMs = 0;
    std::optional<std::string> location;
    std::vector<VideoId> videos;
};

struct VideoAsset {
    VideoId id;
    SurveyId surveyId;
    std::string path;
    TimestampMs startTimeMs = 0;
    std::int64_t durationMs = 0;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
};

struct Observation {
    ObservationId id;
    VideoId videoId;
    TimestampMs timestampMs = 0;
    std::int64_t frameIndex = 0;
    LocalTrackId localTrackId = 0;
    std::optional<GlobalTrackId> globalTrackId;
    BoundingBox box;
    std::string className;
    float confidence = 0.0F;
    bool humanVerified = false;
};

struct EnvironmentSample {
    TimestampMs timestampMs = 0;
    std::optional<double> temperatureC;
    std::optional<double> salinityPsu;
    std::optional<double> depthM;
    std::optional<double> oxygenMgL;
    std::optional<double> turbidityNtu;
};

[[nodiscard]] bool isValid(const Survey& survey);
[[nodiscard]] bool isValid(const VideoAsset& video);
[[nodiscard]] bool isValid(const Observation& observation);

}  // namespace aquatrack::domain
