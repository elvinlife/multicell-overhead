add_library(util util.cpp ue_context.cpp slice_context.cpp cell_context.cpp)
set(FLAGS "-Wall -O3 -pthread -g")
set_target_properties(util PROPERTIES
    COMPILE_FLAGS "${FLAGS}")
