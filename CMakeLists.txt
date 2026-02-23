cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(the_tag)

target_sources(app PRIVATE 
	src/draw_test.c
	src/jd79661_driver.c
)
