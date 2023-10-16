cmake_minimum_required(VERSION 3.2)

set(PRJ_NAME mytest)
project(${PRJ_NAME})

include_directories(./)
file(GLOB SOURCES "./*.cpp")

add_executable(${PRJ_NAME} ${SOURCES})