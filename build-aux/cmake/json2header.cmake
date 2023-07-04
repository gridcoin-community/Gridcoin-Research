cmake_minimum_required(VERSION 3.19)

if(CMAKE_ARGC LESS 6)
    message(FATAL_ERROR "Usage: cmake -P json2header.cmake xxd in_file out_file")
endif()

set(XXD ${CMAKE_ARGV3})
set(IN_FILE ${CMAKE_ARGV4})
set(OUT_FILE ${CMAKE_ARGV5})

get_filename_component(var_name "${IN_FILE}" NAME_WE)
execute_process(COMMAND "${XXD}" -i -n ${var_name} "${IN_FILE}"
    OUTPUT_VARIABLE command_out
    COMMAND_ERROR_IS_FATAL ANY
)

file(WRITE ${OUT_FILE} "namespace json_tests {\n")
file(APPEND "${OUT_FILE}" "${command_out}")
file(APPEND ${OUT_FILE} "};\n")
