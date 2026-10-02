cmake_minimum_required(VERSION 3.20)
project(qapla-chess-tools CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(qapla-chess-tools
    src/qapla-chess-tools.cpp
    src/cli-settings-manager.cpp
    src/game-file/game-file.cpp
    src/game-file/pgn-to-game-file.cpp
    src/game-file/game-file-to-pgn.cpp
)
target_include_directories(qapla-chess-tools PRIVATE src)
