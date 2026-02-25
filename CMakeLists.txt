cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(the_tag)

file(GLOB_RECURSE SRC_FILES "src/*.c")
target_sources(app PRIVATE ${SRC_FILES})