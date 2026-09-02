add_library(aquatrack-data
    src/data/BrackishMotSequence.cpp
    src/data/BrackishMotSequence.h
    src/data/ProjectFileStore.cpp
    src/data/ProjectFileStore.h
    src/data/ModelRegistry.cpp
    src/data/ModelRegistry.h
    src/data/SettingsStore.cpp
    src/data/SettingsStore.h
    src/data/catalog/DatasetCatalog.cpp
    src/data/catalog/DatasetCatalog.h
    src/data/observations/JsonObservationRepository.cpp
    src/data/observations/JsonObservationRepository.h
)
target_include_directories(aquatrack-data PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-data PUBLIC aquatrack-domain aquatrack-system Qt6::Core Qt6::Gui)
