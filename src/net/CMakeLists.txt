add_library(fh_net STATIC
  udp.cpp
  pump.cpp)
target_link_libraries(fh_net PUBLIC fh_core)
