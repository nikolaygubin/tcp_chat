set(target_name client)

add_executable(${target_name})

include(CompileOptions)
set_compile_options(${target_name})

target_sources(
  ${target_name}
  PRIVATE
    client.cpp
)

target_link_libraries(
  ${target_name}
  PRIVATE
    nlohmann_json
)

