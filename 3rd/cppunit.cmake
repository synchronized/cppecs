

set(TARGET_NAME cppunit)
add_library(${TARGET_NAME} INTERFACE)
add_library(${TARGET_NAME}::${TARGET_NAME} ALIAS ${TARGET_NAME})
target_include_directories(${TARGET_NAME} INTERFACE cppunit)