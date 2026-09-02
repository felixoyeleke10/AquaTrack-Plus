add_library(aquatrack-analytics
    src/analytics/SurveyAnalytics.cpp
    src/analytics/SurveyAnalytics.h
)
target_include_directories(aquatrack-analytics PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aquatrack-analytics PUBLIC aquatrack-domain)
