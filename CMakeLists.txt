cmake_minimum_required(VERSION 2.8)
project(shazam)
 
add_executable(shazam_demo shazam.c)
target_link_libraries(shazam_demo -lm)
