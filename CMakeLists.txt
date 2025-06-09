cmake_minimum_required(VERSION 3.16)

project(cppecs)
set(CMAKE_CXX_STANDARD 17)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# By default, MSVC uses the GBK encoding.
# To use UTF-8 without a BOM header,
#     you need to add the following statement.
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_subdirectory(3rd)

# cppecs库
set(TARGET_NAME cppecs)
file(GLOB LIB_SOURCE "src/*.cpp")
add_library(${TARGET_NAME})
add_library(${TARGET_NAME}::${TARGET_NAME} ALIAS ${TARGET_NAME})
target_include_directories(${TARGET_NAME} PUBLIC include)
target_sources(${TARGET_NAME} PRIVATE ${LIB_SOURCE})

# 单元测试
enable_testing()
set(TEST_TARGET_NAME cppecs-test)
file(GLOB TEST_SOURCE "test/*.cpp")
add_executable(${TEST_TARGET_NAME})
target_include_directories(${TEST_TARGET_NAME} PRIVATE test)
target_sources(${TEST_TARGET_NAME} PRIVATE ${TEST_SOURCE})
target_link_libraries(${TEST_TARGET_NAME} PRIVATE cppecs::cppecs)
target_link_libraries(${TEST_TARGET_NAME} PRIVATE cppunit::cppunit)
add_test(cppecs-test cppecs-test)