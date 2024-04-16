set(target_name server)

add_executable(${target_name})

include(CompileOptions)
set_compile_options(${target_name})

target_sources(
  ${target_name}
  PRIVATE
    server.cpp
)

target_link_libraries(
  ${target_name}
  PRIVATE
    nlohmann_json
)
