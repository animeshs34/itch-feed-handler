add_library(fh_nasdaq STATIC
  be_reader.cpp
  binary_file.cpp
  itch_decoder.cpp
  itch_encoder.cpp
  moldudp64.cpp
  snapshot_decoder.cpp)
target_link_libraries(fh_nasdaq PUBLIC fh_core)
