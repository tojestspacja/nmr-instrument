# The DSP of the shared physics core (../../../physics) and the generated instrument configuration (../../../generated).
set(R ${CMAKE_CURRENT_LIST_DIR}/../../..)
idf_component_register(SRCS ${R}/physics/src/dsp.cpp ${R}/physics/src/pipeline.cpp
                       INCLUDE_DIRS ${R}/physics/include ${R}/generated)
