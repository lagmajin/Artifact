cmake_minimum_required(VERSION 3.28)
project(Artifact)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# ソース収集 (vcxproj の ItemGroup を参考に)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APP_IMPL CONFIGURE_DEPENDS 
    "src/*.cpp"
)

# モジュールインターフェース / 実装ユニット
file(GLOB_RECURSE APP_MODULES CONFIGURE_DEPENDS 
    "include/*.ixx" 
    "src/*.ixx" 
    "src/*.cppm"
)

# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------
add_executable(Artifact WIN32)

target_sources(Artifact
    PRIVATE 
        ${APP_IMPL}
    PUBLIC 
        FILE_SET CXX_MODULES FILES ${APP_MODULES}
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
        "${VCPKG_INSTALLED}/include/python3.12"
        "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.win-x64/9.0.5/runtimes/win-x64/native"
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
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia HINTS "${VCPKG_INSTALLED}/share/Qt6")
target_link_libraries(Artifact PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Multimedia)

# OpenCV
find_package(OpenCV REQUIRED HINTS "${OPENCV_BUILD_DIR}")
target_link_libraries(Artifact PRIVATE ${OpenCV_LIBS})

# OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(Artifact PRIVATE OpenMP::OpenMP_CXX)

# Windows 固有
if(WIN32)
    target_link_libraries(Artifact PRIVATE dwmapi shell32 user32 ole32 advapi32)
endif()

# ---------------------------------------------------------------------------
# シェーダー (簡易的な HLSL 登録)
# ---------------------------------------------------------------------------
# TODO: FxCompile の CMake 対応が必要であれば追加
