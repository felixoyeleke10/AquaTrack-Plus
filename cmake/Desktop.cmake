qt_add_executable(AquaTrack
    apps/desktop/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/AppTheme.cpp
    src/ui/AppTheme.h
    src/ui/navigation/AppHeader.cpp
    src/ui/navigation/AppHeader.h
    src/ui/review/left/SurveyPanel.cpp
    src/ui/review/left/SurveyPanel.h
    src/ui/review/center/ReviewPanel.cpp
    src/ui/review/center/ReviewPanel.h
    src/ui/review/center/analytics/AnalysisPanel.cpp
    src/ui/review/center/analytics/AnalysisPanel.h
    src/ui/review/center/video/SequenceView.cpp
    src/ui/review/center/video/SequenceView.h
    src/ui/review/right/TrackPanel.cpp
    src/ui/review/right/TrackPanel.h
)
target_link_libraries(AquaTrack PRIVATE aquatrack-application aquatrack-domain aquatrack-data aquatrack-inference Qt6::Widgets)
set_target_properties(AquaTrack PROPERTIES WIN32_EXECUTABLE TRUE)
