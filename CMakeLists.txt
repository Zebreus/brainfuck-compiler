cmake_minimum_required(VERSION 3.19)
project(brainfuck)

set(ANTLR4_JAR_LOCATION /usr/share/java/antlr-4.9.2-complete.jar)
set(ANTLR4_CPP_GENERATED_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ANTLR4_GENERATED_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

find_package(antlr4-runtime REQUIRED)
find_package(antlr4-generator REQUIRED)
find_package(LLVM REQUIRED CONFIG)

antlr4_generate(
        parser
        ${CMAKE_CURRENT_SOURCE_DIR}/brainfuck.g4
        BOTH
        FALSE
        TRUE
)

include_directories( ${PROJECT_BINARY_DIR} ${ANTLR4_INCLUDE_DIR} ${ANTLR4_INCLUDE_DIR_parser} )


include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${PROJECT_BINARY_DIR} ${ANTLR4_INCLUDE_DIR} ${ANTLR4_INCLUDE_DIR_parser})

add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 20)

add_executable(brainfuck main.cpp ${ANTLR4_SRC_FILES_parser} visitor.cpp visitor.h)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core orcjit native all)


# add required runtime library
add_dependencies( brainfuck antlr4_shared )

target_link_libraries( brainfuck PRIVATE antlr4_shared  ${llvm_libs})