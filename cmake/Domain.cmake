add_library(aquatrack-domain
    src/domain/DetectionTypes.h
    src/domain/IoUTracker.cpp
    src/domain/IoUTracker.h
)
target_include_directories(aquatrack-domain PUBLIC ${PROJECT_SOURCE_DIR}/src)
