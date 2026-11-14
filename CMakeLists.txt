cmake_minimum_required(VERSION 3.22)

project(MAOQuack VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows : pas besoin d'installer de "redistribuable" Visual C++ sur le PC
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
endif()

# JUCE (fourni avec Tracktion Engine) puis les modules Tracktion
add_subdirectory(external/tracktion_engine/modules/juce)
add_subdirectory(external/tracktion_engine/modules)

juce_add_gui_app(MAOQuack
    PRODUCT_NAME "MAO Quack"
    COMPANY_NAME "TDL-Quack"
    BUNDLE_ID "com.tdlquack.maoquack"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Enregistrer du son avec le micro"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(MAOQuack PRIVATE
    src/Main.cpp)

target_include_directories(MAOQuack PRIVATE src)

target_compile_definitions(MAOQuack PRIVATE
    MAOQUACK_VERSION="${PROJECT_VERSION}"
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

if (MSVC)
    target_compile_options(MAOQuack PRIVATE /utf-8 /bigobj /MP)
endif()

target_link_libraries(MAOQuack PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

# Export MP3 : encodeur libre LAME (facultatif ; installé par vcpkg lors de la fabrication)
find_package(mp3lame CONFIG QUIET)

if (TARGET mp3lame::mp3lame)
    message(STATUS "MAO Quack : export MP3 active (LAME trouve)")
    target_link_libraries(MAOQuack PRIVATE mp3lame::mp3lame)
    target_compile_definitions(MAOQuack PRIVATE QUACK_AVEC_MP3=1)
else()
    message(STATUS "MAO Quack : export MP3 desactive (LAME introuvable)")
    target_compile_definitions(MAOQuack PRIVATE QUACK_AVEC_MP3=0)
endif()
