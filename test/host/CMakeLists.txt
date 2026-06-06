cmake_minimum_required(VERSION 3.16)
project(cybergame_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(test_fsm
    test_fsm.c
    ${REPO_ROOT}/components/fsm/fsm.c
    ${REPO_ROOT}/components/gamestate/gamestate.c
)

target_include_directories(test_fsm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO_ROOT}/components/fsm/include
    ${REPO_ROOT}/components/hardware/include
    ${REPO_ROOT}/components/gamestate/include
)

if(MSVC)
    target_compile_options(test_fsm PRIVATE /W3 /Zc:preprocessor)
else()
    target_compile_options(test_fsm PRIVATE -Wall -Wextra)
endif()

add_executable(test_gamestate
    test_gamestate.c
    ${REPO_ROOT}/components/gamestate/gamestate.c
)

target_include_directories(test_gamestate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO_ROOT}/components/gamestate/include
)

if(MSVC)
    target_compile_options(test_gamestate PRIVATE /W3 /Zc:preprocessor)
else()
    target_compile_options(test_gamestate PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME test_fsm        COMMAND test_fsm)
add_test(NAME test_gamestate  COMMAND test_gamestate)
