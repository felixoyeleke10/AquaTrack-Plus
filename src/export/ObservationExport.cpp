#include "export/ObservationExport.h"
#include <iomanip>
#include <sstream>

namespace {
std::string csvField(const std::string &value) {
  if (value.find_first_of(",\"\r\n") == std::string::npos)
    return value;
  std::string escaped = "\"";
  for (const char character : value) {
    escaped += character;
    if (character == '"')
      escaped += '"';
  }
  return escaped + '"';
}
} // namespace

namespace aquatrack::exporting {
std::string
observationsToCsv(std::span<const domain::Observation> observations) {
  std::ostringstream output;
  output << "observation_id,video_id,timestamp_ms,frame_index,local_track_id,"
            "global_track_id,class,confidence,x1,y1,x2,y2,human_verified\n";
  output << std::fixed << std::setprecision(6);
  for (const auto &item : observations) {
    output << csvField(item.id) << ',' << csvField(item.videoId) << ','
           << item.timestampMs << ',' << item.frameIndex << ','
           << item.localTrackId << ',';
    if (item.globalTrackId)
      output << *item.globalTrackId;
    output << ',' << csvField(item.className) << ',' << item.confidence << ','
           << item.box.x1 << ',' << item.box.y1 << ',' << item.box.x2 << ','
           << item.box.y2 << ',' << (item.humanVerified ? "true" : "false")
           << '\n';
  }
  return output.str();
}

std::string
observationsToMotChallenge(std::span<const domain::Observation> observations) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(3);
  for (const auto &item : observations)
    output << item.frameIndex + 1 << ',' << item.localTrackId << ','
           << item.box.x1 << ',' << item.box.y1 << ',' << item.box.width()
           << ',' << item.box.height() << ',' << item.confidence
           << ",-1,-1,-1\n";
  return output.str();
}
} // namespace aquatrack::exporting
