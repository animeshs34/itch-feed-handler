add_library(fh_sim STATIC
  ground_truth.cpp
  day.cpp
  wire.cpp
  snapshots.cpp
  trace.cpp
  faults.cpp
  delivery.cpp
  main_faults.cpp
  snapshot_faults.cpp
  scenario.cpp
  consistency.cpp)
target_link_libraries(fh_sim PUBLIC fh_nasdaq fh_testkit)
