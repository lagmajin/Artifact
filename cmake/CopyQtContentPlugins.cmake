if(NOT DEFINED TARGET_FILE_DIR OR TARGET_FILE_DIR STREQUAL "")
    message(WARNING "TARGET_FILE_DIR is empty. Skipping Qt content plugin deployment.")
    return()
endif()

if(NOT DEFINED VCPKG_INSTALLED OR VCPKG_INSTALLED STREQUAL "")
    message(WARNING "VCPKG_INSTALLED is empty. Skipping Qt content plugin deployment.")
    return()
endif()

if(CONFIG STREQUAL "Debug")
    set(_qt_plugin_root "${VCPKG_INSTALLED}/debug/Qt6/plugins")
else()
    set(_qt_plugin_root "${VCPKG_INSTALLED}/Qt6/plugins")
endif()

foreach(_plugin_category IN ITEMS imageformats multimedia)
    set(_source_dir "${_qt_plugin_root}/${_plugin_category}")
    if(NOT EXISTS "${_source_dir}")
        message(STATUS "No Qt ${_plugin_category} plugins found at ${_source_dir}.")
        continue()
    endif()

    file(GLOB _plugin_files "${_source_dir}/*.dll")
    if(NOT _plugin_files)
        message(STATUS "No Qt ${_plugin_category} plugin DLLs found at ${_source_dir}.")
        continue()
    endif()

    set(_destination_dir "${TARGET_FILE_DIR}/plugins/${_plugin_category}")
    file(MAKE_DIRECTORY "${_destination_dir}")
    foreach(_plugin_file IN LISTS _plugin_files)
        get_filename_component(_plugin_name "${_plugin_file}" NAME)
        file(COPY_FILE "${_plugin_file}"
            "${_destination_dir}/${_plugin_name}"
            ONLY_IF_DIFFERENT
        )
    endforeach()
endforeach()
