
project(
    rocky_demo
    VERSION 0
    DESCRIPTION "simple demo using Rocky"
    HOMEPAGE_URL https://github.com/pelicanmapping/rocky
    LANGUAGES CXX C
)

find_package(ImGui)

if(ImGui_FOUND)                
    file(GLOB SOURCES *.cpp vsgImGui/*.cpp)
    file(GLOB HEADERS *.h vsgImGui/*.h)

    # enable folders for IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
    assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

    add_executable(rocky_demo ${SOURCES} ${HEADERS})
    
    target_link_libraries(rocky_demo rocky imgui::imgui)

    install(TARGETS rocky_demo RUNTIME DESTINATION bin)

    set_target_properties(rocky_demo PROPERTIES FOLDER "apps")
    set_target_properties(rocky_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "../../../install/bin")
else()
    message(WARNING "imgui not found, rocky_demo will not build :(")
endif()
