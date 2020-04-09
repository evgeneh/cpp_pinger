cmake_minimum_required( VERSION 3.13 )

project (Server)
# Find necessary packages
find_package( Threads )
find_package( Boost  COMPONENTS system thread )

include_directories(${CMAKE_SOURCE_DIR} ${Boost_INCLUDE_DIRS})



# Build our app
add_executable( server server.cpp )

# Define the wrapper library that wraps our library
#target_link_libraries( server ${CMAKE_PROJECT_NAME} Threads::Threads)
target_link_libraries( server ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} )

#target_link_libraries( server ${CMAKE_PROJECT_NAME})

# don't prepend wrapper library name with lib
set_target_properties( server PROPERTIES PREFIX "" )

