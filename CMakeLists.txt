cmake_minimum_required(VERSION 3.16)
project(APlaceForAgents VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------- raylib
include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ---------------------------------------------------------------- game
add_executable(aplaceforagents
    src/main.cpp
    src/music.cpp
    src/core.cpp
    src/world.cpp
    src/overworld.cpp
    src/render.cpp
    src/dialog.cpp
    src/room.cpp
    src/battle.cpp
)
target_include_directories(aplaceforagents PRIVATE src)
target_link_libraries(aplaceforagents PRIVATE raylib)

if (MSVC)
    target_compile_options(aplaceforagents PRIVATE /W3 /utf-8)
else()
    target_compile_options(aplaceforagents PRIVATE -Wall -Wno-unused-parameter)
endif()

set_target_properties(aplaceforagents PROPERTIES OUTPUT_NAME "APlaceForAgents")

# copy runtime assets next to the binary so the exe is self-contained
add_custom_command(TARGET aplaceforagents POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:aplaceforagents>/assets
)
