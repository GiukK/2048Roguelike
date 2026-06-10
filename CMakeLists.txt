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

# Invariant tests: a second executable sharing every game source except the
# game's main(), plus a plain-assert driver (no test-framework dependency).
# It drives the real core against a hidden window, since the core classes
# currently require a RenderSystem. DLLs/assets are shared: both targets land
# in the same config directory, so the copies below cover the tests too.
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
set(GAME_SOURCES_NO_MAIN ${SOURCES})
list(REMOVE_ITEM GAME_SOURCES_NO_MAIN "${CMAKE_SOURCE_DIR}/src/main.cpp")
add_executable(SFML3_Tests ${GAME_SOURCES_NO_MAIN} ${TEST_SOURCES} ${HEADERS})
target_include_directories(SFML3_Tests PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(SFML3_Tests PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Network
    SFML::Audio
)

enable_testing()
add_test(NAME invariants
         COMMAND SFML3_Tests
         WORKING_DIRECTORY $<TARGET_FILE_DIR:SFML3_Tests>)

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
