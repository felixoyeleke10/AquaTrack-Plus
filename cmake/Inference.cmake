add_library(aquatrack-inference INTERFACE)
target_sources(aquatrack-inference INTERFACE src/inference/IDetector.h)
target_include_directories(aquatrack-inference INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-inference INTERFACE aquatrack-domain Qt6::Core Qt6::Gui)
