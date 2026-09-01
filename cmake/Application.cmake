add_library(aquatrack-application
    src/application/controllers/ReviewController.cpp
    src/application/controllers/ReviewController.h
)
target_include_directories(aquatrack-application PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-application PUBLIC aquatrack-data Qt6::Core Qt6::Gui)
