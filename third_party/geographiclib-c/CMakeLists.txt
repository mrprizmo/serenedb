add_library(geographiclib-c
  geodesic.c)

target_include_directories(geographiclib-c PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR})
