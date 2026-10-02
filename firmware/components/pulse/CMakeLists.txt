# The pulse-program library shared with the host tools and the simulator (../../../pulse). Not copied.
set(P ${CMAKE_CURRENT_LIST_DIR}/../../../pulse)
idf_component_register(SRCS ${P}/src/program.cpp ${P}/src/compiler.cpp ${P}/src/backend.cpp
                       INCLUDE_DIRS ${P}/include)
