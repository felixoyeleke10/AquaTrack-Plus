#pragma once

#include <QString>

#include <optional>

namespace aquatrack::application {

// Where review of a project last stopped, so the dashboard can offer to
// resume there. Kept beside the manifest (<project>/.aquatrack/session.json)
// rather than in it: it changes every time someone reviews, and saving it
// must not mark the project as edited.
struct ReviewSession {
    QString mediaPath;  // absolute
    int frame = 0;      // 1-based
    qint64 savedAtMs = 0;
};

[[nodiscard]] std::optional<ReviewSession> loadReviewSession(const QString& projectFile);
bool saveReviewSession(const QString& projectFile, const ReviewSession& session);

}  // namespace aquatrack::application
