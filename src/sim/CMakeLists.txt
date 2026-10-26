add_library(fh_sim STATIC
  ground_truth.cpp
  day.cpp
  wire.cpp
  snapshots.cpp
  consistency.cpp)
target_link_libraries(fh_sim PUBLIC fh_nasdaq fh_testkit)
