# CMakeList.txt : CMake project for TinyRaycaster, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_package(SFML 2 COMPONENTS graphics REQUIRED)

# Add source to this project's executable.
add_executable (TinyRaycaster "TinyRaycaster.cpp" "TinyRaycaster.h")
target_link_libraries(TinyRaycaster sfml-graphics)


function(add_assets tgt lst)
    foreach (asset IN ITEMS ${lst})
        add_custom_command(
            TARGET ${tgt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
          	    ${asset}
                ${CMAKE_CURRENT_BINARY_DIR}
        )
    endforeach(asset)
endfunction(add_assets)

file(GLOB assets ${CMAKE_SOURCE_DIR}/*.png)
add_assets(TinyRaycaster ${assets})

#add_custom_command(
#  TARGET TinyRaycaster POST_BUILD
#  COMMAND copy_assets(${assets})
#)

#add_custom_command(
#  TARGET TinyRaycaster POST_BUILD
#  COMMAND ${CMAKE_COMMAND} -E copy
#          	${IMAGES}
#            ${CMAKE_CURRENT_BINARY_DIR}
#)


# TODO: Add tests and install targets if needed.
