add_library(fh_bench STATIC
  histogram.cpp
  open_loop.cpp
  environment.cpp
  report.cpp
  workload.cpp
  closed_loop.cpp
  rate_sweep.cpp
  end_to_end.cpp
  recovery.cpp)
target_link_libraries(fh_bench PUBLIC fh_nasdaq fh_sim fh_net fh_live)
target_compile_definitions(fh_bench PRIVATE FH_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
