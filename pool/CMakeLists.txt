cmake_minimum_required(VERSION 3.20)
project(pool LANGUAGES CXX)

add_library(pool STATIC
	src/Logger.cpp
    src/ThreadPool.cpp
)
		
# Add include directories
target_include_directories(pool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include> # for installed usage
)

target_compile_features(pool PUBLIC cxx_std_20)