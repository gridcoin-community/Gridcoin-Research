cmake_minimum_required(VERSION 3.19)

if(CMAKE_ARGC LESS 5)
    message(FATAL_ERROR "Usage: cmake -P txt2header.cmake in_file out_file")
endif()

set(IN_FILE ${CMAKE_ARGV3})
set(OUT_FILE ${CMAKE_ARGV4})

get_filename_component(var_name "${IN_FILE}" NAME_WE)

file(READ ${IN_FILE} text)
file(WRITE "${OUT_FILE}" "
    static const std::string ${var_name}_text = R\"(${text})\";
")
