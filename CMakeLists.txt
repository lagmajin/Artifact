cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "Required for import std" FORCE)
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "Keep import std opt-in during toolchain detection" FORCE)
set(CMAKE_CXX_STANDARD 23 CACHE STRING "Required for import std detection" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)

project(Artifact)

# Only the toolchain-detection phase needs the global import-std default.
# Reset it before adding subdirectories so third-party targets do not inherit
# CXX_MODULE_STD unless they opt in explicitly.
set(CMAKE_CXX_MODULE_STD OFF CACHE BOOL "" FORCE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ARTIFACT_MSVC_MODULES_JSON "")

    get_filename_component(_std_x64   "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_std_host  "${_std_x64}" DIRECTORY)
    get_filename_component(_std_bin   "${_std_host}" DIRECTORY)
    get_filename_component(_std_root  "${_std_bin}" DIRECTORY)
    if(EXISTS "${_std_root}/modules/modules.json")
        set(ARTIFACT_MSVC_MODULES_JSON "${_std_root}/modules/modules.json")
    endif()

    if(NOT ARTIFACT_MSVC_MODULES_JSON)
        file(GLOB_RECURSE _std_ixx_cands LIST_DIRECTORIES FALSE
            "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/14.*/modules/modules.json"
            "C:/Program Files/Microsoft Visual Studio/18/*/VC/Tools/MSVC/14.*/modules/modules.json")
        list(SORT _std_ixx_cands ORDER DESCENDING)
        if(_std_ixx_cands)
            list(GET _std_ixx_cands 0 ARTIFACT_MSVC_MODULES_JSON)
        endif()
    endif()

    if(ARTIFACT_MSVC_MODULES_JSON)
        set(CMAKE_CXX_STDLIB_MODULES_JSON
            "${ARTIFACT_MSVC_MODULES_JSON}"
            CACHE FILEPATH "MSVC standard library module metadata override" FORCE)
    endif()

    get_filename_component(_artifact_msvc_include_root "${_std_root}/include" ABSOLUTE)
    file(GLOB _artifact_windows_sdk_include_versions LIST_DIRECTORIES TRUE
        "C:/Program Files (x86)/Windows Kits/10/Include/10.*")
    if(_artifact_windows_sdk_include_versions)
        list(SORT _artifact_windows_sdk_include_versions ORDER DESCENDING)
        list(GET _artifact_windows_sdk_include_versions 0 _artifact_windows_sdk_include_root)
    endif()

    set(_artifact_std_include_dirs "")
    if(EXISTS "${_artifact_msvc_include_root}")
        list(APPEND _artifact_std_include_dirs "${_artifact_msvc_include_root}")
    endif()
    foreach(_artifact_sdk_subdir IN ITEMS ucrt shared um winrt)
        if(EXISTS "${_artifact_windows_sdk_include_root}/${_artifact_sdk_subdir}")
            list(APPEND _artifact_std_include_dirs "${_artifact_windows_sdk_include_root}/${_artifact_sdk_subdir}")
        endif()
    endforeach()

    if(_artifact_std_include_dirs)
        foreach(_artifact_include_dir IN LISTS _artifact_std_include_dirs)
            string(APPEND CMAKE_C_FLAGS   " /I\"${_artifact_include_dir}\"")
            string(APPEND CMAKE_CXX_FLAGS " /I\"${_artifact_include_dir}\"")
        endforeach()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C flags with explicit MSVC include paths" FORCE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "CXX flags with explicit MSVC include paths" FORCE)
        set(ENV{INCLUDE} "${_artifact_std_include_dirs};$ENV{INCLUDE}")

        if(TARGET __cmake_cxx23)
            target_include_directories(__cmake_cxx23 SYSTEM PRIVATE ${_artifact_std_include_dirs})
        endif()
    endif()
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

function(artifact_enable_delay_load target)
    if(NOT WIN32 OR NOT MSVC)
        return()
    endif()

    cmake_parse_arguments(AEDL "" "" "DEBUG_DLLS;RELEASE_DLLS" ${ARGN})
    if(NOT AEDL_DEBUG_DLLS AND NOT AEDL_RELEASE_DLLS)
        return()
    endif()

    get_filename_component(_artifact_msvc_link_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_link_dir "${_artifact_msvc_link_dir}" DIRECTORY)
    set(_artifact_delayimp_lib "${_artifact_msvc_link_dir}/lib/x64/delayimp.lib")
    if(EXISTS "${_artifact_delayimp_lib}")
        target_link_libraries(${target} PRIVATE "${_artifact_delayimp_lib}")
    else()
        target_link_libraries(${target} PRIVATE delayimp)
    endif()
    foreach(dllName IN LISTS AEDL_DEBUG_DLLS)
        target_link_options(${target} PRIVATE "$<$<CONFIG:Debug>:/DELAYLOAD:${dllName}>")
    endforeach()
    foreach(dllName IN LISTS AEDL_RELEASE_DLLS)
        target_link_options(${target} PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/DELAYLOAD:${dllName}>")
    endforeach()
endfunction()

set(ARTIFACT_COMPILER_MSVC_CL OFF)
set(ARTIFACT_COMPILER_CLANG_CL OFF)
set(ARTIFACT_COMPILER_INTELLLVM_CL OFF)
set(ARTIFACT_COMPILER_INTELLLVM_GNU OFF)
set(ARTIFACT_FRONTEND_MSVC OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_MSVC_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_CLANG_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM" AND
       CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(ARTIFACT_COMPILER_INTELLLVM_CL ON)
    set(ARTIFACT_FRONTEND_MSVC ON)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set(ARTIFACT_COMPILER_INTELLLVM_GNU ON)
endif()

if(NOT TARGET ArtifactCore)
    if(NOT TARGET ufbx AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ufbx/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/ufbx"
                         "${CMAKE_CURRENT_BINARY_DIR}/ufbx"
                         EXCLUDE_FROM_ALL)
    endif()

    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ArtifactCore"
                     "${CMAKE_CURRENT_BINARY_DIR}/ArtifactCore")
endif()

if(NOT TARGET ArtifactWidgets)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ArtifactWidgets"
                     "${CMAKE_CURRENT_BINARY_DIR}/ArtifactWidgets")
endif()

# ---------------------------------------------------------------------------
# Version
# ---------------------------------------------------------------------------
set(ARTIFACT_VERSION_MAJOR 0)
set(ARTIFACT_VERSION_MINOR 9)
set(ARTIFACT_VERSION_PATCH 0)
set(ARTIFACT_VERSION "${ARTIFACT_VERSION_MAJOR}.${ARTIFACT_VERSION_MINOR}.${ARTIFACT_VERSION_PATCH}")

option(ARTIFACT_EMBED_BUILD_METADATA
    "Embed current Git revision, dirty state, and configuration timestamp" OFF)

# Do not invoke Git or churn AppMain's compile definitions for ordinary
# developer builds.  Release/diagnostic builds can opt in when provenance is
# required in the window title and --version output.
set(ARTIFACT_BUILD_GIT_HASH "dev")
set(ARTIFACT_BUILD_DIRTY "not-checked")
set(ARTIFACT_BUILD_TIMESTAMP "local")
if(ARTIFACT_EMBED_BUILD_METADATA)
    find_program(ARTIFACT_GIT_EXECUTABLE git)
    if(ARTIFACT_GIT_EXECUTABLE)
        execute_process(
            COMMAND "${ARTIFACT_GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse --short=12 HEAD
            OUTPUT_VARIABLE ARTIFACT_BUILD_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT ARTIFACT_BUILD_GIT_HASH)
            set(ARTIFACT_BUILD_GIT_HASH "unknown")
        endif()

        execute_process(
            COMMAND "${ARTIFACT_GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" status --porcelain --untracked-files=no
            OUTPUT_VARIABLE ARTIFACT_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(ARTIFACT_GIT_STATUS)
            set(ARTIFACT_BUILD_DIRTY "dirty")
        else()
            set(ARTIFACT_BUILD_DIRTY "clean")
        endif()
    endif()
    string(TIMESTAMP ARTIFACT_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
endif()

# ---------------------------------------------------------------------------
# Explicit source manifest
# ---------------------------------------------------------------------------
# Do not recursively discover application sources during configure.  New
# files must be registered in cmake/ArtifactSources.cmake, where public
# source-only .cppm units are listed in ARTIFACT_APP_MODULE_SOURCES.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArtifactSources.cmake")
set(APP_MODULES ${ARTIFACT_APP_MODULE_SOURCES})
set(APP_IMPL ${ARTIFACT_APP_IMPL_SOURCES})

# Composition export modules are imported by the File menu and each writer.
# Keep the complete public/implementation set explicit so an existing build
# directory cannot omit newly-added export units when the recursive glob is
# not re-evaluated until a later configure.
set(_artifact_export_modules
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportDialog.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportGamefaceWriter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportLottieWriter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportNoesisXamlWriter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportPreRenderPipeline.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportRmlUiWriter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportSession.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/ArtifactExportUnityUxmlWriter.ixx")
set(_artifact_export_impls
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportDialog.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportGamefaceWriter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportLottieWriter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportNoesisXamlWriter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportPreRenderPipeline.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportRmlUiWriter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportSession.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/ArtifactExportUnityUxmlWriter.cppm")
foreach(_artifact_export_module IN LISTS _artifact_export_modules)
    list(FIND APP_MODULES "${_artifact_export_module}" _artifact_export_module_idx)
    if(_artifact_export_module_idx EQUAL -1)
        list(APPEND APP_MODULES "${_artifact_export_module}")
    endif()
endforeach()
foreach(_artifact_export_impl IN LISTS _artifact_export_impls)
    list(FIND APP_IMPL "${_artifact_export_impl}" _artifact_export_impl_idx)
    if(_artifact_export_impl_idx EQUAL -1)
        list(APPEND APP_IMPL "${_artifact_export_impl}")
    endif()
endforeach()

# Source-only .cppm interfaces and partitions are classified in the manifest.

# CMake の module 自動分類で AppDebuggerWidget.cppm が link 対象から漏れるケースがあるため、
# 明示的に private source として固定する。
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Diagnostics/AppDebuggerWidget.cppm" _app_debugger_widget_idx)
if(_app_debugger_widget_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Diagnostics/AppDebuggerWidget.cppm")
endif()

# CompositionManager は module/legacy 混在の影響で自動収集から漏れることがあるため固定する。
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/ArtifactCompositionManager.cppm" _composition_manager_idx)
if(_composition_manager_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/ArtifactCompositionManager.cppm")
endif()

# SwitchLayer はインターフェースと同名の実装 module を持つため、
# 自動分類や古い生成結果に依存せず実装を必ずターゲットへ含める。
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactSwitchLayer.cppm" _switch_layer_impl_idx)
if(_switch_layer_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactSwitchLayer.cppm")
endif()

# OffscreenCompositionRenderer は他モジュールから import される公開モジュールなので、
# 自動分類の漏れを防ぐため明示的に CXX_MODULES に含める。
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactOffscreenCompositionRenderer.cppm" _offscreen_composition_module_idx)
if(_offscreen_composition_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactOffscreenCompositionRenderer.cppm")
endif()

# MaterialContainerLayer is a public layer module. Keep it explicitly registered
# so new-file discovery does not depend on the recursive glob pass alone.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactMaterialContainerLayer.ixx" _material_container_module_idx)
if(_material_container_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactMaterialContainerLayer.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactMaterialContainerLayer.cppm" _material_container_impl_idx)
if(_material_container_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactMaterialContainerLayer.cppm")
endif()

# SandSim2DLayer - 2D falling-sand cellular automaton layer
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactSandSim2DLayer.ixx" _sand_sim_2d_layer_module_idx)
if(_sand_sim_2d_layer_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactSandSim2DLayer.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactSandSim2DLayer.cppm" _sand_sim_2d_layer_impl_idx)
if(_sand_sim_2d_layer_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactSandSim2DLayer.cppm")
endif()

# FormParticleLayer is imported from multiple modules and should also be pinned
# explicitly so module BMI generation does not depend on the recursive glob scan.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactFormParticleLayer.ixx" _form_particle_module_idx)
if(_form_particle_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactFormParticleLayer.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactFormParticleLayer.cppm" _form_particle_impl_idx)
if(_form_particle_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactFormParticleLayer.cppm")
endif()

# Environment map layer is imported by the factory and should be pinned
# explicitly so module discovery does not depend on recursive glob timing.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactEnvironmentMapLayer.ixx" _environment_map_layer_module_idx)
if(_environment_map_layer_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactEnvironmentMapLayer.ixx")
endif()
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactEnvironmentMapLayerInitParams.ixx" _environment_map_params_module_idx)
if(_environment_map_params_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactEnvironmentMapLayerInitParams.ixx")
endif()

# Paint layer is imported by the composition overlay and brush tools, so pin it
# explicitly to avoid module discovery timing issues.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactPaintLayer.ixx" _paint_layer_module_idx)
if(_paint_layer_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactPaintLayer.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactPaintLayer.cppm" _paint_layer_impl_idx)
if(_paint_layer_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactPaintLayer.cppm")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactEnvironmentMapLayer.cppm" _environment_map_layer_impl_idx)
if(_environment_map_layer_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactEnvironmentMapLayer.cppm")
endif()

# DopeSheetWidget is imported from AppMain and should be pinned explicitly so
# module discovery does not depend on the recursive glob scan.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/ArtifactDopeSheetWidget.ixx" _dope_sheet_widget_module_idx)
if(_dope_sheet_widget_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/ArtifactDopeSheetWidget.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/ArtifactDopeSheetWidget.cppm" _dope_sheet_widget_impl_idx)
if(_dope_sheet_widget_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/ArtifactDopeSheetWidget.cppm")
endif()

# Composition settings shell and inspector tab surfaces are imported from
# multiple widget modules, so pin them explicitly instead of relying on the
# recursive glob scan / module detection order.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Dialog/ArtifactCompositionDialogShell.ixx" _composition_dialog_shell_module_idx)
if(_composition_dialog_shell_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Dialog/ArtifactCompositionDialogShell.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Dialog/ArtifactCompositionDialogShell.cppm" _composition_dialog_shell_impl_idx)
if(_composition_dialog_shell_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Dialog/ArtifactCompositionDialogShell.cppm")
endif()
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Inspector/ArtifactEffectTabSurface.ixx" _effect_tab_surface_module_idx)
if(_effect_tab_surface_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Inspector/ArtifactEffectTabSurface.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Inspector/ArtifactEffectTabSurface.cppm" _effect_tab_surface_impl_idx)
if(_effect_tab_surface_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Inspector/ArtifactEffectTabSurface.cppm")
endif()
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Inspector/ArtifactComponentTabSurface.ixx" _component_tab_surface_module_idx)
if(_component_tab_surface_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Inspector/ArtifactComponentTabSurface.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Inspector/ArtifactComponentTabSurface.cppm" _component_tab_surface_impl_idx)
if(_component_tab_surface_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Inspector/ArtifactComponentTabSurface.cppm")
endif()

# Procedural3DLayer owns the Terrain / Path Tube authoring and renderer bridge.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactProcedural3DLayer.ixx" _procedural_3d_layer_module_idx)
if(_procedural_3d_layer_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactProcedural3DLayer.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactProcedural3DLayer.cppm" _procedural_3d_layer_impl_idx)
if(_procedural_3d_layer_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Layer/ArtifactProcedural3DLayer.cppm")
endif()

# Layer component contracts are imported by the layer implementation and by
# renderer-side evaluators. Pin the public module explicitly instead of relying
# on recursive glob discovery.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactLayerComponentSystem.ixx" _layer_component_system_module_idx)
if(_layer_component_system_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Layer/ArtifactLayerComponentSystem.ixx")
endif()

# Color science / OCIO modules are imported from the color science panel and
# should be pinned explicitly so module discovery does not depend on scan order.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactOCIOManager.ixx" _ocio_manager_module_idx)
if(_ocio_manager_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactOCIOManager.ixx")
endif()

# Glow Variants Pack modules are explicit so adding the files does not rely on
# recursive glob discovery.
foreach(glow_variant IN ITEMS LiquidGlow ResidualGlow)
    list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/${glow_variant}Effect.ixx" _glow_variant_module_idx)
    if(_glow_variant_module_idx EQUAL -1)
        list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/${glow_variant}Effect.ixx")
    endif()
    list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/${glow_variant}Effect.cppm" _glow_variant_impl_idx)
    if(_glow_variant_impl_idx EQUAL -1)
        list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/${glow_variant}Effect.cppm")
    endif()
endforeach()

# New creative effect modules (Kaleidoscope, Dithering, Kuwahara) — explicitly pinned
foreach(_new_effect IN ITEMS Kaleidoscope Dithering Kuwahara)
    list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/${_new_effect}/${_new_effect}Effect.ixx" _new_effect_module_idx)
    if(_new_effect_module_idx EQUAL -1)
        list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/${_new_effect}/${_new_effect}Effect.ixx")
    endif()
    list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_new_effect}/${_new_effect}Effect.cppm" _new_effect_impl_idx)
    if(_new_effect_impl_idx EQUAL -1)
        list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_new_effect}/${_new_effect}Effect.cppm")
    endif()
endforeach()

# EffectService imports these modules directly.  Keep both the interface and
# implementation units explicit so the C++ module mapper always emits their
# IFC references, including files that glob discovery can miss.
foreach(_service_effect IN ITEMS
    "ColorCorrection/InvertEffect"
    "Rasterizer/VignetteEffect"
    "Rasterizer/StripesEffect"
    "Rasterizer/VoronoiEffect"
    "Rasterizer/BricksEffect"
    "Rasterizer/HexGridEffect")
    list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/${_service_effect}.ixx" _service_effect_module_idx)
    if(_service_effect_module_idx EQUAL -1)
        list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/${_service_effect}.ixx")
    endif()
    list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_service_effect}.cppm" _service_effect_impl_idx)
    if(_service_effect_impl_idx EQUAL -1)
        list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_service_effect}.cppm")
    endif()
endforeach()

# Distort pack additions are pinned explicitly so new modules do not depend on
# recursive glob discovery or content scanning behavior.
foreach(_distort_effect IN ITEMS TimeDisplacement)
    list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/${_distort_effect}Effect.ixx" _distort_effect_module_idx)
    if(_distort_effect_module_idx EQUAL -1)
        list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/${_distort_effect}Effect.ixx")
    endif()
    list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_distort_effect}/${_distort_effect}Effect.cppm" _distort_effect_impl_idx)
    if(_distort_effect_impl_idx EQUAL -1)
        list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/${_distort_effect}/${_distort_effect}Effect.cppm")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
# ViewportColorPipeline is imported by the composition controller; pin both
# units explicitly so the module mapper sees the interface before the
# controller BMI is scanned.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Render/ViewportColorPipeline.ixx" _viewport_color_module_idx)
if(_viewport_color_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Render/ViewportColorPipeline.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Render/ViewportColorPipeline.cppm" _viewport_color_impl_idx)
if(_viewport_color_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Render/ViewportColorPipeline.cppm")
endif()

# ViewportMath is imported by the composition controller; pin both units so
# the module mapper sees the coordinate conversion contract before scanning
# ArtifactCompositionRenderController.cppm.
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Render/ViewportMath.ixx" _viewport_math_module_idx)
if(_viewport_math_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Widgets/Render/ViewportMath.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Render/ViewportMath.cppm" _viewport_math_impl_idx)
if(_viewport_math_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Widgets/Render/ViewportMath.cppm")
endif()

set(ARTIFACT_RENDER_SUPPORT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactHDRMonitor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRamPreviewController.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderController.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderScheduler.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderQueuePresets.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderROI.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderFoundation.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactPerformanceMonitor.ixx"
)
set(ARTIFACT_RENDER_SUPPORT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactHDRMonitor.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactRamPreviewController.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactRenderController.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactRenderScheduler.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactRenderQueuePresets.cppm"
)

set(ARTIFACT_RENDER_SUPPORT_CONTRACT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderROI.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactRenderFoundation.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactPerformanceMonitor.ixx"
)
set(ARTIFACT_RENDER_SUPPORT_CONTRACT_IMPL)
list(REMOVE_ITEM ARTIFACT_RENDER_SUPPORT_MODULES ${ARTIFACT_RENDER_SUPPORT_CONTRACT_MODULES})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_RENDER_SUPPORT_CONTRACT_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_RENDER_SUPPORT_CONTRACT_IMPL})

list(REMOVE_ITEM APP_MODULES ${ARTIFACT_RENDER_SUPPORT_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_RENDER_SUPPORT_IMPL})

add_library(ArtifactRenderSupport STATIC)
set_target_properties(ArtifactRenderSupport PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactRenderSupport
    PRIVATE ${ARTIFACT_RENDER_SUPPORT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_RENDER_SUPPORT_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactRenderSupport PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactRenderSupport PROPERTY CXX_MODULE_STD OFF)
endif()
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactRenderSupport PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactRenderSupport PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
endif()
target_include_directories(ArtifactRenderSupport PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
)
target_link_libraries(ArtifactRenderSupport PUBLIC ArtifactCore ArtifactCoreAudio)

add_library(ArtifactRenderSupportContracts STATIC)
set_target_properties(ArtifactRenderSupportContracts PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactRenderSupportContracts
    PRIVATE ${ARTIFACT_RENDER_SUPPORT_CONTRACT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_RENDER_SUPPORT_CONTRACT_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactRenderSupportContracts PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactRenderSupportContracts PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactRenderSupportContracts PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
)
target_link_libraries(ArtifactRenderSupportContracts PUBLIC ArtifactCore)
target_link_libraries(ArtifactRenderSupport PUBLIC ArtifactRenderSupportContracts)

set(ARTIFACT_ASSET_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetMenuModel.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/ArtifactAssetResult.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/ArtifactAssetReference.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/ArtifactAssetIntegrationSettings.ixx"
)
set(ARTIFACT_ASSET_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetMenuModel.cppm"
)
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_ASSET_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_ASSET_IMPL})

add_library(ArtifactAsset SHARED)
set_target_properties(ArtifactAsset PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactAsset
    PRIVATE ${ARTIFACT_ASSET_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_ASSET_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactAsset PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactAsset PROPERTY CXX_MODULE_STD OFF)
endif()
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactAsset PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactAsset PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
endif()
target_include_directories(ArtifactAsset PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
)
target_link_libraries(ArtifactAsset PUBLIC ArtifactCore ArtifactCoreAudio ArtifactCoreMedia ArtifactCoreData ArtifactCoreAsset)

set(ARTIFACT_COLOR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ColorPaletteManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactOCIOManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorSettings.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorScienceManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorNodeGraph.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorNode.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorManagement.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorGradingEngine.ixx"
)
set(ARTIFACT_COLOR_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ColorPaletteManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactOCIOManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorSettings.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorScienceManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorNodeGraph.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorNode.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorManagement.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorGradingEngine.cppm"
)

set(ARTIFACT_COLOR_PALETTE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ColorPaletteManager.ixx"
)
set(ARTIFACT_COLOR_PALETTE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ColorPaletteManager.cppm"
)
set(ARTIFACT_COLOR_NODE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorNode.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorNodeGraph.ixx"
)
set(ARTIFACT_COLOR_NODE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorNode.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorNodeGraph.cppm"
)
set(ARTIFACT_COLOR_SETTINGS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorSettings.ixx"
)
set(ARTIFACT_COLOR_SETTINGS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorSettings.cppm"
)
set(ARTIFACT_COLOR_SCIENCE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorScienceManager.ixx"
)
set(ARTIFACT_COLOR_SCIENCE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorScienceManager.cppm"
)
set(ARTIFACT_COLOR_MANAGEMENT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorManagement.ixx"
)
set(ARTIFACT_COLOR_MANAGEMENT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorManagement.cppm"
)
set(ARTIFACT_COLOR_GRADING_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Color/ArtifactColorGradingEngine.ixx"
)
set(ARTIFACT_COLOR_GRADING_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ArtifactColorGradingEngine.cppm"
)
list(REMOVE_ITEM ARTIFACT_COLOR_MODULES ${ARTIFACT_COLOR_PALETTE_MODULES} ${ARTIFACT_COLOR_NODE_MODULES} ${ARTIFACT_COLOR_SETTINGS_MODULES} ${ARTIFACT_COLOR_SCIENCE_MODULES} ${ARTIFACT_COLOR_MANAGEMENT_MODULES} ${ARTIFACT_COLOR_GRADING_MODULES})
list(REMOVE_ITEM ARTIFACT_COLOR_IMPL ${ARTIFACT_COLOR_PALETTE_IMPL} ${ARTIFACT_COLOR_NODE_IMPL} ${ARTIFACT_COLOR_SETTINGS_IMPL} ${ARTIFACT_COLOR_SCIENCE_IMPL} ${ARTIFACT_COLOR_MANAGEMENT_IMPL} ${ARTIFACT_COLOR_GRADING_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_COLOR_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_COLOR_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_COLOR_PALETTE_MODULES} ${ARTIFACT_COLOR_NODE_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_COLOR_PALETTE_IMPL} ${ARTIFACT_COLOR_NODE_IMPL} ${ARTIFACT_COLOR_SETTINGS_IMPL} ${ARTIFACT_COLOR_SCIENCE_IMPL} ${ARTIFACT_COLOR_MANAGEMENT_IMPL} ${ARTIFACT_COLOR_GRADING_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_COLOR_SETTINGS_MODULES} ${ARTIFACT_COLOR_SCIENCE_MODULES} ${ARTIFACT_COLOR_MANAGEMENT_MODULES} ${ARTIFACT_COLOR_GRADING_MODULES})

add_library(ArtifactColor SHARED)
set_target_properties(ArtifactColor PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactColor
    PRIVATE ${ARTIFACT_COLOR_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColor PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColor PROPERTY CXX_MODULE_STD OFF)
endif()
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactColor PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactColor PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
endif()
target_include_directories(ArtifactColor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
)
target_compile_definitions(ArtifactColor PRIVATE LIBRARY_DLL_MODE=1)
target_link_libraries(ArtifactColor PUBLIC ArtifactCore ArtifactCoreAudio)

add_library(ArtifactColorPalette STATIC)
set_target_properties(ArtifactColorPalette PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorPalette PRIVATE ${ARTIFACT_COLOR_PALETTE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_PALETTE_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorPalette PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorPalette PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorPalette PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorPalette PUBLIC ArtifactCore)

add_library(ArtifactColorNode STATIC)
set_target_properties(ArtifactColorNode PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorNode PRIVATE ${ARTIFACT_COLOR_NODE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_NODE_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorNode PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorNode PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorNode PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorNode PUBLIC ArtifactCore)

add_library(ArtifactColorSettings STATIC)
set_target_properties(ArtifactColorSettings PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorSettings PRIVATE ${ARTIFACT_COLOR_SETTINGS_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_SETTINGS_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorSettings PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorSettings PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorSettings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorSettings PUBLIC ArtifactCore)

add_library(ArtifactColorScience STATIC)
set_target_properties(ArtifactColorScience PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorScience PRIVATE ${ARTIFACT_COLOR_SCIENCE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_SCIENCE_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorScience PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorScience PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorScience PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorScience PUBLIC ArtifactCore)
target_link_libraries(ArtifactColor PUBLIC ArtifactColorScience)

add_library(ArtifactColorManagement STATIC)
set_target_properties(ArtifactColorManagement PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorManagement PRIVATE ${ARTIFACT_COLOR_MANAGEMENT_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_MANAGEMENT_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorManagement PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorManagement PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorManagement PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorManagement PUBLIC ArtifactCore)

add_library(ArtifactColorGrading STATIC)
set_target_properties(ArtifactColorGrading PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON FOLDER "Artifact/Libraries")
target_sources(ArtifactColorGrading PRIVATE ${ARTIFACT_COLOR_GRADING_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_COLOR_GRADING_MODULES})
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactColorGrading PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactColorGrading PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactColorGrading PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src")
target_link_libraries(ArtifactColorGrading PUBLIC ArtifactCore)
target_link_libraries(ArtifactColor PUBLIC ArtifactColorManagement ArtifactColorGrading)

set(ARTIFACT_RENDER_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/RenderConfig.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/DiligentDeviceManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ShaderManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/RenderCommandBuffer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/IRenderSubmitter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/PrimitiveRenderer2D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/PrimitiveRenderer3D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/DiligentImmediateSubmitter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/DiligentBindlessSubmitter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactFinalPostProcess.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactOffscreenRenderer2D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactIRenderer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ArtifactMotionBlurPass.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/GPUTextureCacheManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/LOD/ArtifactLODManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ThickLineShaders.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/ViewerHelperShaders.ixx"
)
set(ARTIFACT_RENDER_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/DiligentDeviceManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ShaderManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/RenderCommandBuffer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/PrimitiveRenderer2D.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/PrimitiveRenderer3D.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/DiligentImmediateSubmitter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/DiligentBindlessSubmitter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactFinalPostProcess.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactOffscreenRenderer2D.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactIRenderer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/ArtifactMotionBlurPass.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/GPUTextureCacheManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/DiligentUploadCoordinator.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/LOD/ArtifactLODManager.cppm"
)

# Keep the first render split deliberately small.  Device and shader modules
# are the highest fan-out units and are therefore isolated before moving the
# renderer and widget orchestration layers.
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_RENDER_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_RENDER_IMPL})

add_library(ArtifactRender STATIC)
set_target_properties(ArtifactRender PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactRender
    PRIVATE ${ARTIFACT_RENDER_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_RENDER_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactRender PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactRender PROPERTY CXX_MODULE_STD OFF)
endif()
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactRender PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_definitions(ArtifactRender PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        GLOG_USE_GLOG_EXPORT
        _WIN32_WINNT=0x0A00
    )
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactRender PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_definitions(ArtifactRender PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        GLOG_USE_GLOG_EXPORT
        _WIN32_WINNT=0x0A00
    )
endif()
target_include_directories(ArtifactRender PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactRender PUBLIC ArtifactCore ArtifactCoreAudio ArtifactCoreVideo ArtifactCoreMedia ArtifactCoreNetwork ArtifactRenderSupportContracts)
if(TARGET Diligent-GraphicsTools)
    target_link_libraries(ArtifactRender PRIVATE Diligent-GraphicsTools)
endif()

set(ARTIFACT_EFFECT_CONTRACT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ArtifactAbstractEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ArtifactEffectImplBase.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/EffectContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ArtifactAbstractField.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ArtifactEffectFrameSampler.ixx"
)
set(ARTIFACT_EFFECT_CONTRACT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ArtifactAbstractEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ArtifactAbstractField.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ArtifactEffectFrameSampler.cppm"
)

list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECT_CONTRACT_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECT_CONTRACT_IMPL})

add_library(ArtifactEffectContract SHARED)
set_target_properties(ArtifactEffectContract PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectContract
    PRIVATE ${ARTIFACT_EFFECT_CONTRACT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECT_CONTRACT_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectContract PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectContract PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectContract PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
)
target_compile_definitions(ArtifactEffectContract PRIVATE LIBRARY_DLL_MODE=1)
target_link_libraries(ArtifactEffectContract PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactRenderSupport ArtifactRenderSupportContracts)

set(ARTIFACT_EFFECTS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AutoMosaicEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DataMoshEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/RadialBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/InvertEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/BrightnessEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/GrayscaleEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/FillEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ExposureEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/LevelsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/HueAndSaturation.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/SelectiveColorEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColorBalanceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColoramaEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/PhotoFilterEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/TritoneEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/CurvesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/GradientRampEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/WhiteBalanceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kaleidoscope/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Dithering/DitheringEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kuwahara/KuwaharaEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ChannelMixerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColorWheelsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VignetteEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StripesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HexGridEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticAberrationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalSmearEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/EdgeBloomEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ChromaticGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ReactiveGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DropShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/DirectionalGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/SatinEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Bevel/BevelEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/LumaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/ChromaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/DifferenceKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/IBKKeyerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LinearWipe/LinearWipeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Liquify/LiquifyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Mosaic/MosaicEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Spherize/SpherizeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Sharpen/SharpenEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/FindEdges/FindEdgesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/OpticalFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StrokeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorFlowGlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/DisplacementMapEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/TimeDisplacementEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/TurbulentDisplace/TurbulentDisplaceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/SimpleRainEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/RadioWavesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LensDistortion/LensDistortionEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/OpticsCompensation/OpticsCompensationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/BricksEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/EdgeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/EchoEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HalftoneEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PosterizeTimeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/RadialShadow/RadialShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/InnerShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LiquidGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ResidualGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LiftGammaGainEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AddNoise/AddNoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/AnisotropicFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ApertureShapeBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ReactionDiffusionBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/PhysicalHalationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LuminescenceCausticsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Noise/NoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticReliefEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DeflickerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DifferenceMatteEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FeedbackEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FilmDamageEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameBlendEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameAverageEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameAccumulationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FreezeFrameEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalDenoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalMedianEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TimeBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TimeWarpEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TrailFadeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/SlitScanEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ScreenShakeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StrobeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/SurfaceFX/SurfaceFXEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Wave/WaveEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GhostEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/LightTrailsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/MotionTrailEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PixelSortEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VoronoiEffect.ixx"
)
set(ARTIFACT_EFFECTS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AutoMosaicEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DataMoshEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/RadialBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/InvertEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/BrightnessEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/GrayscaleEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/FillEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ExposureEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/LevelsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/HueAndSaturation.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/SelectiveColorEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColorBalanceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColoramaEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/PhotoFilterEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/TritoneEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/CurvesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/GradientRampEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/WhiteBalanceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Dithering/DitheringEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kuwahara/KuwaharaEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ChannelMixerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColorWheelsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VignetteEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/StripesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HexGridEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalSmearEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/EdgeBloomEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ChromaticGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ReactiveGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DropShadow/DropShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DirectionalGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Satin/SatinEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Bevel/BevelEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/LumaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/ChromaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/DifferenceKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/IBKKeyerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LinearWipe/LinearWipeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Liquify/LiquifyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Mosaic/MosaicEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Spherize/SpherizeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Sharpen/SharpenEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/FindEdges/FindEdgesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/OpticalFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Stroke/StrokeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DisplacementMap/DisplacementMapEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/TimeDisplacement/TimeDisplacementEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/SimpleRainEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/RadioWavesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LensDistortion/LensDistortionEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/BricksEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/EdgeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/EchoEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HalftoneEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PosterizeTimeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/RadialShadow/RadialShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/InnerShadow/InnerShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LiquidGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ResidualGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LiftGammaGainEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AddNoise/AddNoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ApertureShapeBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/PhysicalHalationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LuminescenceCausticsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/NoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticReliefEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DeflickerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DifferenceMatteEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FeedbackEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FilmDamageEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameBlendEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameAverageEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameAccumulationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FreezeFrameEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalDenoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalMedianEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TimeBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TimeWarpEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TrailFadeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/SlitScanEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ScreenShakeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/StrobeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/SurfaceFX/SurfaceFXEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Wave/WaveEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GhostEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/LightTrailsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/MotionTrailEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PixelSortEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VoronoiEffect.cppm"
)
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_IMPL})

# Keep the implemented color-correction effects in a separate pack so changes
# to the much larger rasterizer pack do not rebuild these modules.  The three
# ColorCorrection interfaces without a cppm implementation remain outside
# this target and are intentionally not added here.
set(ARTIFACT_EFFECTS_COLOR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/BrightnessEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ChannelMixerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColoramaEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColorBalanceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ColorWheelsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/CurvesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/ExposureEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/FillEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/GradientRampEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/GrayscaleEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/HueAndSaturation.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/InvertEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/LevelsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/PhotoFilterEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/SelectiveColorEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/ColorCorrection/TritoneEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/WhiteBalanceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LiftGammaGainEffect.ixx"
)
set(ARTIFACT_EFFECTS_COLOR_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/BrightnessEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ChannelMixerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColoramaEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColorBalanceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ColorWheelsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/CurvesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/ExposureEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/FillEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/GradientRampEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/GrayscaleEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/HueAndSaturation.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/InvertEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/LevelsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/PhotoFilterEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/SelectiveColorEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/ColorCorrection/TritoneEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/WhiteBalanceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LiftGammaGainEffect.cppm"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES ${ARTIFACT_EFFECTS_COLOR_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL ${ARTIFACT_EFFECTS_COLOR_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_COLOR_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_COLOR_IMPL})

add_library(ArtifactEffectsColor STATIC)
set_target_properties(ArtifactEffectsColor PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsColor
    PRIVATE ${ARTIFACT_EFFECTS_COLOR_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_COLOR_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsColor PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsColor PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsColor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsColor PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Temporal effects keep frame/history state and are isolated from the broad
# rasterizer pack.  Effects that merely happen to be rendered in the
# Rasterizer directory remain in ArtifactEffects until their state contract
# is reviewed separately.
set(ARTIFACT_EFFECTS_TEMPORAL_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/EchoEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FeedbackEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameBlendEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameAverageEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FrameAccumulationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FreezeFrameEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalDenoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalMedianEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TemporalSmearEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TimeBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TimeWarpEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/TrailFadeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/SlitScanEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StrobeEffect.ixx"
)
set(ARTIFACT_EFFECTS_TEMPORAL_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/EchoEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FeedbackEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameBlendEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameAverageEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FrameAccumulationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FreezeFrameEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalDenoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalMedianEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TemporalSmearEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TimeBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TimeWarpEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/TrailFadeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/SlitScanEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/StrobeEffect.cppm"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES ${ARTIFACT_EFFECTS_TEMPORAL_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL ${ARTIFACT_EFFECTS_TEMPORAL_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_TEMPORAL_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_TEMPORAL_IMPL})

add_library(ArtifactEffectsTemporal STATIC)
set_target_properties(ArtifactEffectsTemporal PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsTemporal
    PRIVATE ${ARTIFACT_EFFECTS_TEMPORAL_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_TEMPORAL_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsTemporal PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsTemporal PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsTemporal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsTemporal PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Keying effects form a cohesive matte-generation pack.  Keep them separate
# from the broader spatial operators so keying changes do not rebuild that
# larger target.
set(ARTIFACT_EFFECTS_KEYING_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/LumaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/ChromaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/DifferenceKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/IBKKeyerEffect.ixx"
)
set(ARTIFACT_EFFECTS_KEYING_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/LumaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/ChromaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/DifferenceKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/IBKKeyerEffect.cppm"
)
add_library(ArtifactEffectsKeying STATIC)
set_target_properties(ArtifactEffectsKeying PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsKeying
    PRIVATE ${ARTIFACT_EFFECTS_KEYING_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_KEYING_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsKeying PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsKeying PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsKeying PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsKeying PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Blur effects are another cohesive spatial operator family.  Isolate the
# three implemented Blur modules from the larger Spatial pack.
set(ARTIFACT_EFFECTS_BLUR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/AnisotropicFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ApertureShapeBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ReactionDiffusionBlurEffect.ixx"
)
set(ARTIFACT_EFFECTS_BLUR_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ApertureShapeBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm"
)

add_library(ArtifactEffectsBlur STATIC)
set_target_properties(ArtifactEffectsBlur PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsBlur
    PRIVATE ${ARTIFACT_EFFECTS_BLUR_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_BLUR_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsBlur PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsBlur PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsBlur PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsBlur PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Procedural generators are kept in a small dedicated pack so generator
# changes do not widen the remaining spatial-effects target.
set(ARTIFACT_EFFECTS_GENERATE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/SimpleRainEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/RadioWavesEffect.ixx"
)
set(ARTIFACT_EFFECTS_GENERATE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/SimpleRainEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/RadioWavesEffect.cppm"
)

add_library(ArtifactEffectsGenerate STATIC)
set_target_properties(ArtifactEffectsGenerate PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsGenerate
    PRIVATE ${ARTIFACT_EFFECTS_GENERATE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_GENERATE_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsGenerate PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsGenerate PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsGenerate PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsGenerate PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Stateless distortion operators share the same image/property/parallel
# boundary and are isolated from the remaining spatial-effects pack.
set(ARTIFACT_EFFECTS_DISTORT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/DisplacementMapEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/TurbulentDisplace/TurbulentDisplaceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/TimeDisplacementEffect.ixx"
)
set(ARTIFACT_EFFECTS_DISTORT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DisplacementMap/DisplacementMapEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/TimeDisplacement/TimeDisplacementEffect.cppm"
)

add_library(ArtifactEffectsDistort STATIC)
set_target_properties(ArtifactEffectsDistort PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsDistort
    PRIVATE ${ARTIFACT_EFFECTS_DISTORT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_DISTORT_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsDistort PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsDistort PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsDistort PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsDistort PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Stylize operators share the same image/property/GPU-compute boundary and
# are isolated from the remaining spatial-effects pack.
set(ARTIFACT_EFFECTS_STYLIZE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kaleidoscope/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Dithering/DitheringEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kuwahara/KuwaharaEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Bevel/BevelEffect.ixx"
)
set(ARTIFACT_EFFECTS_STYLIZE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Dithering/DitheringEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kuwahara/KuwaharaEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Bevel/BevelEffect.cppm"
)

add_library(ArtifactEffectsStylize STATIC)
set_target_properties(ArtifactEffectsStylize PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsStylize
    PRIVATE ${ARTIFACT_EFFECTS_STYLIZE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_STYLIZE_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsStylize PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsStylize PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsStylize PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsStylize PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Glow and directional-light operators share the same image/property and
# optional GPU-compute boundary, so keep the complete glow family together.
set(ARTIFACT_EFFECTS_GLOW_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/DirectionalGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/EdgeBloomEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ChromaticGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ReactiveGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LiquidGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ResidualGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/PhysicalHalationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LuminescenceCausticsEffect.ixx"
)
set(ARTIFACT_EFFECTS_GLOW_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DirectionalGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/EdgeBloomEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ChromaticGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ReactiveGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LiquidGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ResidualGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/PhysicalHalationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LuminescenceCausticsEffect.cppm"
)

add_library(ArtifactEffectsGlow STATIC)
set_target_properties(ArtifactEffectsGlow PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsGlow
    PRIVATE ${ARTIFACT_EFFECTS_GLOW_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_GLOW_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsGlow PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsGlow PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsGlow PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsGlow PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Lens distortion and optics compensation share the distortion-processing
# boundary even though their current source entries live in different packs.
set(ARTIFACT_EFFECTS_OPTICS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LensDistortion/LensDistortionEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/OpticsCompensation/OpticsCompensationEffect.ixx"
)
set(ARTIFACT_EFFECTS_OPTICS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LensDistortion/LensDistortionEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm"
)

add_library(ArtifactEffectsOptics STATIC)
set_target_properties(ArtifactEffectsOptics PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsOptics
    PRIVATE ${ARTIFACT_EFFECTS_OPTICS_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_OPTICS_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsOptics PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsOptics PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsOptics PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsOptics PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Wave distortion is kept as a focused pack because it has a distinct
# procedural operator contract and GPU-compute implementation.
set(ARTIFACT_EFFECTS_WAVE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Wave/WaveEffect.ixx"
)
set(ARTIFACT_EFFECTS_WAVE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Wave/WaveEffect.cppm"
)

add_library(ArtifactEffectsWave STATIC)
set_target_properties(ArtifactEffectsWave PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsWave
    PRIVATE ${ARTIFACT_EFFECTS_WAVE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_WAVE_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsWave PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsWave PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsWave PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsWave PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Remaining image filters share the common GPU operator boundary and are kept
# together so the residual Spatial target stays focused.
set(ARTIFACT_EFFECTS_FILTERS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LinearWipe/LinearWipeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Liquify/LiquifyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Mosaic/MosaicEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Spherize/SpherizeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Sharpen/SharpenEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/FindEdges/FindEdgesEffect.ixx"
)
set(ARTIFACT_EFFECTS_FILTERS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LinearWipe/LinearWipeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Liquify/LiquifyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Mosaic/MosaicEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Spherize/SpherizeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Sharpen/SharpenEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/FindEdges/FindEdgesEffect.cppm"
)

add_library(ArtifactEffectsFilters STATIC)
set_target_properties(ArtifactEffectsFilters PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsFilters
    PRIVATE ${ARTIFACT_EFFECTS_FILTERS_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_FILTERS_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsFilters PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsFilters PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsFilters PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsFilters PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# AddNoise is kept as a focused GPU/image operator pack.
set(ARTIFACT_EFFECTS_NOISE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AddNoise/AddNoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Noise/NoiseEffect.ixx"
)
set(ARTIFACT_EFFECTS_NOISE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AddNoise/AddNoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/NoiseEffect.cppm"
)

add_library(ArtifactEffectsNoise STATIC)
set_target_properties(ArtifactEffectsNoise PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsNoise
    PRIVATE ${ARTIFACT_EFFECTS_NOISE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_NOISE_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsNoise PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsNoise PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsNoise PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsNoise PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# SurfaceFX has a distinct graphics-surface contract and remains separate from
# the generic image-effect packs.
set(ARTIFACT_EFFECTS_SURFACEFX_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/SurfaceFX/SurfaceFXEffect.ixx"
)
set(ARTIFACT_EFFECTS_SURFACEFX_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/SurfaceFX/SurfaceFXEffect.cppm"
)

add_library(ArtifactEffectsSurfaceFX STATIC)
set_target_properties(ArtifactEffectsSurfaceFX PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsSurfaceFX
    PRIVATE ${ARTIFACT_EFFECTS_SURFACEFX_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_SURFACEFX_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsSurfaceFX PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsSurfaceFX PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsSurfaceFX PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsSurfaceFX PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# AutoMosaic has a distinct face-detection/CV boundary, so keep it outside the
# generic spatial residual pack.
set(ARTIFACT_EFFECTS_AUTO_MOSAIC_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AutoMosaicEffect.ixx"
)
set(ARTIFACT_EFFECTS_AUTO_MOSAIC_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AutoMosaicEffect.cppm"
)

add_library(ArtifactEffectsAutoMosaic STATIC)
set_target_properties(ArtifactEffectsAutoMosaic PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsAutoMosaic
    PRIVATE ${ARTIFACT_EFFECTS_AUTO_MOSAIC_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_AUTO_MOSAIC_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsAutoMosaic PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsAutoMosaic PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsAutoMosaic PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsAutoMosaic PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Motion/flow rasterizers share Effect.Context and image-history semantics;
# isolate them from the larger stateless rasterizer pack.
set(ARTIFACT_EFFECTS_MOTION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/OpticalFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorFlowGlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/LightTrailsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/MotionTrailEffect.ixx"
)
set(ARTIFACT_EFFECTS_MOTION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/OpticalFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/LightTrailsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/MotionTrailEffect.cppm"
)

add_library(ArtifactEffectsMotion STATIC)
set_target_properties(ArtifactEffectsMotion PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsMotion
    PRIVATE ${ARTIFACT_EFFECTS_MOTION_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_MOTION_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsMotion PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsMotion PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsMotion PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsMotion PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Digital artifact operators share the frame-context/image boundary and are
# isolated from the broader rasterizer pack.
set(ARTIFACT_EFFECTS_DIGITAL_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DataMoshEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FilmDamageEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DeflickerEffect.ixx"
)
set(ARTIFACT_EFFECTS_DIGITAL_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DataMoshEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FilmDamageEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DeflickerEffect.cppm"
)

add_library(ArtifactEffectsDigital STATIC)
set_target_properties(ArtifactEffectsDigital PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsDigital
    PRIVATE ${ARTIFACT_EFFECTS_DIGITAL_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_DIGITAL_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsDigital PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsDigital PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsDigital PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsDigital PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Pattern generators form a cohesive image/Property pack independent of the
# remaining rasterizer operators.
set(ARTIFACT_EFFECTS_PATTERNS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/BricksEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HexGridEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HalftoneEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StripesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VoronoiEffect.ixx"
)
set(ARTIFACT_EFFECTS_PATTERNS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/BricksEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HexGridEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HalftoneEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/StripesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VoronoiEffect.cppm"
)

add_library(ArtifactEffectsPatterns STATIC)
set_target_properties(ArtifactEffectsPatterns PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsPatterns
    PRIVATE ${ARTIFACT_EFFECTS_PATTERNS_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_PATTERNS_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsPatterns PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsPatterns PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsPatterns PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsPatterns PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Chromatic operators share the image/color-aberration boundary and are kept
# separate from the remaining rasterizer effects.
set(ARTIFACT_EFFECTS_CHROMATIC_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticAberrationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticReliefEffect.ixx"
)
set(ARTIFACT_EFFECTS_CHROMATIC_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticReliefEffect.cppm"
)

add_library(ArtifactEffectsChromatic STATIC)
set_target_properties(ArtifactEffectsChromatic PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsChromatic
    PRIVATE ${ARTIFACT_EFFECTS_CHROMATIC_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_CHROMATIC_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsChromatic PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsChromatic PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsChromatic PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsChromatic PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Shadow operators share the same image/Property boundary and are kept in a
# focused pack.
set(ARTIFACT_EFFECTS_SHADOWS_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DropShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/InnerShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/RadialShadow/RadialShadowEffect.ixx"
)
set(ARTIFACT_EFFECTS_SHADOWS_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DropShadow/DropShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/InnerShadow/InnerShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/RadialShadow/RadialShadowEffect.cppm"
)

add_library(ArtifactEffectsShadows STATIC)
set_target_properties(ArtifactEffectsShadows PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsShadows
    PRIVATE ${ARTIFACT_EFFECTS_SHADOWS_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_SHADOWS_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsShadows PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsShadows PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsShadows PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsShadows PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Context-aware raster operators share the frame-context/image boundary and
# are isolated from the stateless rasterizer remainder.
set(ARTIFACT_EFFECTS_CONTEXTUAL_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DifferenceMatteEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/EdgeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GhostEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PixelSortEffect.ixx"
)
set(ARTIFACT_EFFECTS_CONTEXTUAL_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DifferenceMatteEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/EdgeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GhostEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PixelSortEffect.cppm"
)

add_library(ArtifactEffectsContextual STATIC)
set_target_properties(ArtifactEffectsContextual PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsContextual
    PRIVATE ${ARTIFACT_EFFECTS_CONTEXTUAL_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_CONTEXTUAL_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsContextual PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsContextual PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsContextual PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsContextual PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Time-aware raster operators share Effect.Context and frame sampling
# semantics, so isolate them from the stateless rasterizer remainder.
set(ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PosterizeTimeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ScreenShakeEffect.ixx"
)
set(ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PosterizeTimeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ScreenShakeEffect.cppm"
)

add_library(ArtifactEffectsTemporalContext STATIC)
set_target_properties(ArtifactEffectsTemporalContext PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsTemporalContext
    PRIVATE ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsTemporalContext PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsTemporalContext PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsTemporalContext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsTemporalContext PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Final image-finishing operators share the common image/Property/GPU boundary
# and are isolated from the rasterizer residual target.
set(ARTIFACT_EFFECTS_FINISHING_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/RadialBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/SatinEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StrokeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VignetteEffect.ixx"
)
set(ARTIFACT_EFFECTS_FINISHING_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/RadialBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Satin/SatinEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Stroke/StrokeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VignetteEffect.cppm"
)

add_library(ArtifactEffectsFinishing STATIC)
set_target_properties(ArtifactEffectsFinishing PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Artifact/Libraries"
)
target_sources(ArtifactEffectsFinishing
    PRIVATE ${ARTIFACT_EFFECTS_FINISHING_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_EFFECTS_FINISHING_MODULES}
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactEffectsFinishing PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactEffectsFinishing PROPERTY CXX_MODULE_STD OFF)
endif()
target_include_directories(ArtifactEffectsFinishing PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
    "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
)
target_link_libraries(ArtifactEffectsFinishing PUBLIC ArtifactCore ArtifactCoreAudio ArtifactRender ArtifactEffectContract)

# Spatial effects are independent per-frame operators.  Keep this pack separate
# from color and history-dependent effects so new spatial effects do not widen
# the largest remaining effect target.
set(ARTIFACT_EFFECTS_SPATIAL_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/DirectionalGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/EdgeBloomEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ChromaticGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ReactiveGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LiquidGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/ResidualGlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/PhysicalHalationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/LuminescenceCausticsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/LumaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/ChromaKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/DifferenceKeyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Keying/IBKKeyerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/AnisotropicFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ApertureShapeBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Blur/ReactionDiffusionBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kaleidoscope/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Dithering/DitheringEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Kuwahara/KuwaharaEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Bevel/BevelEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LinearWipe/LinearWipeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Liquify/LiquifyEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Mosaic/MosaicEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Spherize/SpherizeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Sharpen/SharpenEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/FindEdges/FindEdgesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Distort/DisplacementMapEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/TurbulentDisplace/TurbulentDisplaceEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/SimpleRainEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Generate/RadioWavesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/LensDistortion/LensDistortionEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Wave/WaveEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AddNoise/AddNoiseEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/AutoMosaicEffect.ixx"
)
set(ARTIFACT_EFFECTS_SPATIAL_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DirectionalGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/EdgeBloomEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ChromaticGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ReactiveGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LiquidGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/ResidualGlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/PhysicalHalationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/LuminescenceCausticsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/LumaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/ChromaKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/DifferenceKeyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Keying/IBKKeyerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/AnisotropicFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ApertureShapeBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Blur/ReactionDiffusionBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kaleidoscope/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Dithering/DitheringEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Kuwahara/KuwaharaEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Bevel/BevelEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LinearWipe/LinearWipeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Liquify/LiquifyEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Mosaic/MosaicEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Spherize/SpherizeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Sharpen/SharpenEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/FindEdges/FindEdgesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DisplacementMap/DisplacementMapEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/SimpleRainEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Generate/RadioWavesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/LensDistortion/LensDistortionEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Wave/WaveEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AddNoise/AddNoiseEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/AutoMosaicEffect.cppm"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_KEYING_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_KEYING_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_BLUR_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_BLUR_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_GENERATE_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_GENERATE_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_DISTORT_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_DISTORT_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_STYLIZE_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_STYLIZE_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_GLOW_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_GLOW_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_OPTICS_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_OPTICS_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES ${ARTIFACT_EFFECTS_OPTICS_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL ${ARTIFACT_EFFECTS_OPTICS_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_OPTICS_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_OPTICS_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_WAVE_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_WAVE_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_FILTERS_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_FILTERS_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_NOISE_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_NOISE_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_MODULES ${ARTIFACT_EFFECTS_AUTO_MOSAIC_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_SPATIAL_IMPL ${ARTIFACT_EFFECTS_AUTO_MOSAIC_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES ${ARTIFACT_EFFECTS_SPATIAL_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL ${ARTIFACT_EFFECTS_SPATIAL_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_SPATIAL_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_SPATIAL_IMPL})

add_library(ArtifactEffectsSpatial INTERFACE)
set_target_properties(ArtifactEffectsSpatial PROPERTIES FOLDER "Artifact/Libraries")
target_link_libraries(ArtifactEffectsSpatial INTERFACE
    ArtifactEffectsKeying
    ArtifactEffectsBlur
    ArtifactEffectsGenerate
    ArtifactEffectsDistort
    ArtifactEffectsStylize
    ArtifactEffectsGlow
    ArtifactEffectsOptics
    ArtifactEffectsWave
    ArtifactEffectsFilters
    ArtifactEffectsNoise
    ArtifactEffectsAutoMosaic
)

# The remaining Rasterizer operators are independent per-frame effects.  Keep
# history-dependent operators in ArtifactEffectsTemporal and isolate this
# stateless raster pack from the small residual effect target below.
set(ARTIFACT_EFFECTS_RASTERIZER_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/BricksEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticAberrationEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ChromaticReliefEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DataMoshEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DeflickerEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DifferenceMatteEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/DropShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/EdgeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/FilmDamageEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GhostEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HalftoneEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/HexGridEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/InnerShadowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/LightTrailsEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/MotionTrailEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/OpticalFlowBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PixelSortEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/PosterizeTimeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/RadialBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/SatinEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/ScreenShakeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StripesEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/StrokeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorBlurEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VectorFlowGlitchEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VignetteEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/VoronoiEffect.ixx"
)
set(ARTIFACT_EFFECTS_RASTERIZER_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/BricksEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ChromaticReliefEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DataMoshEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DeflickerEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/DifferenceMatteEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/DropShadow/DropShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/EdgeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/FilmDamageEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GhostEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HalftoneEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/HexGridEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/InnerShadow/InnerShadowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/LightTrailsEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/MotionTrailEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/OpticalFlowBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PixelSortEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/PosterizeTimeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/RadialBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Satin/SatinEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/ScreenShakeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/StripesEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Stroke/StrokeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorBlurEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VectorFlowGlitchEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VignetteEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/VoronoiEffect.cppm"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_MOTION_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_MOTION_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_DIGITAL_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_DIGITAL_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_PATTERNS_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_PATTERNS_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_CHROMATIC_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_CHROMATIC_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_SHADOWS_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_SHADOWS_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_CONTEXTUAL_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_CONTEXTUAL_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_IMPL})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES ${ARTIFACT_EFFECTS_FINISHING_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL ${ARTIFACT_EFFECTS_FINISHING_IMPL})
# These legacy Rasterizer-path units export the same module names as the
# canonical Glow/Kaleidoscope/RadialBlur units already owned by their focused
# packs.
# Keep the files for history, but do not compile a second definition.
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/RadialBlurEffect.ixx"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_RASTERIZER_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/RadialBlurEffect.cppm"
)
# Remove every focused pack explicitly from the residual lists.  The source
# family variables above are narrowed before the generic Spatial/Rasterizer
# cleanup, so relying on those mutated variables here would leave split sources
# in ArtifactEffectsResidual as well.
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES
    ${ARTIFACT_EFFECTS_KEYING_MODULES}
    ${ARTIFACT_EFFECTS_BLUR_MODULES}
    ${ARTIFACT_EFFECTS_GENERATE_MODULES}
    ${ARTIFACT_EFFECTS_DISTORT_MODULES}
    ${ARTIFACT_EFFECTS_STYLIZE_MODULES}
    ${ARTIFACT_EFFECTS_GLOW_MODULES}
    ${ARTIFACT_EFFECTS_OPTICS_MODULES}
    ${ARTIFACT_EFFECTS_WAVE_MODULES}
    ${ARTIFACT_EFFECTS_FILTERS_MODULES}
    ${ARTIFACT_EFFECTS_NOISE_MODULES}
    ${ARTIFACT_EFFECTS_AUTO_MOSAIC_MODULES}
    ${ARTIFACT_EFFECTS_MOTION_MODULES}
    ${ARTIFACT_EFFECTS_DIGITAL_MODULES}
    ${ARTIFACT_EFFECTS_PATTERNS_MODULES}
    ${ARTIFACT_EFFECTS_CHROMATIC_MODULES}
    ${ARTIFACT_EFFECTS_SHADOWS_MODULES}
    ${ARTIFACT_EFFECTS_CONTEXTUAL_MODULES}
    ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_MODULES}
    ${ARTIFACT_EFFECTS_FINISHING_MODULES}
    ${ARTIFACT_EFFECTS_SURFACEFX_MODULES}
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/GlowEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/KaleidoscopeEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Rasterizer/RadialBlurEffect.ixx"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL
    ${ARTIFACT_EFFECTS_KEYING_IMPL}
    ${ARTIFACT_EFFECTS_BLUR_IMPL}
    ${ARTIFACT_EFFECTS_GENERATE_IMPL}
    ${ARTIFACT_EFFECTS_DISTORT_IMPL}
    ${ARTIFACT_EFFECTS_STYLIZE_IMPL}
    ${ARTIFACT_EFFECTS_GLOW_IMPL}
    ${ARTIFACT_EFFECTS_OPTICS_IMPL}
    ${ARTIFACT_EFFECTS_WAVE_IMPL}
    ${ARTIFACT_EFFECTS_FILTERS_IMPL}
    ${ARTIFACT_EFFECTS_NOISE_IMPL}
    ${ARTIFACT_EFFECTS_AUTO_MOSAIC_IMPL}
    ${ARTIFACT_EFFECTS_MOTION_IMPL}
    ${ARTIFACT_EFFECTS_DIGITAL_IMPL}
    ${ARTIFACT_EFFECTS_PATTERNS_IMPL}
    ${ARTIFACT_EFFECTS_CHROMATIC_IMPL}
    ${ARTIFACT_EFFECTS_SHADOWS_IMPL}
    ${ARTIFACT_EFFECTS_CONTEXTUAL_IMPL}
    ${ARTIFACT_EFFECTS_TEMPORAL_CONTEXT_IMPL}
    ${ARTIFACT_EFFECTS_FINISHING_IMPL}
    ${ARTIFACT_EFFECTS_SURFACEFX_IMPL}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/GlowEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/KaleidoscopeEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Rasterizer/RadialBlurEffect.cppm"
)
list(REMOVE_ITEM ARTIFACT_EFFECTS_MODULES ${ARTIFACT_EFFECTS_RASTERIZER_MODULES})
list(REMOVE_ITEM ARTIFACT_EFFECTS_IMPL ${ARTIFACT_EFFECTS_RASTERIZER_IMPL})
list(REMOVE_ITEM APP_MODULES ${ARTIFACT_EFFECTS_RASTERIZER_MODULES})
list(REMOVE_ITEM APP_IMPL ${ARTIFACT_EFFECTS_RASTERIZER_IMPL})

add_library(ArtifactEffectsRasterizer INTERFACE)
set_target_properties(ArtifactEffectsRasterizer PROPERTIES FOLDER "Artifact/Libraries")
target_link_libraries(ArtifactEffectsRasterizer INTERFACE
    ArtifactEffectsMotion
    ArtifactEffectsDigital
    ArtifactEffectsPatterns
    ArtifactEffectsChromatic
    ArtifactEffectsShadows
    ArtifactEffectsContextual
    ArtifactEffectsTemporalContext
    ArtifactEffectsFinishing
)

# ArtifactGlobalEffectManager and the OFX host/implementation remain in the
# application target: they depend on application plugin/service modules, so
# moving them into ArtifactEffects would reverse the dependency direction.

add_library(ArtifactEffectsResidual INTERFACE)
set_target_properties(ArtifactEffectsResidual PROPERTIES FOLDER "Artifact/Libraries")
target_link_libraries(ArtifactEffectsResidual INTERFACE
    ArtifactEffectsColor
    ArtifactEffectsTemporal
    ArtifactEffectsKeying
    ArtifactEffectsBlur
    ArtifactEffectsGenerate
    ArtifactEffectsDistort
    ArtifactEffectsStylize
    ArtifactEffectsGlow
    ArtifactEffectsOptics
    ArtifactEffectsWave
    ArtifactEffectsFilters
    ArtifactEffectsNoise
    ArtifactEffectsAutoMosaic
    ArtifactEffectsMotion
    ArtifactEffectsDigital
    ArtifactEffectsPatterns
    ArtifactEffectsChromatic
    ArtifactEffectsShadows
    ArtifactEffectsContextual
    ArtifactEffectsTemporalContext
    ArtifactEffectsFinishing
    ArtifactEffectsSurfaceFX
)

add_library(ArtifactEffects ALIAS ArtifactEffectsResidual)

add_executable(Artifact WIN32)

set_target_properties(Artifact PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# Build metadata is consumed only by the startup and application-service units.
# Keeping it target-wide turns every CMake reconfigure (and its timestamp) into
# an avoidable full rebuild of Artifact.
option(ARTIFACT_FEATURE_COMMAND_PALETTE "Enable experimental command palette widget (shell only, no real action registration yet)" OFF)
option(ARTIFACT_FEATURE_FARM_WORKER "Build out-of-process render farm worker binary (Phase 4)" OFF)

set(ARTIFACT_APPMAIN_BUILD_METADATA_DEFINITIONS
    ARTIFACT_VERSION_STRING="${ARTIFACT_VERSION}"
    ARTIFACT_BUILD_GIT_HASH="${ARTIFACT_BUILD_GIT_HASH}"
    ARTIFACT_BUILD_TIMESTAMP="${ARTIFACT_BUILD_TIMESTAMP}"
    ARTIFACT_BUILD_DIRTY="${ARTIFACT_BUILD_DIRTY}"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AppMain.cppm"
    PROPERTIES COMPILE_DEFINITIONS "${ARTIFACT_APPMAIN_BUILD_METADATA_DEFINITIONS}"
)
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Service/ApplicationService.cppm"
    PROPERTIES COMPILE_DEFINITIONS "ARTIFACT_VERSION_STRING=\"${ARTIFACT_VERSION}\""
)

if(ARTIFACT_FEATURE_COMMAND_PALETTE)
    target_compile_definitions(Artifact PRIVATE ARTIFACT_FEATURE_COMMAND_PALETTE=1)
endif()

if(TARGET __CMAKE::CXX23)
    set_property(TARGET Artifact PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET Artifact PROPERTY CXX_MODULE_STD OFF)
endif()

target_sources(Artifact
    PRIVATE ${APP_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${APP_MODULES}
)

# Strict MSVC-cl only flags
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(Artifact PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_definitions(Artifact PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        GLOG_USE_GLOG_EXPORT
        _WIN32_WINNT=0x0A00
    )
    target_compile_options(Artifact PRIVATE /WX-)
elseif(ARTIFACT_FRONTEND_MSVC)
    # clang-cl / icx-cl (MSVC-like frontend)
    target_compile_options(Artifact PRIVATE /Zc:__cplusplus /utf-8 /WX- /wd5050)
    target_compile_definitions(Artifact PRIVATE 
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        GLOG_USE_GLOG_EXPORT
        _WIN32_WINNT=0x0A00
    )
endif()

# MSVC library-resolution tracing is useful for link diagnosis but adds noisy
# linker work to every ordinary incremental build. Keep it opt-in.
option(ARTIFACT_ENABLE_LINK_LIBRARY_TRACE "Enable MSVC linker library-resolution diagnostics" OFF)
option(ARTIFACT_ENABLE_LTCG "Enable link-time code generation for shipping builds" OFF)
if(ARTIFACT_FRONTEND_MSVC)
    # The monolithic Debug executable exceeds the MSVC linker PDB limit.
    # The installed MSVC toolchain no longer supports FASTLINK, so omit the
    # executable PDB while keeping embedded debug information in object files.
    target_link_options(Artifact PRIVATE
        "$<$<CONFIG:Debug>:/PDB:NONE>"
    )
endif()
if(ARTIFACT_ENABLE_LINK_LIBRARY_TRACE)
    target_link_options(Artifact PRIVATE "$<$<BOOL:${ARTIFACT_FRONTEND_MSVC}>:/VERBOSE:LIB>")
endif()
if(ARTIFACT_FRONTEND_MSVC AND ARTIFACT_ENABLE_LTCG)
    target_link_options(Artifact PRIVATE
        "$<$<NOT:$<CONFIG:Debug>>:/LTCG>"
    )
endif()

target_include_directories(Artifact
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentTools/ThirdParty/imgui"
)

# ---------------------------------------------------------------------------
# 依存関係
# ---------------------------------------------------------------------------

# 内部ライブラリ
target_link_libraries(Artifact PRIVATE ArtifactCore ArtifactCoreAudio ArtifactCoreAI ArtifactCoreVideo ArtifactCoreMedia ArtifactCoreComposition ArtifactCoreScene ArtifactCoreMaterial ArtifactCoreUI ArtifactCoreData ArtifactCoreCommand ArtifactCoreAsset ArtifactCoreShape ArtifactCoreText ArtifactCoreGenerate ArtifactCoreSimulation ArtifactCoreTrack ArtifactCoreSource ArtifactCoreProject ArtifactCoreRig ArtifactCoreGrid ArtifactCoreColorCollection ArtifactCoreSound ArtifactCoreSequence ArtifactCoreAnalyze ArtifactCoreTracking ArtifactCoreIPC ArtifactCoreNetwork ArtifactCoreCollaborate ArtifactCoreNLE ArtifactCorePlayback ArtifactCorePreview ArtifactCoreExport ArtifactCoreVST3 ArtifactCoreLocalization ArtifactCoreCoordinate ArtifactCoreEvent ArtifactCoreFile ArtifactCorePlugin ArtifactCoreControl ArtifactCoreDatabase ArtifactCoreMask ArtifactCoreConfiguration ArtifactCoreEnvironment ArtifactCorePlatform ArtifactCoreAcoustic ArtifactCoreLight ArtifactCoreCrowd ArtifactCoreDomain ArtifactCoreFileSystem ArtifactCoreIcon ArtifactCoreVST ArtifactColorPalette ArtifactColorNode ArtifactColorSettings ArtifactColorScience ArtifactColorManagement ArtifactColorGrading ArtifactEffectsResidual ArtifactWidgets ArtifactAsset ArtifactColor ArtifactRenderSupport ArtifactRenderSupportContracts ArtifactRender ArtifactEffectContract ArtifactEffectsColor ArtifactEffectsTemporal ArtifactEffectsSpatial ArtifactEffectsKeying ArtifactEffectsBlur ArtifactEffectsGenerate ArtifactEffectsDistort ArtifactEffectsRasterizer)

# DiligentEngine
set(DILIGENT_LIBS "")
set(DILIGENT_COMPILE_DEFINITIONS "")
if(TARGET Diligent-GraphicsEngineD3D11-static)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3D11-static)
elseif(TARGET Diligent-GraphicsEngineD3D11-shared)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3D11-shared)
endif()
if(TARGET Diligent-GraphicsEngineD3D12-static)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3D12-static)
elseif(TARGET Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_COMPILE_DEFINITIONS DILIGENT_D3D12_SHARED=1)
endif()
if(TARGET Diligent-GraphicsEngineVk-static)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineVk-static)
elseif(TARGET Diligent-GraphicsEngineVk-shared)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineVk-shared)
    list(APPEND DILIGENT_COMPILE_DEFINITIONS DILIGENT_VK_SHARED=1)
endif()
if(TARGET Diligent-GraphicsEngineD3DBase)
    list(APPEND DILIGENT_LIBS Diligent-GraphicsEngineD3DBase)
endif()
if(TARGET Diligent-Common)
    list(APPEND DILIGENT_LIBS Diligent-Common)
endif()
target_link_libraries(Artifact PRIVATE ArtifactEffectsStylize)
target_link_libraries(Artifact PRIVATE ${DILIGENT_LIBS})
target_link_libraries(Artifact PRIVATE ArtifactEffectsOptics)
target_link_libraries(Artifact PRIVATE ArtifactEffectsWave)
target_link_libraries(Artifact PRIVATE ArtifactEffectsFilters)
target_link_libraries(Artifact PRIVATE ArtifactEffectsNoise)
target_link_libraries(Artifact PRIVATE ArtifactEffectsAutoMosaic)
target_link_libraries(Artifact PRIVATE ArtifactEffectsMotion)
target_link_libraries(Artifact PRIVATE ArtifactEffectsDigital)
target_link_libraries(Artifact PRIVATE ArtifactEffectsPatterns)
target_link_libraries(Artifact PRIVATE ArtifactEffectsChromatic)
target_link_libraries(Artifact PRIVATE ArtifactEffectsShadows)
target_link_libraries(Artifact PRIVATE ArtifactEffectsContextual)
target_link_libraries(Artifact PRIVATE ArtifactEffectsTemporalContext)
target_link_libraries(Artifact PRIVATE ArtifactEffectsFinishing)
target_link_libraries(Artifact PRIVATE ArtifactEffectsSurfaceFX)
target_link_libraries(Artifact PRIVATE ArtifactEffectsGlow)
target_link_libraries(ArtifactRender PRIVATE ${DILIGENT_LIBS})
if(DILIGENT_COMPILE_DEFINITIONS)
    list(REMOVE_DUPLICATES DILIGENT_COMPILE_DEFINITIONS)
    target_compile_definitions(Artifact PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactRender PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectContract PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectsColor PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectsTemporal PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectsSpatial INTERFACE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectsRasterizer PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
    target_compile_definitions(ArtifactEffectsResidual PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
endif()

if(WIN32)
    target_compile_definitions(ArtifactEffectContract PRIVATE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
    target_compile_definitions(ArtifactEffectsColor PRIVATE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
    target_compile_definitions(ArtifactEffectsTemporal PRIVATE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
    target_compile_definitions(ArtifactEffectsSpatial INTERFACE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
    target_compile_definitions(ArtifactEffectsRasterizer INTERFACE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
    target_compile_definitions(ArtifactEffectsResidual INTERFACE
        PLATFORM_WIN32
        _WIN32_WINNT=0x0A00
    )
endif()
if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactEffectContract PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_options(ArtifactEffectsColor PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_options(ArtifactEffectsTemporal PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_options(ArtifactEffectsSpatial INTERFACE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_options(ArtifactEffectsRasterizer INTERFACE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
    target_compile_options(ArtifactEffectsResidual INTERFACE /Zf /Zc:__cplusplus /utf-8 /openmp /wd5050)
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactEffectContract PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_options(ArtifactEffectsColor PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_options(ArtifactEffectsTemporal PRIVATE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_options(ArtifactEffectsSpatial INTERFACE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_options(ArtifactEffectsRasterizer INTERFACE /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_options(ArtifactEffectsResidual INTERFACE /Zc:__cplusplus /utf-8 /wd5050)
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets Concurrent Svg)
target_link_libraries(Artifact PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Concurrent Qt6::Svg)
target_link_libraries(ArtifactAsset PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_link_libraries(ArtifactRenderSupport PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent)
target_link_libraries(ArtifactRender PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)

# OpenAssetIO is an optional interoperability layer for external asset
# management systems. The local filesystem asset workflow remains the default.
find_package(OpenAssetIO CONFIG QUIET)
if(TARGET openassetio::openassetio-core)
    target_link_libraries(Artifact PRIVATE openassetio::openassetio-core)
    target_compile_definitions(Artifact PRIVATE ARTIFACT_HAS_OPENASSETIO=1)
elseif(TARGET OpenAssetIO::openassetio-core)
    target_link_libraries(Artifact PRIVATE OpenAssetIO::openassetio-core)
    target_compile_definitions(Artifact PRIVATE ARTIFACT_HAS_OPENASSETIO=1)
endif()

# Qt resource bundle: app icons
file(GLOB_RECURSE ARTIFACT_ICON_RESOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/App/Icon/*"
)
list(FILTER ARTIFACT_ICON_RESOURCE_FILES INCLUDE REGEX "\\.(svg|png|json)$")
list(FILTER ARTIFACT_ICON_RESOURCE_FILES EXCLUDE REGEX "\\.crdownload$")
list(FILTER ARTIFACT_ICON_RESOURCE_FILES EXCLUDE REGEX "desktop\\.ini$")
if(ARTIFACT_ICON_RESOURCE_FILES)
    qt_add_resources(Artifact "ArtifactIconResources"
        PREFIX "/icons"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/App/Icon"
        FILES ${ARTIFACT_ICON_RESOURCE_FILES}
    )
endif()

# OpenCV
find_package(OpenCV REQUIRED)
target_link_libraries(Artifact PRIVATE ${OpenCV_LIBS})

# OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(Artifact PRIVATE OpenMP::OpenMP_CXX)

find_package(TBB REQUIRED)
target_link_libraries(Artifact PRIVATE TBB::tbb)

if(NOT TARGET ads::qtadvanceddocking-qt6)
    find_package(qtadvanceddocking-qt6 REQUIRED)
endif()
target_link_libraries(Artifact PRIVATE ads::qtadvanceddocking-qt6)

find_package(glm CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE glm::glm)

find_package(Stb REQUIRED)
target_include_directories(Artifact PRIVATE ${Stb_INCLUDE_DIR})

find_package(EnTT CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE EnTT::EnTT)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE magic_enum::magic_enum)

find_package(Boost REQUIRED COMPONENTS iostreams)
target_link_libraries(Artifact PRIVATE Boost::iostreams)

# FFmpeg
find_package(FFMPEG REQUIRED)
if(FFMPEG_FOUND)
    target_include_directories(Artifact PRIVATE ${FFMPEG_INCLUDE_DIRS})
    if(FFMPEG_LIBRARY_DIRS)
        target_link_directories(Artifact PRIVATE ${FFMPEG_LIBRARY_DIRS})
    endif()
    target_link_libraries(Artifact PRIVATE ${FFMPEG_LIBRARIES})
    # Some FFmpeg package configurations omit avutil even though codec/format
    # objects expose helpers such as av_make_error_string that reference it.
    # Keep the final Artifact link self-contained instead of relying only on
    # transitive ArtifactCore linkage.
    target_link_libraries(Artifact PRIVATE avutil)
else()
    # Fallback for manual specification
    target_link_libraries(Artifact PRIVATE 
        "${VCPKG_INSTALLED}/lib/avcodec.lib"
        "${VCPKG_INSTALLED}/lib/avformat.lib"
        "${VCPKG_INSTALLED}/lib/avutil.lib"
        "${VCPKG_INSTALLED}/lib/swscale.lib"
        "${VCPKG_INSTALLED}/lib/swresample.lib"
    )
    target_include_directories(Artifact PRIVATE "${VCPKG_INSTALLED}/include")
endif()

artifact_enable_delay_load(Artifact
    DEBUG_DLLS
        opencv_core4d.dll
        opencv_calib3d4d.dll
        opencv_dnn4d.dll
        opencv_features2d4d.dll
        opencv_flann4d.dll
        opencv_gapi4d.dll
        opencv_highgui4d.dll
        opencv_imgcodecs4d.dll
        opencv_imgproc4d.dll
        opencv_ml4d.dll
        opencv_objdetect4d.dll
        opencv_photo4d.dll
        opencv_stitching4d.dll
        opencv_video4d.dll
        opencv_videoio4d.dll
        tbb12_debug.dll
        tbbmalloc_debug.dll
        tbbbind_2_5_debug.dll
        avcodec-62.dll
        avformat-62.dll
        avutil-60.dll
        swscale-9.dll
        swresample-6.dll
    RELEASE_DLLS
        opencv_core4.dll
        opencv_calib3d4.dll
        opencv_dnn4.dll
        opencv_features2d4.dll
        opencv_flann4.dll
        opencv_gapi4.dll
        opencv_highgui4.dll
        opencv_imgcodecs4.dll
        opencv_imgproc4.dll
        opencv_ml4.dll
        opencv_objdetect4.dll
        opencv_photo4.dll
        opencv_stitching4.dll
        opencv_video4.dll
        opencv_videoio4.dll
        tbb12.dll
        tbbmalloc.dll
        tbbbind_2_5.dll
        avcodec-62.dll
        avformat-62.dll
        avutil-60.dll
        swscale-9.dll
        swresample-6.dll
)

# Windows 固有
if(WIN32)
    target_link_libraries(Artifact PRIVATE
        dwmapi
        shell32
        user32
        ole32
        advapi32
    )
endif()

# DirectX 12 Agility SDK (retail 1.619.x / D3D12SDKVersion 619)
# The SDK runtime is intentionally not vendored. Set ARTIFACT_AGILITY_SDK_DIR
# to the unpacked Microsoft.Direct3D.D3D12 package directory.
if(WIN32 AND ARTIFACT_AGILITY_SDK_DIR)
    set(_ARTIFACT_AGILITY_D3D12CORE "${ARTIFACT_AGILITY_SDK_DIR}/build/native/bin/x64/D3D12Core.dll")
    if(NOT EXISTS "${_ARTIFACT_AGILITY_D3D12CORE}")
        message(FATAL_ERROR "Agility SDK D3D12Core.dll not found: ${_ARTIFACT_AGILITY_D3D12CORE}")
    endif()
    message(STATUS "Agility SDK selected: ${ARTIFACT_AGILITY_SDK_DIR}")
    message(STATUS "Agility SDK runtime: ${_ARTIFACT_AGILITY_D3D12CORE}")
    add_custom_command(TARGET Artifact POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_ARTIFACT_AGILITY_D3D12CORE}"
            "$<TARGET_FILE_DIR:Artifact>/D3D12Core.dll"
        COMMENT "Deploying DirectX 12 Agility SDK retail runtime"
        VERBATIM
    )
elseif(WIN32)
    message(STATUS "Agility SDK runtime deployment disabled; set ARTIFACT_AGILITY_SDK_DIR to enable it")
endif()

# DirectStorage is an optional Windows/DX12 fast path. The reader is compiled
# with a portable QFile fallback when the SDK is absent, so Vulkan and machines
# without the redistributable keep the same higher-level loading contract.
if(WIN32 AND ARTIFACT_ENABLE_DIRECTSTORAGE AND
   ARTIFACT_DIRECTSTORAGE_SDK_DIR)
    set(_ARTIFACT_DIRECTSTORAGE_INCLUDE
        "${ARTIFACT_DIRECTSTORAGE_SDK_DIR}/native/include")
    set(_ARTIFACT_DIRECTSTORAGE_DLL
        "${ARTIFACT_DIRECTSTORAGE_SDK_DIR}/native/bin/x64/dstorage.dll")
    if(NOT EXISTS "${_ARTIFACT_DIRECTSTORAGE_INCLUDE}/dstorage.h")
        message(FATAL_ERROR
            "DirectStorage header not found: ${_ARTIFACT_DIRECTSTORAGE_INCLUDE}/dstorage.h")
    endif()
    if(NOT EXISTS "${_ARTIFACT_DIRECTSTORAGE_DLL}")
        message(FATAL_ERROR "DirectStorage runtime not found: ${_ARTIFACT_DIRECTSTORAGE_DLL}")
    endif()
    target_include_directories(Artifact PRIVATE "${_ARTIFACT_DIRECTSTORAGE_INCLUDE}")
    add_custom_command(TARGET Artifact POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_ARTIFACT_DIRECTSTORAGE_DLL}"
            "$<TARGET_FILE_DIR:Artifact>/dstorage.dll"
        COMMENT "Deploying DirectStorage runtime"
        VERBATIM
    )
    message(STATUS "DirectStorage SDK selected: ${ARTIFACT_DIRECTSTORAGE_SDK_DIR}")
elseif(WIN32 AND ARTIFACT_ENABLE_DIRECTSTORAGE)
    message(STATUS
        "DirectStorage SDK unavailable; portable asynchronous file I/O will be used")
elseif(WIN32)
    message(STATUS "DirectStorage explicitly disabled")
endif()

# ---------------------------------------------------------------------------
# Out-of-process render farm worker (Phase 4)
# ---------------------------------------------------------------------------
if(ARTIFACT_FEATURE_FARM_WORKER)
    add_executable(ArtifactWorker)
    set_target_properties(ArtifactWorker PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    )
    target_sources(ArtifactWorker
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/Worker/FarmWorkerMain.cppm"
    )
target_link_libraries(ArtifactWorker PRIVATE ArtifactCore ArtifactCoreAudio ArtifactCoreNetwork Qt6::Core Qt6::Network)
endif()

# ---------------------------------------------------------------------------
# Qt runtime deployment (Windows)
# ---------------------------------------------------------------------------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        set(_ARTIFACT_DEPLOY_QT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunWindeployqt.cmake")
        set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED}")
        if(NOT EXISTS "${_ARTIFACT_VCPKG_INSTALLED}/debug/bin"
           AND DEFINED VCPKG_INSTALLED_DIR
           AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
            set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED_DIR}/x64-windows")
        endif()
        if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
            set(_ARTIFACT_VCPKG_INSTALLED "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        endif()
        add_custom_command(TARGET Artifact POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}"
                "-DTARGET_FILE=$<TARGET_FILE:Artifact>"
                "-DVCPKG_INSTALLED=${_ARTIFACT_VCPKG_INSTALLED}"
                "-DCONFIG=$<CONFIG>"
                -P "${_ARTIFACT_DEPLOY_QT_SCRIPT}"
            COMMENT "Deploying Qt runtime for Artifact with windeployqt"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found. Qt plugins and runtime DLLs will not be auto-deployed.")
    endif()

    set(_ARTIFACT_COPY_RUNTIME_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyVcpkgRuntimeDeps.cmake")
    set(_ARTIFACT_COPY_QT_PLATFORM_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyQtPlatformPlugin.cmake")
    set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED}")
    if(NOT EXISTS "${_ARTIFACT_VCPKG_INSTALLED}/debug/bin"
       AND DEFINED VCPKG_INSTALLED_DIR
       AND EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows")
        set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED_DIR}/x64-windows")
    endif()
    if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        set(_ARTIFACT_VCPKG_INSTALLED "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
    endif()
    add_custom_command(TARGET Artifact POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:Artifact>"
            "-DVCPKG_INSTALLED=${_ARTIFACT_VCPKG_INSTALLED}"
            "-DCONFIG=$<CONFIG>"
            -P "${_ARTIFACT_COPY_RUNTIME_SCRIPT}"
        COMMENT "Deploying vcpkg runtime DLLs for Artifact"
        VERBATIM
    )
    add_custom_command(TARGET Artifact POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:Artifact>"
            "-DVCPKG_INSTALLED=${_ARTIFACT_VCPKG_INSTALLED}"
            "-DCONFIG=$<CONFIG>"
            -P "${_ARTIFACT_COPY_QT_PLATFORM_SCRIPT}"
        COMMENT "Deploying Qt platform plugin for Artifact"
        VERBATIM
    )

endif()

# ---------------------------------------------------------------------------
# Translations deployment
# ---------------------------------------------------------------------------
add_custom_command(TARGET Artifact POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/translations"
        "$<TARGET_FILE_DIR:Artifact>/translations"
    COMMENT "Deploying translation files for Artifact"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Optional deploy copy (no hard-coded path)
# ---------------------------------------------------------------------------
if(ARTIFACT_DEPLOY_DIR)
    add_custom_command(TARGET Artifact POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACT_DEPLOY_DIR}/$<CONFIG>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Artifact>"
            "${ARTIFACT_DEPLOY_DIR}/$<CONFIG>/"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:Artifact>
            "${ARTIFACT_DEPLOY_DIR}/$<CONFIG>/"
        COMMENT "Copying Artifact executable and runtime DLLs to ARTIFACT_DEPLOY_DIR"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()
