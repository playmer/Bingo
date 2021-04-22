add_executable(PadInput "")

target_sources(PadInput 
PRIVATE
    main.cpp
)

target_include_directories(PadInput 
PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${DependenciesDirectory}/glm
    ${DependenciesDirectory}/gli)

target_link_libraries(PadInput 
PRIVATE
    SOIS
)

target_compile_definitions(PadInput PRIVATE GLFW_INCLUDE_NONE)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    target_link_libraries(PadInput PRIVATE opengl32.lib)

    target_compile_options(PadInput PRIVATE 
    PUBLIC
        -permissive- -std:c++17
    PRIVATE
        -WX- -W4
    )
endif()

set_target_properties(PadInput PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${AssetDirectory}/Bin)
