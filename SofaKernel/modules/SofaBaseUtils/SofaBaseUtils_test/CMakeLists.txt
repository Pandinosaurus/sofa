cmake_minimum_required(VERSION 3.12)

project(SofaBaseUtils_test)

set(SOURCE_FILES
)

# Do not find dependencies explicitely, let CMake decide the appropriate time to do it
#sofa_find_package(SofaTest REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} Sofa.Testing SofaBaseUtils SofaBase)
add_definitions("-DSOFABASEUTILS_TEST_RESOURCES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"")

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
