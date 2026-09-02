add_library(aquatrack-application
    src/application/AppState.cpp
    src/application/AppState.h
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
