# CMakeList.txt : CMake project for TinyRaycaster, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(SFML 2 COMPONENTS graphics REQUIRED)

# Add source to this project's executable.
add_executable (TinyRaycaster "TinyRaycaster.cpp" "TinyRaycaster.h")
target_link_libraries(TinyRaycaster sfml-graphics)

add_custom_command(
  TARGET TinyRaycaster POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
          	${CMAKE_SOURCE_DIR}/walltext.png
            ${CMAKE_CURRENT_BINARY_DIR}/walltext.png
)


# TODO: Add tests and install targets if needed.
