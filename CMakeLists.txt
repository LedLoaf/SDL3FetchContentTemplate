cmake_minimum_required(VERSION 3.24)
project(SDL3ImGuiApp LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "This project uses the prebuilt MSVC (VC) SDL3 packages. Use a Visual Studio generator.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Versions -- bump these as new releases come out (check the GitHub releases
# pages for libsdl-org/SDL, SDL_image, SDL_mixer, SDL_ttf and ocornut/imgui).
# ---------------------------------------------------------------------------
set(SDL3_VERSION        "3.4.0")
set(SDL3_IMAGE_VERSION  "3.4.0")
set(SDL3_MIXER_VERSION  "3.2.4")
set(SDL3_TTF_VERSION    "3.2.2")
set(IMGUI_VERSION       "1.92.3")

# ---------------------------------------------------------------------------
# Everything is downloaded into <project root>/dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

# Name of the folder that holds all downloads. Change it here if you want a different spelling.
set(DEPS_FOLDER_NAME "dependencies")
set(DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/${DEPS_FOLDER_NAME}")

# Keep FetchContent's own scratch/stamp files out of the build folder (_deps)
# by forcing the base dir into the cache, and pin the per-dependency dirs too.
set(FETCHCONTENT_BASE_DIR "${DEPS_DIR}/_fetchcontent" CACHE PATH "" FORCE)
set(FETCHCONTENT_QUIET OFF)

# Prebuilt x64 MSVC development packages (headers + lib/x64/*.lib + *.dll)
FetchContent_Declare(sdl3
    URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-devel-${SDL3_VERSION}-VC.zip
    SOURCE_DIR "${DEPS_DIR}/SDL3"
    BINARY_DIR "${DEPS_DIR}/_fetchcontent/SDL3-build"
    SUBBUILD_DIR "${DEPS_DIR}/_fetchcontent/SDL3-subbuild"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_Declare(sdl3_image
    URL https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL3_IMAGE_VERSION}/SDL3_image-devel-${SDL3_IMAGE_VERSION}-VC.zip
    SOURCE_DIR "${DEPS_DIR}/SDL3_image"
    BINARY_DIR "${DEPS_DIR}/_fetchcontent/SDL3_image-build"
    SUBBUILD_DIR "${DEPS_DIR}/_fetchcontent/SDL3_image-subbuild"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_Declare(sdl3_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL3_MIXER_VERSION}/SDL3_mixer-devel-${SDL3_MIXER_VERSION}-VC.zip
    SOURCE_DIR "${DEPS_DIR}/SDL3_mixer"
    BINARY_DIR "${DEPS_DIR}/_fetchcontent/SDL3_mixer-build"
    SUBBUILD_DIR "${DEPS_DIR}/_fetchcontent/SDL3_mixer-subbuild"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_Declare(sdl3_ttf
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL3_TTF_VERSION}/SDL3_ttf-devel-${SDL3_TTF_VERSION}-VC.zip
    SOURCE_DIR "${DEPS_DIR}/SDL3_ttf"
    BINARY_DIR "${DEPS_DIR}/_fetchcontent/SDL3_ttf-build"
    SUBBUILD_DIR "${DEPS_DIR}/_fetchcontent/SDL3_ttf-subbuild"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# imgui is source-only; we compile the files we need straight into the exe
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.zip
    SOURCE_DIR "${DEPS_DIR}/imgui"
    BINARY_DIR "${DEPS_DIR}/_fetchcontent/imgui-build"
    SUBBUILD_DIR "${DEPS_DIR}/_fetchcontent/imgui-subbuild"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(sdl3 sdl3_image sdl3_mixer sdl3_ttf imgui)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
set(IMGUI_SOURCES
    ${DEPS_DIR}/imgui/imgui.cpp
    ${DEPS_DIR}/imgui/imgui_demo.cpp
    ${DEPS_DIR}/imgui/imgui_draw.cpp
    ${DEPS_DIR}/imgui/imgui_tables.cpp
    ${DEPS_DIR}/imgui/imgui_widgets.cpp
    ${DEPS_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    ${DEPS_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp)

add_executable(app src/main.cpp ${IMGUI_SOURCES})

# ---------------------------------------------------------------------------
# Include / library paths expressed with $(SolutionDir).
# The generated .sln lives in the CMake build folder, so we compute the
# relative path from there to <project root>/dependencies
# (e.g. "../dependencies" when building into <root>/build).
# ---------------------------------------------------------------------------
file(RELATIVE_PATH DEPS_REL "${CMAKE_BINARY_DIR}" "${DEPS_DIR}")
set(DEPS_VS "$(SolutionDir)${DEPS_REL}")

target_compile_options(app PRIVATE
    "/I${DEPS_VS}/SDL3/include"
    "/I${DEPS_VS}/SDL3_image/include"
    "/I${DEPS_VS}/SDL3_mixer/include"
    "/I${DEPS_VS}/SDL3_ttf/include"
    "/I${DEPS_VS}/imgui"
    /utf-8)

target_link_options(app PRIVATE
    "/LIBPATH:${DEPS_VS}/SDL3/lib/x64"
    "/LIBPATH:${DEPS_VS}/SDL3_image/lib/x64"
    "/LIBPATH:${DEPS_VS}/SDL3_mixer/lib/x64"
    "/LIBPATH:${DEPS_VS}/SDL3_ttf/lib/x64")

target_link_libraries(app PRIVATE
    SDL3.lib SDL3_image.lib SDL3_mixer.lib SDL3_ttf.lib)

# Copy the runtime DLLs next to the exe after every build
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEPS_DIR}/SDL3/lib/x64/SDL3.dll"
        "${DEPS_DIR}/SDL3_image/lib/x64/SDL3_image.dll"
        "${DEPS_DIR}/SDL3_mixer/lib/x64/SDL3_mixer.dll"
        "${DEPS_DIR}/SDL3_ttf/lib/x64/SDL3_ttf.dll"
        "$<TARGET_FILE_DIR:app>")

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT app)
set_target_properties(app PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")