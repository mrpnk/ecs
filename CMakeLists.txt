cmake_minimum_required(VERSION 3.22)
project(ECS)

set(CMAKE_CXX_STANDARD 23)


include_directories(../cppLibraries/sfml/include/)

#set(SFML_DIR ../cppLibraries/sfml/extlibs/libs-mingw/x64)

#find_package(SFML COMPONENTS graphics REQUIRED)
find_library(SFML_LIBRARIES_Graphics sfml-graphics HINT ../cppLibraries/sfml/cmake-build-release/lib)
find_library(SFML_LIBRARIES_Window sfml-window HINT ../cppLibraries/sfml/cmake-build-release/lib)
find_library(SFML_LIBRARIES_System sfml-system HINT ../cppLibraries/sfml/cmake-build-release/lib)

message(STATUS "SFML_LIBRARIES is ${SFML_LIBRARIES}")


add_executable(ECS src/main.cpp)

target_link_libraries(ECS   ${SFML_LIBRARIES_Graphics}
                            ${SFML_LIBRARIES_Window}
                            ${SFML_LIBRARIES_System})
