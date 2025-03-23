cmake_minimum_required(VERSION 3.10)

PROJECT(palloc LANGUAGES C)

OPTION(PALLOC_THREAD "PALLOC_THREAD" OFF)
OPTION(PALLOC_LOCKFREE "PALLOC_LOCKFREE" OFF)
OPTION(PALLOC_MUTEX "PALLOC_MUTEX" OFF)
OPTION(PALLOC_SANITIZE "PALLOC_SANITIZE" OFF)
OPTION(PALLOC_TEST "PALLOC_TEST" OFF)

if(PALLOC_SANITIZE)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /fsanitize=address")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /fsanitize=address")
endif()

MACRO(PALLOC_ADD_FILTER group_name)
    SOURCE_GROUP(${group_name} FILES ${ARGN})
    SET(PALLOC_SOURCE_FILES ${HUMMINGBIRD_SOURCE_FILES} ${ARGN})
ENDMACRO()

PALLOC_ADD_FILTER(
src
    include/palloc/palloc.h
    src/palloc.c
)

if(PALLOC_THREAD)
    add_definitions(-DPALLOC_THREAD)
endif()

if(PALLOC_LOCKFREE)
    add_definitions(-DPALLOC_LOCKFREE)
endif()

if(PALLOC_MUTEX)
    add_definitions(-DPALLOC_MUTEX)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

ADD_LIBRARY(${PROJECT_NAME} STATIC ${PALLOC_SOURCE_FILES})
    
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER palloc)

macro(ADD_PALLOC_TEST testname)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    ADD_EXECUTABLE(test_${testname} tests/test_${testname}.c)
    
    set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER tests)
    
    TARGET_LINK_LIBRARIES(test_${testname} palloc)

    set_target_properties (test_${testname} PROPERTIES
        FOLDER tests
    )
    
    if(PALLOC_TEST)
        ADD_TEST(NAME ${testname} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_${testname})
    endif()
endmacro()

if(PALLOC_TEST)
    include(CTest)
    enable_testing()
endif()

ADD_PALLOC_TEST(fuzz)