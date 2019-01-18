cmake_minimum_required(VERSION 3.13)
project(hangman)

set(CMAKE_CXX_STANDARD 14)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/words.txt ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
set (CMAKE_CXX_FLAGS "-pthread -Wall")
add_executable(hangman server.cpp Player.cpp Player.h Hangman.h Alphabet.h)
