cmake_minimum_required(VERSION 3.20)

project(AutoClickerPro LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_dx11 STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui_dx11 PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_dx11 PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=0)
target_link_libraries(imgui_dx11 PUBLIC d3d11 dxgi)

FetchContent_Declare(
  lua
  URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SRC
  ${lua_SOURCE_DIR}/src/*.c
)
list(FILTER LUA_SRC EXCLUDE REGEX ".*lua\\.c$")
list(FILTER LUA_SRC EXCLUDE REGEX ".*luac\\.c$")

add_library(lua_static STATIC ${LUA_SRC})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR}/src)

set(ACP_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ACP_GENERATED_DIR}")

add_executable(acp_icon_gen
  tools/icon_gen.cpp
)
if(MSVC)
  target_compile_options(acp_icon_gen PRIVATE /W4 /permissive- /utf-8)
endif()

set(ACP_ICON_PATH "${ACP_GENERATED_DIR}/AutoClickerPro.ico")
add_custom_command(
  OUTPUT "${ACP_ICON_PATH}"
  COMMAND acp_icon_gen "${ACP_ICON_PATH}"
  DEPENDS acp_icon_gen
  VERBATIM
)
add_custom_target(acp_generate_icon DEPENDS "${ACP_ICON_PATH}")

set(APP_ICON_PATH "${ACP_ICON_PATH}")
set(ACP_RESOURCE_H "${CMAKE_SOURCE_DIR}/src/resources/resource.h")
configure_file(src/resources/app.rc.in "${ACP_GENERATED_DIR}/app.rc" @ONLY)

add_executable(AutoClickerPro
  src/main.cpp
  src/app/App.cpp
  src/core/Converter.cpp
  src/core/Hooks.cpp
  src/core/Humanizer.cpp
  src/core/LuaEngine.cpp
  src/core/OverlayWindow.cpp
  src/core/Recorder.cpp
  src/core/Replayer.cpp
  src/core/TrcIO.cpp
  "${ACP_GENERATED_DIR}/app.rc"
  src/resources/resource.h
)

target_include_directories(AutoClickerPro PRIVATE src)
target_link_libraries(AutoClickerPro PRIVATE imgui_dx11 lua_static d3d11 dxgi user32 gdi32 shell32 comdlg32)
target_compile_definitions(AutoClickerPro PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
add_dependencies(AutoClickerPro acp_generate_icon)

if(MSVC)
  target_compile_options(AutoClickerPro PRIVATE /W4 /permissive- /utf-8)
  target_link_options(AutoClickerPro PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()

set_target_properties(AutoClickerPro PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(AutoClickerPro PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

add_executable(AutoClickerProTests
  tests/main.cpp
  src/core/Converter.cpp
  src/core/Recorder.cpp
  src/core/Replayer.cpp
  src/core/TrcIO.cpp
)
target_include_directories(AutoClickerProTests PRIVATE src)
target_link_libraries(AutoClickerProTests PRIVATE user32)
target_compile_definitions(AutoClickerProTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
  target_compile_options(AutoClickerProTests PRIVATE /W4 /permissive- /utf-8)
endif()
