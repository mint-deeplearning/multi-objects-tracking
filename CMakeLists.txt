cmake_minimum_required(VERSION 2.8)
project(test)

find_package(OpenCV REQUIRED)
find_package(Threads)

if(NOT WIN32)
ADD_DEFINITIONS("-std=c++0x -O3")
endif(NOT WIN32)

include_directories(src) 
FILE(GLOB_RECURSE sourcefiles "src/*.cpp")
add_executable( kcf ${sourcefiles} )
target_link_libraries( kcf ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})




