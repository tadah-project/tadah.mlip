cmake_minimum_required(VERSION 3.12)

project(Tadah.MLIP)

file(GLOB_RECURSE MLIP_SRC CONFIGURE_DEPENDS "*.cpp")

add_library(tadah.mlip
    ${MLIP_SRC}
    )

