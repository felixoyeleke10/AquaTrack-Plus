qt_add_executable(aquatrack-cli apps/cli/main.cpp)
target_link_libraries(aquatrack-cli PRIVATE aquatrack-application aquatrack-data Qt6::Core)
