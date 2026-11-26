cmake_minimum_required(VERSION 3.21)
project(vwatch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(vwatch main.cpp vmwindow.cpp watchview.cpp session.cpp)
target_include_directories(vwatch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include)
target_link_libraries(vwatch PRIVATE Qt6::Widgets Qt6::Network)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(vwatch PRIVATE -Wall -Wextra)
endif()
