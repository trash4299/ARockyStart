
# Main PROJECT definition
project(
    rocky_simple
    VERSION 0
    DESCRIPTION "simple globe demo using Rocky"
    HOMEPAGE_URL https://github.com/pelicanmapping/rocky
    LANGUAGES CXX C
)

# require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB SOURCES *.cpp)

add_executable(rocky_simple ${SOURCES})

target_link_libraries(rocky_simple rocky)

install(TARGETS rocky_simple RUNTIME DESTINATION bin)

set_target_properties(rocky_simple PROPERTIES FOLDER "apps")
set_target_properties(rocky_simple PROPERTIES RUNTIME_OUTPUT_DIRECTORY "install/bin")
