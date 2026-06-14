cmake_minimum_required(VERSION 3.24)
project(curvsynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CURV_BUILD_PLUGIN "Build the JUCE plugin targets" ON)
option(CURV_SANITIZE "Build core/tests with Address+UB sanitizers" OFF)

include(FetchContent)

# eigen/spectra are used header-only: SOURCE_SUBDIR points nowhere so
# MakeAvailable populates without running their CMake builds
FetchContent_Declare(eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR cmake_disabled)
FetchContent_Declare(spectra
    URL https://github.com/yixuan/spectra/archive/refs/tags/v1.0.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR cmake_disabled)
FetchContent_Declare(catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.4.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(eigen spectra catch2)

# ------------------------------------------------------------------ curv_core
# Pure C++ geometry/DSP/engine — no JUCE. Shared by plugin, tools, and tests
# so the oracles never go dark.
add_library(curv_core STATIC
    src/geometry/Mesh.cpp
    src/geometry/Presets.cpp
    src/geometry/Laplacian.cpp
    src/geometry/EigenSolver.cpp
    src/geometry/RicciFlow.cpp
    src/engine/GeometryService.cpp)
target_include_directories(curv_core PUBLIC
    ${eigen_SOURCE_DIR}
    ${spectra_SOURCE_DIR}/include)
target_compile_options(curv_core PRIVATE -Wall -Wextra)

if(CURV_SANITIZE)
    foreach(tgt curv_core)
        target_compile_options(${tgt} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${tgt} PUBLIC -fsanitize=address,undefined)
    endforeach()
endif()

# -------------------------------------------------------------- render_offline
add_executable(render_offline tools/render_offline.cpp)
target_link_libraries(render_offline PRIVATE curv_core)

add_executable(flow_soak tools/flow_soak.cpp)
target_link_libraries(flow_soak PRIVATE curv_core)

# ---------------------------------------------------------------------- tests
enable_testing()
add_executable(curv_tests
    tests/geometry_test.cpp
    tests/engine_test.cpp
    tests/flow_test.cpp)
target_link_libraries(curv_tests PRIVATE curv_core Catch2::Catch2WithMain)
target_compile_definitions(curv_tests PRIVATE
    CURV_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    CURV_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
add_test(NAME curv_tests COMMAND curv_tests)

# --------------------------------------------------------------------- plugin
if(CURV_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(CurvAssets SOURCES
        assets/manifolds/genus2.obj
        assets/manifolds/mandelbulb.obj)

    juce_add_plugin(curvsynth_plugin
        PRODUCT_NAME "curvsynth"
        COMPANY_NAME "Lifted Truck"
        BUNDLE_ID com.liftedtruck.curvsynth
        PLUGIN_MANUFACTURER_CODE Lftk
        PLUGIN_CODE Crvs
        FORMATS VST3 Standalone
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE)

    target_sources(curvsynth_plugin PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp)
    target_link_libraries(curvsynth_plugin
        PRIVATE curv_core CurvAssets juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
    target_compile_definitions(curvsynth_plugin PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    # Re-sign after JUCE's moduleinfo.json generation: if the manifest is
    # written after ad-hoc signing, the seal breaks and Ableton silently
    # skips the bundle during its plugin scan (observed in the Morphos
    # project). Force a final signature so the seal always covers the
    # finished bundle, both in the build tree and the installed copy.
    if(APPLE)
        add_custom_command(TARGET curvsynth_plugin_VST3 POST_BUILD
            COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:curvsynth_plugin_VST3>"
            COMMAND codesign --force -s - "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/curvsynth.vst3"
            COMMAND codesign --verify --verbose=2 "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/curvsynth.vst3"
            COMMENT "Re-signing VST3 so the seal covers moduleinfo.json")
    endif()
endif()
