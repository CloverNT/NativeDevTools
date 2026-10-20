cmake_minimum_required(VERSION 3.28)
project(NativeDevTools LANGUAGES CXX)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "NativeDevTools must be built with Clang/Clang-CL.")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
        CloverCore
        GIT_REPOSITORY https://github.com/CloverNT/CloverCore.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(CloverCore)

list(APPEND CMAKE_MODULE_PATH "${clovercore_SOURCE_DIR}/cmake")
set(LIBCXX_GITHUB_REPO "CloverNT/libcxx-build")
set(LIBCXX_ABI_NAMESPACE "__Cr")
include(Libc++)

add_library(NativeDevTools SHARED
        src/NativeDevTools.cpp
        src/SessionBookkeeping.cpp
        src/RendererChannel.cpp
        src/FrontendHost.cpp
        src/CdpClient.cpp
        src/NativeWc.cpp
        src/DevToolsDataSource.cpp
        src/Platform.Windows.cpp)
set_target_properties(NativeDevTools PROPERTIES PREFIX "" OUTPUT_NAME "nativedevtools")

target_include_directories(NativeDevTools PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(NativeDevTools PRIVATE CloverCore nlohmann_json::nlohmann_json)

target_use_libcxx(NativeDevTools)

add_custom_command(TARGET NativeDevTools POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/clover.json" "$<TARGET_FILE_DIR:NativeDevTools>/clover.json"
        VERBATIM)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/devtools_resources.pak")
    add_custom_command(TARGET nativedevtools POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/devtools_resources.pak" "$<TARGET_FILE_DIR:NativeDevTools>/devtools_resources.pak"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/devtools_resources.map" "$<TARGET_FILE_DIR:NativeDevTools>/devtools_resources.map"
            COMMENT "Staging bundled DevTools pak"
            VERBATIM)
endif ()
