cmake_minimum_required(VERSION 3.14)
project(notvim C)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(BUILD_SHARED_LIBS ON CACHE BOOL "build shared libraries" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(LuaJIT QUIET luajit)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall)
    #    add_compile_options(-Wextra)
endif()

add_executable(nv)

file(GLOB SRC_FILES
    "buffer.c" "editor.c" "events.c"
    "cursor.c" "error.c" "main.c" "window.c"
    "draw.c" "input.c" "arena.c"
)

if(APPLE)
    target_compile_definitions(nv PRIVATE _DARWIN_C_SOURCE)
    list(APPEND SRC_FILES tui_posix.c)
elseif(UNIX)
    target_compile_definitions(nv PRIVATE
        _XOPEN_SOURCE=700
        _DEFAULT_SOURCE
        _BSD_SOURCE
    )
    list(APPEND SRC_FILES tui_posix.c)
elseif(WIN32)
    # TODO: add when tui_win32 exists
endif()

target_sources(nv PRIVATE ${SRC_FILES})
target_include_directories(nv PRIVATE "./include" "./")

add_library(nvtree STATIC "nvtree/nvtree.c" "nvtree/rb.c")
target_include_directories(nvtree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/nvtree)
target_link_libraries(nv PRIVATE nvtree)

if(LuaJIT_FOUND)
    add_library(nvlua SHARED "./lua/nvlua.c" "./lua/events.c")
    target_include_directories(nvlua PRIVATE ${LuaJIT_INCLUDE_DIRS} "./include" "./")
    target_link_directories(nvlua PRIVATE ${LuaJIT_LIBRARY_DIRS})
    target_link_libraries(nvlua PRIVATE ${LuaJIT_LIBRARIES})
    target_compile_definitions(nvlua PRIVATE NV_LUAJIT)
    set_target_properties(nvlua PROPERTIES OUTPUT_NAME "nvlua")
else()
    message(STATUS "luajit not found, nvlua NOT built")
    target_compile_definitions(nv PRIVATE NV_NO_LUAJIT)
endif() 

# libuv
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libuv EXCLUDE_FROM_ALL)
target_link_libraries(nv PRIVATE uv)

# nng
set(NNG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(nng EXCLUDE_FROM_ALL)

# yyjson
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(yyjson EXCLUDE_FROM_ALL)

# tree-sitter
add_library(tree-sitter STATIC "./tree-sitter/lib/src/lib.c")
target_include_directories(tree-sitter PRIVATE "./tree-sitter/lib/src" PUBLIC "./tree-sitter/lib/include")
target_link_libraries(nv PRIVATE tree-sitter)

add_library(nvrpc SHARED nvrpc.c rpc.c)
target_include_directories(nvrpc PRIVATE "./include" "./")
target_link_libraries(nvrpc PRIVATE nng yyjson)

target_compile_definitions(nv PRIVATE
    TB_OPT_ATTR_W=64
)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

target_compile_definitions(nv PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    )

target_compile_options(nv PRIVATE
    $<$<CONFIG:Debug>:-pg>
    $<$<CONFIG:Release>:-O2>
    )

target_compile_options(nv PRIVATE
    $<$<CONFIG:Debug>:-Wunused-function>
    )
