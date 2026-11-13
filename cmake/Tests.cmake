enable_testing()
add_executable(aquatrack-tracker-tests tests/unit/native/TrackerTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-tracker-tests PRIVATE aquatrack-domain)
add_test(NAME AquaTrackTracker COMMAND aquatrack-tracker-tests)

add_executable(aquatrack-frame-analysis-tests
    tests/unit/native/FrameAnalysisPipelineTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-frame-analysis-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-frame-analysis-tests PRIVATE aquatrack-pipeline)
add_test(NAME AquaTrackFrameAnalysis COMMAND aquatrack-frame-analysis-tests)

add_executable(aquatrack-data-tests tests/unit/native/BrackishMotSequenceTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-data-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackData COMMAND aquatrack-data-tests)

add_executable(aquatrack-dataset-catalog-tests
    tests/unit/native/DatasetCatalogTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-dataset-catalog-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-dataset-catalog-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackDatasetCatalog COMMAND aquatrack-dataset-catalog-tests)

add_executable(aquatrack-observation-repository-tests
    tests/unit/native/JsonObservationRepositoryTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-observation-repository-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-observation-repository-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackObservationRepository COMMAND aquatrack-observation-repository-tests)

add_executable(aquatrack-scientific-types-tests
    tests/unit/native/ScientificTypesTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-scientific-types-tests PRIVATE aquatrack-domain)
add_test(NAME AquaTrackScientificTypes COMMAND aquatrack-scientific-types-tests)

add_executable(aquatrack-annotation-session-tests
    tests/unit/native/AnnotationSessionTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-annotation-session-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-annotation-session-tests PRIVATE aquatrack-domain)
add_test(NAME AquaTrackAnnotationSession COMMAND aquatrack-annotation-session-tests)

add_executable(aquatrack-annotation-controller-tests
    tests/unit/native/AnnotationControllerTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-annotation-controller-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-annotation-controller-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackAnnotationController COMMAND aquatrack-annotation-controller-tests)

add_executable(aquatrack-project-store-tests
    tests/unit/native/ProjectFileStoreTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-project-store-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackProjectStore COMMAND aquatrack-project-store-tests)

add_executable(aquatrack-media-importer-tests
    tests/unit/native/MediaImporterTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-media-importer-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-media-importer-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackMediaImporter COMMAND aquatrack-media-importer-tests)

add_executable(aquatrack-media-pipeline-tests
    tests/unit/native/MediaPipelineTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-media-pipeline-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-media-pipeline-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackMediaPipeline COMMAND aquatrack-media-pipeline-tests)

add_executable(aquatrack-project-controller-tests
    tests/unit/native/ProjectControllerTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-project-controller-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackProjectController COMMAND aquatrack-project-controller-tests)

add_executable(aquatrack-analytics-tests
    tests/unit/native/SurveyAnalyticsTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-analytics-tests PRIVATE aquatrack-analytics)
add_test(NAME AquaTrackAnalytics COMMAND aquatrack-analytics-tests)

add_executable(aquatrack-export-tests tests/unit/native/ObservationExportTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-export-tests PRIVATE aquatrack-export)
add_test(NAME AquaTrackExport COMMAND aquatrack-export-tests)

add_executable(aquatrack-export-controller-tests
    tests/unit/native/ExportControllerTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-export-controller-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-export-controller-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackExportController COMMAND aquatrack-export-controller-tests)

add_executable(aquatrack-model-registry-tests tests/unit/native/ModelRegistryTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-model-registry-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackModelRegistry COMMAND aquatrack-model-registry-tests)

add_executable(aquatrack-settings-store-tests tests/unit/native/SettingsStoreTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-settings-store-tests PRIVATE aquatrack-data aquatrack-system)
add_test(NAME AquaTrackSettingsStore COMMAND aquatrack-settings-store-tests)

add_executable(aquatrack-pipeline-manager-tests tests/unit/native/PipelineManagerTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-pipeline-manager-tests PRIVATE aquatrack-system)
add_test(NAME AquaTrackPipelineManager COMMAND aquatrack-pipeline-manager-tests)

add_executable(aquatrack-structured-logger-tests
    tests/unit/native/StructuredLoggerTests.cpp
    tests/unit/native/TestSupport.h
)
target_include_directories(aquatrack-structured-logger-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-structured-logger-tests PRIVATE aquatrack-system)
add_test(NAME AquaTrackStructuredLogger COMMAND aquatrack-structured-logger-tests)

add_executable(aquatrack-ui-smoke-tests tests/ui/MainWindowSmokeTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-ui-smoke-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-ui-smoke-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackUiSmoke COMMAND aquatrack-ui-smoke-tests)

add_executable(aquatrack-ui-wiring-tests tests/ui/MainWindowWiringTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-ui-wiring-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-ui-wiring-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackUiWiring COMMAND aquatrack-ui-wiring-tests)

add_executable(aquatrack-project-flow-tests tests/ui/ProjectFlowTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-project-flow-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-project-flow-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackProjectFlow COMMAND aquatrack-project-flow-tests)

add_executable(aquatrack-datasets-page-tests tests/ui/DatasetsPageTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-datasets-page-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-datasets-page-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackDatasetsPage COMMAND aquatrack-datasets-page-tests)

add_executable(aquatrack-glyph-tests tests/ui/GlyphTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-glyph-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-glyph-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackGlyphs COMMAND aquatrack-glyph-tests)

add_executable(aquatrack-models-page-tests tests/ui/ModelsPageTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-models-page-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-models-page-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackModelsPage COMMAND aquatrack-models-page-tests)

add_executable(aquatrack-videos-page-tests tests/ui/VideosPageTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-videos-page-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-videos-page-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackVideosPage COMMAND aquatrack-videos-page-tests)
add_test(NAME AquaTrackCliHelp COMMAND aquatrack-cli --help)

add_executable(aquatrack-review-video-tests tests/ui/ReviewVideoTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-review-video-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-review-video-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackReviewVideo COMMAND aquatrack-review-video-tests)

add_executable(aquatrack-project-switch-tests tests/ui/ProjectSwitchTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-project-switch-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-project-switch-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackProjectSwitch COMMAND aquatrack-project-switch-tests)

add_executable(aquatrack-sequence-editing-tests
    tests/unit/native/SequenceEditingTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-sequence-editing-tests PRIVATE tests/unit/native)
target_link_libraries(aquatrack-sequence-editing-tests PRIVATE aquatrack-application)
add_test(NAME AquaTrackSequenceEditing COMMAND aquatrack-sequence-editing-tests)

add_executable(aquatrack-sequence-edit-flow-tests tests/ui/SequenceEditFlowTests.cpp tests/unit/native/TestSupport.h)
target_include_directories(aquatrack-sequence-edit-flow-tests PRIVATE ${PROJECT_SOURCE_DIR}/tests/unit/native)
target_link_libraries(aquatrack-sequence-edit-flow-tests PRIVATE aquatrack-ui)
add_test(NAME AquaTrackSequenceEditFlow COMMAND aquatrack-sequence-edit-flow-tests)
