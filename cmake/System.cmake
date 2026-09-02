add_library(aquatrack-system
    src/system/PipelineManager.cpp
    src/system/PipelineManager.h
    src/system/logging/StructuredLogger.cpp
    src/system/logging/StructuredLogger.h
    src/system/settings/AquaTrackSettings.cpp
    src/system/settings/AquaTrackSettings.h
)
target_include_directories(aquatrack-system PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-system PUBLIC Qt6::Core)
