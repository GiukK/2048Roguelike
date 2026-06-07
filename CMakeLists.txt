cmake_minimum_required(VERSION 3.20)
project(SFML3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SFML from the bundled External directory
set(SFML_DIR "${CMAKE_SOURCE_DIR}/External/SFML/lib/cmake/SFML")
find_package(SFML 3 COMPONENTS Graphics Window System Network Audio REQUIRED)

# Collect sources automatically
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Network
    SFML::Audio
)

# Copy SFML DLLs next to the executable after build
file(GLOB SFML_DLLS "${CMAKE_SOURCE_DIR}/External/SFML/bin/*.dll")
foreach(DLL ${SFML_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endforeach()

# Copy assets (if any) next to the executable
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
endif()
