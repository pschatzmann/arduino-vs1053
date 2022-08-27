cmake_minimum_required(VERSION 3.16)

# set the project name
project(arduino_vs1053)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_LIST_C CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/*.cpp" )

# define libraries
add_library (arduino_vs1053 ${SRC_LIST_C})


# define location for header files
target_include_directories(arduino_vs1053 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src )

