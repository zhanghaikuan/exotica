cmake_minimum_required(VERSION 2.8.3)
project(ik_solver_demo)

find_package(catkin REQUIRED COMPONENTS
  exotica
  sensor_msgs
)

catkin_package(
 CATKIN_DEPENDS exotica sensor_msgs
 DEPENDS system_lib
)

###########
## Build ##
###########

include_directories(
  ${catkin_INCLUDE_DIRS}
  include
)

## Declare a cpp executable
add_executable(IKexample src/node.cpp)

## Specify libraries to link a library or executable target against
target_link_libraries(IKexample
  ${catkin_LIBRARIES}
)
