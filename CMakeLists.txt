cmake_minimum_required(VERSION 3.22)
project(ECS)

set(CMAKE_CXX_STANDARD 23)


include_directories(../cppLibraries/sfml/include/)
include_directories(../cppLibraries/fmt/include/)


if(MSVC)
    set(SFML_DIR "../../cppLibraries/sfml/vs/lib/Release")
else()
    set(SFML_DIR "../cppLibraries/sfml/cmake-build-release/lib")
endif()

set(SFML_LIBRARIES_Graphics "${SFML_DIR}/sfml-graphics")
set(SFML_LIBRARIES_Window   "${SFML_DIR}/sfml-window")
set(SFML_LIBRARIES_System   "${SFML_DIR}/sfml-system")


add_executable(ECS src/main.cpp)

if(MSVC)
    set_target_properties(ECS PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "../env/" )
    set_target_properties(ECS PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "../env/" )
    set_target_properties(ECS PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "../env/")
endif()

target_link_libraries(ECS   ${SFML_LIBRARIES_Graphics}
                            ${SFML_LIBRARIES_Window}
                            ${SFML_LIBRARIES_System})


