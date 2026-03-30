cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0025 NEW)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
    cmake_policy(SET CMP0093 NEW)
endif()

project(InSearchCreator VERSION 1.0)

set(CMAKE_POLICY_VERSION_MINIMUM "3.5")

option(STATIC "Set to ON to build xlnt as a static library instead of a shared library" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
    FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    set_target_properties(gmock gmock_main gtest gtest_main PROPERTIES FOLDER "GTest")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-char8_t /Zc:char8_t-")

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /utf-8")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)

    # TODO: add to release: /O2 /Oi /Ot /Ob2 /GL 
	set(ERROR_FLAGS "${ERROR_FLAGS} /Wall /WX /sdl")
    # set(SANITIZER_FLAGS "/fsanitize=address")
else()
	set(ERROR_FLAGS "${ERROR_FLAGS} -Wall -Wpedantic -Werror -g")
    set(SANITIZER_FLAGS "-fsanitize=leak,undefined,address")
endif()

option(SANITIZE "Enable sanitizers" ON)
if (${SANITIZE})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
endif()

option(DEBUG "Enable debug" ON)
if (${DEBUG})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

    add_compile_definitions(OPENGL)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(RES_FOLDER="${CMAKE_SOURCE_DIR}")
else()
    add_compile_definitions(RES_FOLDER="")
endif()

# add_custom_target(check chmod 777 ${CMAKE_SOURCE_DIR}/lint.sh && ${CMAKE_SOURCE_DIR}/lint.sh)

add_subdirectory(Engine)
add_subdirectory(App)
