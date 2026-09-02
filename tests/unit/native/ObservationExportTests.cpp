#include "TestSupport.h"
#include "export/ObservationExport.h"
#include <vector>
int main() {
  std::vector<aquatrack::domain::Observation> observations{
      {.id = "obs,1",
       .videoId = "video-1",
       .timestampMs = 250,
       .frameIndex = 4,
       .localTrackId = 9,
       .globalTrackId = 101,
       .box = {10, 20, 40, 50},
       .className = "unknown \"fish\"",
       .confidence = 0.75F,
       .humanVerified = true}};
  const auto csv = aquatrack::exporting::observationsToCsv(observations);
  requireTest(csv.find("\"obs,1\"") != std::string::npos, "CSV comma escaping");
  requireTest(csv.find("\"unknown \"\"fish\"\"\"") != std::string::npos,
              "CSV quote escaping");
  requireTest(csv.find(",101,") != std::string::npos, "global id exported");
  const auto mot =
      aquatrack::exporting::observationsToMotChallenge(observations);
  requireTest(mot.rfind("5,9,10.000,20.000,30.000,30.000,0.750", 0) == 0,
              "MOT export");
  return 0;
}
