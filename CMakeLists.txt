cmake_minimum_required(VERSION 3.10)

PROJECT(palloc LANGUAGES C)

OPTION(PALLOC_TEST "PALLOC_TEST" OFF)

MACRO(PALLOC_ADD_FILTER group_name)
    SOURCE_GROUP(${group_name} FILES ${ARGN})
    SET(PALLOC_SOURCE_FILES ${HUMMINGBIRD_SOURCE_FILES} ${ARGN})
ENDMACRO()

PALLOC_ADD_FILTER(
src
    include/palloc/palloc.h
    src/palloc.c
)

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