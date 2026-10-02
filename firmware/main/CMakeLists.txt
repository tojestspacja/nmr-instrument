# The control plane: state machine + experiment sequencer. Depends on the hardware components and, through nmrdsp, on
# the shared physics/DSP core and the generated instrument config; through pulse on the shared pulse library.
idf_component_register(SRCS main.cpp controller.cpp
                       INCLUDE_DIRS include
                       REQUIRES engine adc rf pulse nmrdsp)
