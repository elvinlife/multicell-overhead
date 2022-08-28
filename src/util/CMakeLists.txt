add_library(util util.cpp ue_context.cpp slice_context.cpp scheduler_context.cpp)
set(FLAGS "-Wall -O3 -pthread")
set_target_properties(util PROPERTIES
    COMPILE_FLAGS "${FLAGS}")