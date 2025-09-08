cmake_minimum_required(VERSION 3.10)
project(MyShell C)

# Enable debug flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

# Add the executable
add_executable(myshell main.c)
