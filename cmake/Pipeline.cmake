add_library(aquatrack-pipeline
    src/pipeline/FrameAnalysisPipeline.cpp
    src/pipeline/FrameAnalysisPipeline.h
    src/pipeline/IFrameSource.h
    src/pipeline/BrackishFrameSource.cpp
    src/pipeline/BrackishFrameSource.h
    src/pipeline/BatchAnalysisRunner.cpp
    src/pipeline/BatchAnalysisRunner.h
)
target_include_directories(aquatrack-pipeline PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-pipeline PUBLIC aquatrack-domain aquatrack-data aquatrack-inference Qt6::Core Qt6::Gui)
