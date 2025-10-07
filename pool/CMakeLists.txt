cmake_minimum_required(VERSION 3.20)
project(pool LANGUAGES CXX)

# Source files
file(GLOB_RECURSE POOL_SRC
	"src/*.cpp"
	"include/*.h"
	"include/*.hpp"
	"include/*.inl"
)

add_library(pool STATIC
	${POOL_SRC}
)
		
# Add include directories
target_include_directories(pool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include> # for installed usage
)

target_compile_features(pool PUBLIC cxx_std_20)