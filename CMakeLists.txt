cmake_minimum_required(VERSION 3.25)

set(GLM_BUILD_INSTALL OFF CACHE BOOL "Do not install GLM from CMake")
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "Force C++ 20 support for GLM")
set(FASTFLOAT_CXX_STANDARD 20 CACHE STRING "Force C++ 20 support for fast float" )
set(FASTFLOAT_INSTALL OFF CACHE BOOL "Do not install fast float from CMake" FORCE)

project(obj LANGUAGES CXX)

# Add ThreadPool project as a target
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pool pool)

# Source files
file(GLOB_RECURSE OBJ_SRC
	"obj/src/*.cpp"
	"obj/include/*.h"
	"obj/include/*.hpp"
	"obj/include/*.inl"
)

add_library(obj STATIC ${OBJ_SRC})

# Header only GLM
if (NOT TARGET glm::glm)
	include(FetchContent)
	FetchContent_Declare(
		glm
		GIT_REPOSITORY	https://github.com/g-truc/glm.git
		GIT_TAG 	0af55ccecd98d4e5a8d1fad7de25ba429d60e863 #refs/tags/1.0.1
	)
	FetchContent_MakeAvailable(glm)
endif()

# Header only fast float
if (NOT TARGET fast_float)
	FetchContent_Declare(
	  fast_float
	  GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
	  GIT_TAG tags/v8.1.0
	  GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(fast_float)
endif()

# Add include directories
target_include_directories(obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/obj/include>
        $<INSTALL_INTERFACE:include> # for installed usage
)

# Preprocessor definitions
target_compile_definitions(obj
    PRIVATE
		GLM_FORCE_INTRINSICS # force autodetect of SIMD architecture for GLM
)

target_link_libraries(obj PUBLIC pool glm::glm fast_float)

target_compile_features(obj PUBLIC cxx_std_20)
