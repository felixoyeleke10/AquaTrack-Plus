#include "domain/ScientificTypes.h"
#include "TestSupport.h"

int main()
{
    using namespace aquatrack::domain;
    Survey survey{.id = "survey-1", .name = "Holyrood Bay", .createdAtMs = 1};
    requireTest(isValid(survey), "valid survey");
    survey.name.clear();
    requireTest(!isValid(survey), "survey requires name");

    VideoAsset video{.id = "video-1", .surveyId = "survey-1", .path = "station.mp4",
                     .durationMs = 1000, .width = 1920, .height = 1080, .frameRate = 29.97};
    requireTest(isValid(video), "valid video");

    Observation observation{.id = "obs-1", .videoId = "video-1", .timestampMs = 10,
                            .localTrackId = 7, .box = {0, 0, 20, 10},
                            .className = "fish", .confidence = 0.9F};
    requireTest(isValid(observation), "valid observation");
    observation.confidence = 1.1F;
    requireTest(!isValid(observation), "confidence range");
    return 0;
}
