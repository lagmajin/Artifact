cmake_minimum_required(VERSION 3.28)
project(Artifact)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# ソース収集とモジュール分類
# ---------------------------------------------------------------------------
file(GLOB_RECURSE POTENTIAL_SOURCES CONFIGURE_DEPENDS 
    "src/*.cpp" "src/*.cppm" "src/*.ixx"
    "include/*.ixx" "include/*.cppm"
)

set(APP_MODULES "")
set(APP_IMPL "")
set(_PROVIDED_MODULES "")

foreach(file_path ${POTENTIAL_SOURCES})
    if(file_path MATCHES "\\.bak$")
        continue()
    endif()

    file(READ "${file_path}" raw_content LIMIT 4096)
    string(REGEX REPLACE "//[^\n]*" "" content "${raw_content}")
    string(REGEX REPLACE "/\\*.*\\*/" "" content "${content}")
    
    set(is_mod_interface_or_partition FALSE)
    set(found_mod_name "")

    if(content MATCHES "export[ \t\r\n]+module[ \t\r\n]+([a-zA-Z0-9\\._:]+)[ \t\r\n]*;")
        set(is_mod_interface_or_partition TRUE)
        set(found_mod_name "${CMAKE_MATCH_1}")
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
        endif()
    else()
        list(APPEND APP_IMPL "${file_path}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
add_executable(Artifact WIN32)

target_sources(Artifact
    PRIVATE ${APP_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${APP_MODULES}
)

# MSVC フラグ
if(MSVC)
    target_compile_options(Artifact PRIVATE /Zf /Zc:__cplusplus /utf-8 /openmp)
    target_compile_definitions(Artifact PRIVATE 
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        GLOG_USE_GLOG_EXPORT
        QT_NO_KEYWORDS
        _WIN32_WINNT=0x0A00
    )
endif()

target_include_directories(Artifact
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/verdigris/src"
        "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.win-x64/9.0.5/runtimes/win-x64/native"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${VCPKG_INSTALLED}/include/qtadvanceddocking-qt6"
)

# ---------------------------------------------------------------------------
# 依存関係
# ---------------------------------------------------------------------------

# 内部ライブラリ
target_link_libraries(Artifact PRIVATE ArtifactCore ArtifactWidgets)

# DiligentEngine
target_link_libraries(Artifact PRIVATE 
    Diligent-GraphicsEngineD3D11-static
    Diligent-GraphicsEngineD3D12-static
    Diligent-GraphicsEngineVulkan-static
    Diligent-Common
)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia Concurrent)
target_link_libraries(Artifact PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::Concurrent)

# OpenCV
find_package(OpenCV REQUIRED)
target_link_libraries(Artifact PRIVATE ${OpenCV_LIBS})

# OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(Artifact PRIVATE OpenMP::OpenMP_CXX)

find_package(TBB REQUIRED)
target_link_libraries(Artifact PRIVATE TBB::tbb)

find_package(glm CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE glm::glm)

find_package(Stb REQUIRED)
target_include_directories(Artifact PRIVATE ${Stb_INCLUDE_DIR})

find_package(assimp REQUIRED)
target_link_libraries(Artifact PRIVATE assimp::assimp)

find_package(EnTT CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE EnTT::EnTT)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(Artifact PRIVATE magic_enum::magic_enum)

find_package(Boost REQUIRED COMPONENTS iostreams)
target_link_libraries(Artifact PRIVATE Boost::iostreams)

# Windows 固有
if(WIN32)
    target_link_libraries(Artifact PRIVATE dwmapi shell32 user32 ole32 advapi32)
endif()
