cmake_minimum_required(VERSION 3.12)
project(SofaHeadlessRecorder)

set(SOURCE_FILES
    empty.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

find_package(Sofa.GUI.HeadlessRecorder REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Sofa.GUI.HeadlessRecorder)

sofa_create_package_with_targets(
    PACKAGE_NAME ${PROJECT_NAME}
    PACKAGE_VERSION ${Sofa_VERSION}
    TARGETS ${PROJECT_NAME} AUTO_SET_TARGET_PROPERTIES
    INCLUDE_SOURCE_DIR "src"
    INCLUDE_INSTALL_DIR ${PROJECT_NAME}
)
