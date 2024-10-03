FILE(GLOB fuzzing_HEADERS *.h)
set(fuzzing_SOURCES
  fuzzing.cpp
  heap-types.cpp
  random.cpp
  ${fuzzing_HEADERS}
)
add_library(fuzzing OBJECT ${fuzzing_SOURCES})

