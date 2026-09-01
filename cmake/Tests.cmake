enable_testing()
add_executable(aquatrack-tracker-tests tests/unit/native/TrackerTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-tracker-tests PRIVATE aquatrack-domain)
add_test(NAME AquaTrackTracker COMMAND aquatrack-tracker-tests)

add_executable(aquatrack-data-tests tests/unit/native/BrackishMotSequenceTests.cpp tests/unit/native/TestSupport.h)
target_link_libraries(aquatrack-data-tests PRIVATE aquatrack-data)
add_test(NAME AquaTrackData COMMAND aquatrack-data-tests)
