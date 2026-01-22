cmake_minimum_required(VERSION 3.20)
project(fluxc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(fluxc STATIC
  src/error.c
  src/lexer.c
  src/value.c
  src/ast.c
  src/parser.c
  src/eval.c
  src/flux_api.c
)

target_include_directories(fluxc PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(fluxc PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
  target_compile_options(fluxc PRIVATE /W4)
else()
  target_compile_options(fluxc PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- IMPORTANT FOR rust build.rs (cmake crate): create an install target ---
install(TARGETS fluxc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
