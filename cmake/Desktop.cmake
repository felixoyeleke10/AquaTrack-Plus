add_library(aquatrack-ui
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/AppTheme.cpp
    src/ui/AppTheme.h
    src/ui/common/Charts.cpp
    src/ui/common/Charts.h
    src/ui/common/Glyphs.cpp
    src/ui/common/Glyphs.h
    src/ui/common/Widgets.cpp
    src/ui/common/Widgets.h
    src/ui/dashboard/DashboardPage.cpp
    src/ui/dashboard/DashboardPage.h
    src/ui/analytics/AnalyticsPage.cpp
    src/ui/analytics/AnalyticsPage.h
    src/ui/datasets/DatasetsPage.cpp
    src/ui/datasets/DatasetsPage.h
    src/ui/navigation/AppHeader.cpp
    src/ui/navigation/AppHeader.h
    src/ui/review/ReviewPage.cpp
    src/ui/review/ReviewPage.h
    src/ui/review/RuntimeStrip.cpp
    src/ui/review/RuntimeStrip.h
    src/ui/review/center/Timeline.cpp
    src/ui/review/center/Timeline.h
    src/ui/review/left/SurveyPanel.cpp
    src/ui/review/left/SurveyPanel.h
    src/ui/review/center/ReviewPanel.cpp
    src/ui/review/center/ReviewPanel.h
    src/ui/review/center/video/SequenceView.cpp
    src/ui/review/center/video/SequenceView.h
    src/ui/review/right/InsightPanel.cpp
    src/ui/review/right/InsightPanel.h
    src/ui/videos/VideosPage.cpp
    src/ui/videos/VideosPage.h
    src/ui/models/ModelsPage.cpp
    src/ui/models/ModelsPage.h
    src/ui/settings/SettingsPage.cpp
    src/ui/settings/SettingsPage.h
)
target_include_directories(aquatrack-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-ui PUBLIC aquatrack-application aquatrack-domain aquatrack-data aquatrack-inference aquatrack-analytics Qt6::Widgets)

qt_add_executable(AquaTrack apps/desktop/main.cpp)
target_link_libraries(AquaTrack PRIVATE aquatrack-ui)
set_target_properties(AquaTrack PROPERTIES WIN32_EXECUTABLE TRUE)
