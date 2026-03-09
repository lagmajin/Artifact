if(NOT DEFINED TARGET_FILE_DIR OR TARGET_FILE_DIR STREQUAL "")
    message(WARNING "TARGET_FILE_DIR is empty. Skipping Qt platform plugin copy.")
    return()
endif()

if(NOT DEFINED VCPKG_INSTALLED OR VCPKG_INSTALLED STREQUAL "")
    message(WARNING "VCPKG_INSTALLED is empty. Skipping Qt platform plugin copy.")
    return()
endif()

set(_platforms_dst "${TARGET_FILE_DIR}/platforms")
file(MAKE_DIRECTORY "${_platforms_dst}")

set(_candidates "")
if(CONFIG STREQUAL "Debug")
    list(APPEND _candidates
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/platforms/qwindows.dll"
    )
else()
    list(APPEND _candidates
        "${VCPKG_INSTALLED}/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/platforms/qwindows.dll"
    )
endif()

set(_copied FALSE)
foreach(_src IN LISTS _candidates)
    if(EXISTS "${_src}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${_platforms_dst}/"
            RESULT_VARIABLE _copy_result
        )
        if(_copy_result EQUAL 0)
            set(_copied TRUE)
            message(STATUS "Copied Qt platform plugin: ${_src}")
            break()
        endif()
    endif()
endforeach()

if(NOT _copied)
    message(WARNING
        "Qt platform plugin was not found. Tried:\n  ${_candidates}\n"
        "App may fail with 'Could not find the Qt platform plugin \"windows\"'."
    )
endif()
