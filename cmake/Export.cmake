add_library(aquatrack-export src/export/ObservationExport.cpp src/export/ObservationExport.h)
target_include_directories(aquatrack-export PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-export PUBLIC aquatrack-domain)
