function(kabot_generate_version VERSION_FILE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    execute_process(
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../scripts/firmware_version.py
            --version-file
            ${VERSION_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..
        RESULT_VARIABLE KABOT_VERSION_RESULT
    )

    if(NOT KABOT_VERSION_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate ${VERSION_FILE}")
    endif()
endfunction()