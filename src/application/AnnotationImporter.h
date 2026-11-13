#pragma once

#include "data/media/MediaSource.h"
#include "domain/ScientificTypes.h"

#include <QString>

#include <vector>

namespace aquatrack::application {

// Ground-truth tracks for one media item, read from a file.
struct AnnotationImport {
    std::vector<domain::Observation> observations;
    QString format;           // "MOTChallenge" or "COCO tracking JSON"
    int tracks = 0;
    int frames = 0;           // distinct frames with at least one box
    int skipped = 0;          // rows that could not form a valid box
    int outOfRange = 0;       // rows for frames the media does not have
};

// Reads tracking annotations and expresses them as observations of the media
// open in Review (its name is the video id, its clock gives timestamps).
//
//  - MOTChallenge text (gt.txt, det.txt): frame,id,x,y,w,h[,conf,class,vis].
//    Frames are 1-based. In gt.txt a confidence of 0 marks an ignore region
//    and is skipped. Class ids 1-6 follow BrackishMOT; others become "Fish".
//  - COCO-style tracking JSON (images / annotations / categories, with a
//    track_id or instance_id per annotation), as SFISHTRACK and TAO ship.
//    frame_id is used when present (0-based ids are shifted to 1-based),
//    otherwise the number in the image file name. A dataset's single generic
//    category ("object") is named "Fish".
class AnnotationImporter final {
public:
    [[nodiscard]] static bool read(const QString& path, const media::MediaSource& source,
                                   AnnotationImport& result, QString* error = nullptr);
    // File-dialog filter for the formats above.
    [[nodiscard]] static QString fileDialogFilter();
};

}  // namespace aquatrack::application
