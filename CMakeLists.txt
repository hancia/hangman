cmake_minimum_required(VERSION 3.13)
project(hangman)

set(CMAKE_CXX_STANDARD 14)

add_executable(hangman client.cpp server.cpp Message.cpp Message.h Player.cpp Player.h Hangman.h)