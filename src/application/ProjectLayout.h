#pragma once

#include <QString>

namespace aquatrack::application {

// Where everything belonging to a project lives. A project is a folder:
//
//   <Name>/
//     <Name>.aqp      the manifest
//     README.txt      what each folder is for
//     media/
//       videos/       video files copied into the project
//       sequences/    one folder per sequence (sequence.json + README.txt,
//                     or a BrackishMOT seqinfo.ini/img1/gt)
//     annotations/    reviewed edits (observations.json) and MOT gt.txt
//     models/         detector weights and model.json manifests
//     exports/        CSV and other exports
//     snapshots/      frame snapshots from Review
//
// Built from the .aqp path, so projects created before this layout existed
// still resolve: their folders are created when first needed.
class ProjectLayout final {
public:
    explicit ProjectLayout(const QString& projectFile);

    [[nodiscard]] QString root() const { return m_root; }
    [[nodiscard]] QString media() const;
    [[nodiscard]] QString videos() const;
    [[nodiscard]] QString sequences() const;
    // Where a copy of `source` belongs: videos/ for a file, sequences/ for
    // a sequence folder.
    [[nodiscard]] QString mediaFolderFor(const QString& source) const;
    [[nodiscard]] QString annotations() const;
    [[nodiscard]] QString models() const;
    [[nodiscard]] QString exports() const;
    [[nodiscard]] QString snapshots() const;

    // A project name made safe as a Windows folder and file name: reserved
    // characters become '_', trailing dots and spaces go.
    [[nodiscard]] static QString folderNameFor(const QString& projectName);

    // Store path recorded in new manifests, relative to the project folder.
    static constexpr const char* ObservationStore = "annotations/observations.json";

    // Creates every folder and the README. Existing content is left alone.
    bool create(QString* error = nullptr) const;
    // Creates one folder on demand, returning its path (empty on failure).
    [[nodiscard]] QString ensure(const QString& folder) const;
    // True when `path` is inside the project folder.
    [[nodiscard]] bool contains(const QString& path) const;

private:
    QString m_root;
};

}  // namespace aquatrack::application
