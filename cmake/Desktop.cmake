add_library(aquatrack-ui
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/AppTheme.cpp
    src/ui/AppTheme.h
    src/ui/AppStyle.cpp
    src/ui/AppStyle.h
    src/ui/common/Charts.cpp
    src/ui/common/Charts.h
    src/ui/common/GlyphArtwork.cpp
    src/ui/common/GlyphArtwork.h
    src/ui/common/HoverHint.cpp
    src/ui/common/HoverHint.h
    src/ui/common/InspectorTabs.cpp
    src/ui/common/InspectorTabs.h
    src/ui/common/WindowChrome.cpp
    src/ui/common/WindowChrome.h
    src/ui/common/Glyphs.cpp
    src/ui/common/Glyphs.h
    src/ui/common/Widgets.cpp
    src/ui/common/Widgets.h
    src/ui/dashboard/DashboardPage.cpp
    src/ui/dashboard/DashboardPage.h
    src/ui/dashboard/DashboardCards.cpp
    src/ui/dashboard/DashboardCards.h
    src/ui/videos/SequenceDialog.cpp
    src/ui/videos/SequenceDialog.h
    src/ui/analytics/AnalyticsPage.cpp
    src/ui/analytics/AnalyticsPage.h
    src/ui/datasets/DatasetsPage.cpp
    src/ui/datasets/DatasetsPage.h
    src/ui/datasets/DatasetInspector.cpp
    src/ui/datasets/DatasetInspector.h
    src/ui/export/ExportPage.cpp
    src/ui/export/ExportPage.h
    src/ui/navigation/AppHeader.cpp
    src/ui/navigation/AppHeader.h
    src/ui/navigation/NavigationDock.cpp
    src/ui/navigation/NavigationDock.h
    src/ui/review/ReviewPage.cpp
    src/ui/review/ReviewPage.h
    src/ui/review/center/Timeline.cpp
    src/ui/review/center/Timeline.h
    src/ui/review/left/SurveyPanel.cpp
    src/ui/review/left/SurveyPanel.h
    src/ui/review/center/ActivityPanel.cpp
    src/ui/review/center/LogView.cpp
    src/ui/review/center/LogView.h
    src/ui/review/center/ActivityPanel.h
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
    src/ui/models/ModelImportDialog.cpp
    src/ui/models/ModelImportDialog.h
    src/ui/settings/SettingsPage.cpp
    src/ui/settings/SettingsPage.h
)
target_include_directories(aquatrack-ui PUBLIC ${PROJECT_SOURCE_DIR}/src)
if(WIN32)
    # Rounded corners and border colour for the frameless main window.
    target_link_libraries(aquatrack-ui PRIVATE dwmapi)
endif()
target_link_libraries(aquatrack-ui PUBLIC aquatrack-application aquatrack-domain aquatrack-data aquatrack-inference aquatrack-analytics Qt6::Svg Qt6::Widgets)

qt_add_executable(AquaTrack apps/desktop/main.cpp)
target_link_libraries(AquaTrack PRIVATE aquatrack-ui)
set_target_properties(AquaTrack PROPERTIES WIN32_EXECUTABLE TRUE)

# Video review decodes with FFmpeg. When ffmpeg and ffprobe can be found at
# build time they are copied next to AquaTrack.exe, so the build also plays
# video on machines without FFmpeg installed. The app looks there first.
set(_aquatrack_ffmpeg_hints "$ENV{AQUATRACK_FFMPEG_DIR}")
if(WIN32)
    file(GLOB _aquatrack_winget_ffmpeg
         "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/*FFmpeg*/*/bin")
    list(APPEND _aquatrack_ffmpeg_hints ${_aquatrack_winget_ffmpeg})
endif()
find_program(AQUATRACK_FFMPEG ffmpeg HINTS ${_aquatrack_ffmpeg_hints})
find_program(AQUATRACK_FFPROBE ffprobe HINTS ${_aquatrack_ffmpeg_hints})
if(AQUATRACK_FFMPEG AND AQUATRACK_FFPROBE)
    add_custom_command(TARGET AquaTrack POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AQUATRACK_FFMPEG}" "${AQUATRACK_FFPROBE}" "$<TARGET_FILE_DIR:AquaTrack>"
        COMMENT "Bundling FFmpeg next to AquaTrack for video review")
    message(STATUS "FFmpeg for video review: ${AQUATRACK_FFMPEG}")
else()
    message(WARNING "FFmpeg was not found: video files will only open if ffmpeg and ffprobe "
                    "are on PATH when AquaTrack runs.")
endif()
