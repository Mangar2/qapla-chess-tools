# qapla-chess-tools
#
# Build with the presets, see CMakePresets.json:
#
#     cmake --preset release
#     cmake --build --preset release                 every tool
#     cmake --build --preset release-gamefile        one tool on its own (gamefile, embedbin)
#
# ================================================================================================
# THE RULE OF THIS FILE - it has to stay this way
#
# Everything that depends on the operating system lives in exactly one of three sections below:
# WINDOWS, LINUX, MACOS. Each section is complete for its system and is the only place that system
# is configured.
#
# There is never a condition over two systems - no "if(NOT WIN32)", no "if(UNIX)" (which is Linux
# AND macOS), no "elseif(APPLE) ... else()" where the else silently means "Linux". A setting is
# either truly the same on all three systems, and then it stands in the common part, or it goes
# into all three sections, written out three times if need be. A shared condition hides which
# systems a setting reaches, and the next change to it changes a system nobody looked at.
# ================================================================================================

cmake_minimum_required(VERSION 3.21)
project(qapla-chess-tools LANGUAGES CXX)

# ------------------------------------------------------------------------------------------------
# Common - the same on every system
# ------------------------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# What the sections below fill in. Every tool links it, so a setting made there reaches every tool.
add_library(qapla-platform INTERFACE)
# The presets compile with clang on every system, so the warnings are the same everywhere.
target_compile_options(qapla-platform INTERFACE -Wall -Wextra)

# ------------------------------------------------------------------------------------------------
# WINDOWS
# ------------------------------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # clang++ there targets the MSVC ABI and links with lld-link. The C and C++ runtime go into the
    # binary: the C++ runtime of Visual Studio is not on every machine, and a tool that only starts
    # where Visual Studio is installed is not a tool to hand on.
    # A variable and not a property of qapla-platform: the runtime is a property of each target and
    # does not travel through a link, so it is set before any target exists.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # fopen and friends are deprecated by the Microsoft runtime in favour of its own _s functions;
    # the standard ones are what the code uses on every system.
    target_compile_definitions(qapla-platform INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# ------------------------------------------------------------------------------------------------
# LINUX
# ------------------------------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # libstdc++ is linked into the binary: its version is the one that travels badly, an older
    # distribution answers a newer one with "version GLIBCXX_... not found". libgcc_s stays dynamic,
    # as in qapla-engine-tester: linking it in drags in an unwinder that demands a newer glibc.
    target_link_options(qapla-platform INTERFACE -static-libstdc++)
endif()

# ------------------------------------------------------------------------------------------------
# MACOS
# ------------------------------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # libc++ is the C++ runtime of the system and always there, so it stays dynamic.
    target_compile_options(qapla-platform INTERFACE -stdlib=libc++)
    target_link_options(qapla-platform INTERFACE -stdlib=libc++)
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Windows|Linux|Darwin)$")
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} has no section in this file - add a fourth one")
endif()

# ------------------------------------------------------------------------------------------------
# The code the tools share - the same on every system
# ------------------------------------------------------------------------------------------------

# Object libraries, not static ones: a static library is linked by a different rule, the linker
# takes only the object files that close an open symbol, and one whose only contribution is a
# static constructor closes none and silently drops out. qapla-engine-tester lost its move tables
# that way. An object library puts every object file into every tool that uses it.
add_library(qapla-cli OBJECT
    src/cli-settings-manager.cpp
)
target_include_directories(qapla-cli PUBLIC src)
target_link_libraries(qapla-cli PUBLIC qapla-platform)

add_library(qapla-game-file OBJECT
    src/game-file/game-file.cpp
    src/game-file/pgn-to-game-file.cpp
    src/game-file/game-file-to-pgn.cpp
)
target_include_directories(qapla-game-file PUBLIC src)
target_link_libraries(qapla-game-file PUBLIC qapla-platform)

add_library(qapla-embed OBJECT
    src/embed/file-loader.cpp
    src/embed/cpp-array-writer.cpp
)
target_include_directories(qapla-embed PUBLIC src)
target_link_libraries(qapla-embed PUBLIC qapla-platform)

# ------------------------------------------------------------------------------------------------
# The tools - one executable each, each a target of its own, so each can be built alone
# ------------------------------------------------------------------------------------------------

function(qapla_tool name)
    add_executable(${name} src/tools/${name}.cpp)
    target_link_libraries(${name} PRIVATE qapla-platform ${ARGN})
endfunction()

qapla_tool(gamefile qapla-cli qapla-game-file)
qapla_tool(embedbin qapla-cli qapla-embed)
