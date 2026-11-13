#pragma once

#include <QString>
#include <QStringList>

namespace aquatrack::media {

// What a path in a project's media list is, judged from the filesystem alone
// (no decoding). Cheap enough to call while building a list.
enum class MediaKind {
    // Both sequence kinds are shown to people simply as "Sequence"; they
    // differ only in how the frames are stored.
    ImageSequence,  // frames stored as images: seqinfo.ini + img1/ (BrackishMOT)
    VideoFile,      // a single file with a video extension
    VideoSequence,  // frames stored as video clips: sequence.json listing them
    Folder,         // a folder that is neither kind of sequence
    Unsupported,    // a file that is not a video
    Missing         // nothing at this path
};

[[nodiscard]] MediaKind classifyMedia(const QString& path);
[[nodiscard]] bool isReviewable(MediaKind kind);
[[nodiscard]] QString kindLabel(MediaKind kind);

// Lower-case extensions without the dot: mp4, mov, mkv...
[[nodiscard]] const QStringList& videoExtensions();
[[nodiscard]] bool hasVideoExtension(const QString& path);
// "Video files (*.mp4 *.mov ...)" for file dialogs.
[[nodiscard]] QString videoFileDialogFilter();

// File name of the manifest that marks a folder as a video sequence.
inline constexpr const char* VideoSequenceManifestName = "sequence.json";

// Absolute path to ffmpeg or ffprobe, or empty when it cannot be found.
// Looks, in order, at $AQUATRACK_FFMPEG_DIR, the application folder (where
// the build copies it), PATH, and on Windows the WinGet package folder, which
// is not on PATH for processes started before the user signed in again.
[[nodiscard]] QString ffmpegTool(const QString& name);
// A sentence a reviewer can act on when ffmpegTool() comes back empty.
[[nodiscard]] QString ffmpegMissingMessage();

}  // namespace aquatrack::media
