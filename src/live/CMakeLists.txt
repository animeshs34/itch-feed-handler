add_library(fh_live STATIC
  live.cpp)
target_link_libraries(fh_live PUBLIC fh_nasdaq fh_sim fh_net)
