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

find_program(ARTIFACT_GIT_EXECUTABLE git)
set(ARTIFACT_BUILD_GIT_HASH "unknown")
set(ARTIFACT_BUILD_DIRTY "unknown")
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

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------
file(GLOB_RECURSE POTENTIAL_SOURCES 
    "src/*.cpp" "src/*.cppm" "src/*.ixx"
    "src/**/*.cpp" "src/**/*.cppm" "src/**/*.ixx"
    "include/*.ixx" "include/*.cppm"
    "include/**/*.ixx" "include/**/*.cppm"
)

if(NOT ARTIFACT_FEATURE_COMMAND_PALETTE)
    list(FILTER POTENTIAL_SOURCES EXCLUDE REGEX "/Widgets/CommandPalette/")
endif()
# Exclude worker process sources from the main application target
list(FILTER POTENTIAL_SOURCES EXCLUDE REGEX "/src/Worker/")

set(APP_MODULES "")
set(APP_IMPL "")
set(_PROVIDED_MODULES "")

foreach(file_path ${POTENTIAL_SOURCES})
    if(file_path MATCHES "\\.bak$" OR
       file_path MATCHES "src/Audio/Effects/AudioEffectsInit\\.cpp" OR
       file_path MATCHES "src/Test/ArtifactTestRenderQueue\\.cppm" OR
       file_path MATCHES "src/Generator/CloneGenerator\\.ixx" OR
       file_path MATCHES "include/Effects/RadialBlur/RadialBlurEffect\\.ixx" OR
       file_path MATCHES "src/Effects/RadialBlur/RadialBlurEffect\\.cppm" OR
       file_path MATCHES "src/Composition/ArtifactCompositionManager\\.cpp" OR
       file_path MATCHES "include/Effect/ArtifactStabilizer\\.ixx" OR
       file_path MATCHES "src/Effect/ArtifactStabilizer\\.cppm" OR
       file_path MATCHES "include/Render/ArtifactFrameCache\\.ixx" OR
       file_path MATCHES "src/Render/ArtifactFrameCache\\.cppm" OR
       file_path MATCHES "include/Layer/ArtifactLayerGroup\\.ixx" OR
       file_path MATCHES "src/Layer/ArtifactLayerGroup\\.cppm" OR
       file_path MATCHES "include/Color/ArtifactColorWheels\\.ixx" OR
       file_path MATCHES "src/Color/ArtifactColorWheels\\.cppm" OR
       file_path MATCHES "include/Preview/ArtifactTimelineClock\\.ixx" OR
       file_path MATCHES "src/Preview/ArtifactTimelineClock\\.cppm" OR
       file_path MATCHES "src/Widgets/WebUI/ArtifactWebUIHost\\.cppm")
        continue()
    endif()

    # 以前の 4096 バイト制限では、大量の include の後に export module がある場合に
    # 検出に失敗することがあったため、制限を 32KB に拡大します。
    file(READ "${file_path}" raw_content LIMIT 32768)
    
    # コメントを除去して誤判定を防ぐ
    string(REGEX REPLACE "//[^\n]*" "" content "${raw_content}")
    string(REGEX REPLACE "/\\*.*\\*/" "" content "${content}")
    
    set(is_mod_interface_or_partition FALSE)
    set(found_mod_name "")

    # 1. インターフェース判定: export module M;
    if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(is_mod_interface_or_partition TRUE)
        set(found_mod_name "${CMAKE_MATCH_1}")
    # 2. パーティション判定: module M:P; (コロンを含む)
    elseif(content MATCHES "module[ \t\r\n]+([a-zA-Z0-9\\._]+:[a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(is_mod_interface_or_partition TRUE)
        set(found_mod_name "${CMAKE_MATCH_1}")
    endif()

    if(is_mod_interface_or_partition)
        list(FIND _PROVIDED_MODULES "${found_mod_name}" _mod_idx)
        if(_mod_idx EQUAL -1)
            list(APPEND APP_MODULES "${file_path}")
            list(APPEND _PROVIDED_MODULES "${found_mod_name}")
        else()
            list(APPEND APP_IMPL "${file_path}")
            message(WARNING "Duplicate module name detected: ${found_mod_name} in ${file_path}")
        endif()
    else()
        list(APPEND APP_IMPL "${file_path}")
    endif()
endforeach()

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
add_executable(Artifact WIN32)

set_target_properties(Artifact PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# Version compile definitions
option(ARTIFACT_FEATURE_COMMAND_PALETTE "Enable experimental command palette widget (shell only, no real action registration yet)" OFF)
option(ARTIFACT_FEATURE_FARM_WORKER "Build out-of-process render farm worker binary (Phase 4)" OFF)

target_compile_definitions(Artifact PRIVATE
    ARTIFACT_VERSION_STRING="${ARTIFACT_VERSION}"
    ARTIFACT_VERSION_MAJOR=${ARTIFACT_VERSION_MAJOR}
    ARTIFACT_VERSION_MINOR=${ARTIFACT_VERSION_MINOR}
    ARTIFACT_VERSION_PATCH=${ARTIFACT_VERSION_PATCH}
    ARTIFACT_BUILD_GIT_HASH="${ARTIFACT_BUILD_GIT_HASH}"
    ARTIFACT_BUILD_TIMESTAMP="${ARTIFACT_BUILD_TIMESTAMP}"
    ARTIFACT_BUILD_DIRTY="${ARTIFACT_BUILD_DIRTY}"
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

# MSVC-style linker diagnostics
set(ARTIFACT_LINK_VERBOSE_LIB ON CACHE BOOL "Enable MSVC linker library-resolution diagnostics" FORCE)
if(ARTIFACT_LINK_VERBOSE_LIB)
    target_link_options(Artifact PRIVATE "$<$<BOOL:${ARTIFACT_FRONTEND_MSVC}>:/VERBOSE:LIB>")
endif()
if(ARTIFACT_FRONTEND_MSVC)
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
target_link_libraries(Artifact PRIVATE ArtifactCore ArtifactWidgets)

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
target_link_libraries(Artifact PRIVATE ${DILIGENT_LIBS})
if(DILIGENT_COMPILE_DEFINITIONS)
    list(REMOVE_DUPLICATES DILIGENT_COMPILE_DEFINITIONS)
    target_compile_definitions(Artifact PRIVATE ${DILIGENT_COMPILE_DEFINITIONS})
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets Concurrent Svg)
target_link_libraries(Artifact PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Concurrent Qt6::Svg)

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
    target_link_libraries(ArtifactWorker PRIVATE ArtifactCore Qt6::Core Qt6::Network)
endif()

# ---------------------------------------------------------------------------
# Qt runtime deployment (Windows)
# ---------------------------------------------------------------------------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
        set(_ARTIFACT_DEPLOY_QT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunWindeployqt.cmake")
        set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED}")
        if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
            set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED_DIR}")
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
    if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
        set(_ARTIFACT_VCPKG_INSTALLED "${VCPKG_INSTALLED_DIR}")
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
