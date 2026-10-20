cmake_minimum_required(VERSION 3.15)
project(nyx_nes CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O2 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
set(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "")
set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "")
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "")
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "")
set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-I ")

find_package(SDL2 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_nyx STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui_nyx PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(imgui_nyx PUBLIC ${SDL2_LIBRARIES})

add_executable(nes
    src/cpu.cpp
    src/bus.cpp
    src/cartridge.cpp
    src/ppu.cpp
    src/apu.cpp
    src/main.cpp
)
target_include_directories(nes PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(nes PRIVATE
    imgui_nyx
    ${SDL2_LIBRARIES}
    comdlg32
    user32
    ole32
    oleaut32
    shell32
)
