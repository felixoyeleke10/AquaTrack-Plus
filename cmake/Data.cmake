add_library(aquatrack-data
    src/data/BrackishMotSequence.cpp
    src/data/BrackishMotSequence.h
)
target_include_directories(aquatrack-data PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-data PUBLIC Qt6::Core Qt6::Gui)
