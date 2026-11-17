add_library(aquatrack-application
    src/application/AnalysisRunner.cpp
    src/application/AnalysisRunner.h
    src/application/AnnotationImporter.cpp
    src/application/AnnotationImporter.h
    src/application/AppState.cpp
    src/application/AppState.h
    src/application/DetectionMerge.cpp
    src/application/DetectionMerge.h
    src/application/MediaImporter.cpp
    src/application/MediaImporter.h
    src/application/ModelImporter.cpp
    src/application/ModelImporter.h
    src/application/ProjectLayout.cpp
    src/application/ProjectLayout.h
    src/application/ProjectSummary.cpp
    src/application/ProjectSummary.h
    src/application/ReviewSession.cpp
    src/application/ReviewSession.h
    src/application/SequenceEditing.cpp
    src/application/SequenceEditing.h
    src/application/controllers/ReviewController.cpp
    src/application/controllers/ReviewController.h
    src/application/controllers/ProjectController.cpp
    src/application/controllers/ProjectController.h
    src/application/controllers/ModelController.cpp
    src/application/controllers/ModelController.h
    src/application/controllers/SettingsController.cpp
    src/application/controllers/SettingsController.h
    src/application/controllers/DatasetController.cpp
    src/application/controllers/DatasetController.h
    src/application/controllers/AnnotationController.cpp
    src/application/controllers/AnnotationController.h
    src/application/controllers/ExportController.cpp
    src/application/controllers/ExportController.h
)
target_include_directories(aquatrack-application PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-application PUBLIC aquatrack-data aquatrack-export Qt6::Core Qt6::Gui)
# Lets the app find the checkout's .venv and Python package when it runs
# from a build folder outside it.
target_compile_definitions(aquatrack-application PRIVATE AQUATRACK_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
