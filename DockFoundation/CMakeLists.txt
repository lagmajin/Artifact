cmake_minimum_required(VERSION 3.28)

project(ArtifactDockFoundation VERSION 1.0.0 LANGUAGES CXX)

if(NOT TARGET Qt6::Core OR NOT TARGET Qt6::Gui OR NOT TARGET Qt6::Widgets)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets)
endif()

include(CMakePackageConfigHelpers)

add_library(ArtifactDockFoundation STATIC)
target_sources(ArtifactDockFoundation
    PUBLIC FILE_SET CXX_MODULES TYPE CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include/Widgets/Dock"
    FILES
        "${CMAKE_CURRENT_LIST_DIR}/../include/Widgets/Dock/DockManager.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/../include/Widgets/Dock/NativeDockSurface.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/../include/Widgets/Dock/ArtifactDockManager.ixx"
        "${CMAKE_CURRENT_LIST_DIR}/../include/Widgets/Dock/ArtifactNativeDockSurface.ixx"
)
add_library(Dock::Foundation ALIAS ArtifactDockFoundation)

set_target_properties(ArtifactDockFoundation PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    EXPORT_NAME Foundation
    FOLDER "Artifact/Libraries"
)

target_compile_features(ArtifactDockFoundation PUBLIC cxx_std_23)
target_include_directories(ArtifactDockFoundation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ArtifactDockFoundation PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ArtifactDockFoundation PRIVATE
        /Zf
        /Zc:__cplusplus
        /utf-8
        /wd5050
    )
    target_compile_definitions(ArtifactDockFoundation PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        _WIN32_WINNT=0x0A00
    )
endif()

install(TARGETS ArtifactDockFoundation
    EXPORT DockFoundationTargets
    ARCHIVE DESTINATION lib
    FILE_SET CXX_MODULES DESTINATION include/DockFoundation
)
install(EXPORT DockFoundationTargets
    FILE DockFoundationTargets.cmake
    NAMESPACE Dock::
    DESTINATION lib/cmake/DockFoundation
    CXX_MODULES_DIRECTORY cxx-modules
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/DockFoundationConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DockFoundationConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DockFoundation
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DockFoundationConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DockFoundationConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DockFoundationConfigVersion.cmake"
    DESTINATION lib/cmake/DockFoundation
)
