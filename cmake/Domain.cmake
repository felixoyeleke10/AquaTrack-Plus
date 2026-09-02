add_library(aquatrack-domain
    src/domain/DetectionTypes.h
    src/domain/IoUTracker.cpp
    src/domain/IoUTracker.h
    src/domain/IObservationRepository.h
    src/domain/ScientificTypes.cpp
    src/domain/ScientificTypes.h
    src/domain/AnnotationSession.cpp
    src/domain/AnnotationSession.h
    src/domain/ProjectManifest.cpp
    src/domain/ProjectManifest.h
    src/models/ModelManifest.cpp
    src/models/ModelManifest.h
)
target_include_directories(aquatrack-domain PUBLIC ${PROJECT_SOURCE_DIR}/src)
