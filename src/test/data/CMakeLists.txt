set(JSON_TEST_FILES
    base58_keys_valid.json
    base58_encode_decode.json
    base58_keys_invalid.json
    script_valid.json
    script_invalid.json
    tx_invalid.json
    tx_valid.json
)

set(BINARY_TEST_FILES
    mainnet_beacon.bin
    superblock_packed.bin
    testnet_beacon.bin
)

set(TEXT_TEST_FILES
    superblock.txt
    superblock_unpacked.txt
)

foreach(file_in IN LISTS JSON_TEST_FILES)
    set(file_out ${CMAKE_CURRENT_BINARY_DIR}/${file_in}.h)
    add_custom_command(OUTPUT ${file_in}.h
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/build-aux/cmake/json2header.cmake ${XXD} ${CMAKE_CURRENT_SOURCE_DIR}/${file_in} ${file_out}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file_in}
    )
    add_custom_target(generate_${file_in}.h DEPENDS ${file_in}.h)
    add_dependencies(test_gridcoin generate_${file_in}.h)
endforeach()

foreach(file_in IN LISTS BINARY_TEST_FILES)
    get_filename_component(var_name ${file_in} NAME_WE)
    add_custom_command(OUTPUT ${file_in}.h
        COMMAND ${XXD} -i -n ${var_name}_bin ${CMAKE_CURRENT_SOURCE_DIR}/${file_in} ${CMAKE_CURRENT_BINARY_DIR}/${file_in}.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file_in}
    )
    add_custom_target(generate_${file_in}.h DEPENDS ${file_in}.h)
    add_dependencies(test_gridcoin generate_${file_in}.h)
endforeach()

foreach(file_in IN LISTS TEXT_TEST_FILES)
    set(file_out ${CMAKE_CURRENT_BINARY_DIR}/${file_in}.h)
    add_custom_command(OUTPUT ${file_in}.h
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/build-aux/cmake/txt2header.cmake ${CMAKE_CURRENT_SOURCE_DIR}/${file_in} ${file_out}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file_in}
    )
    add_custom_target(generate_${file_in}.h DEPENDS ${file_in}.h)
    add_dependencies(test_gridcoin generate_${file_in}.h)
endforeach()
