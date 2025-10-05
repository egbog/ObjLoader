cmake_minimum_required(VERSION 3.20)
project(obj LANGUAGES CXX)

# Add ThreadPool project as a target
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pool pool)

add_library(obj STATIC
	obj/src/ObjHelpers.cpp
    obj/src/ObjLoader.cpp
)

# Header only GLM
add_library(glmff INTERFACE)
target_include_directories(glmff INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/obj/external)

# Add include directories
target_include_directories(obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/obj/include>
        $<INSTALL_INTERFACE:include> # for installed usage
)

target_link_libraries(obj PUBLIC pool glmff)

target_compile_features(obj PUBLIC cxx_std_20)