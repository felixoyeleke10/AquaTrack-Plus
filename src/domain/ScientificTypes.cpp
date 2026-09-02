#include "domain/ScientificTypes.h"

#include <cmath>

namespace aquatrack::domain {

bool isValid(const Survey& survey)
{
    return !survey.id.empty() && !survey.name.empty() && survey.createdAtMs >= 0;
}

bool isValid(const VideoAsset& video)
{
    return !video.id.empty() && !video.surveyId.empty() && !video.path.empty()
        && video.durationMs >= 0 && video.width > 0 && video.height > 0
        && std::isfinite(video.frameRate) && video.frameRate > 0.0;
}

bool isValid(const Observation& observation)
{
    return !observation.id.empty() && !observation.videoId.empty()
        && observation.timestampMs >= 0 && observation.localTrackId > 0
        && observation.box.area() > 0.0F && !observation.className.empty()
        && std::isfinite(observation.confidence)
        && observation.confidence >= 0.0F && observation.confidence <= 1.0F;
}

}  // namespace aquatrack::domain
