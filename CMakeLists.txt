cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(MemBlock VERSION 0.1)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Include directories
include_directories(include)

# Can manually set pthreads flag on UNIX
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Find the Threads package (pthreads on UNIX)
find_package(Threads REQUIRED)

# Add the executable
add_executable(MemBlock app/server.c app/network/thread_pool.c)

# Link pthread library to the executable
# Replace 'Threads::Threads' with '${CMAKE_THREAD_LIBS_INIT}' if you encounter issues
target_link_libraries(MemBlock Threads::Threads)

