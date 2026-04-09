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

set(_tls_dst "${TARGET_FILE_DIR}/tls")
file(MAKE_DIRECTORY "${_tls_dst}")

set(_plugins_tls_dst "${TARGET_FILE_DIR}/plugins/tls")
file(MAKE_DIRECTORY "${_plugins_tls_dst}")

set(_candidates "")
if(CONFIG STREQUAL "Debug")
    list(APPEND _candidates
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/platforms/qwindowsd.dll"
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/platforms/qwindows.dll"
    )
else()
    list(APPEND _candidates
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/platforms/qwindows.dll"
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/platforms/qwindows.dll"
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

set(_tls_candidates "")
if(CONFIG STREQUAL "Debug")
    list(APPEND _tls_candidates
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/tls/qopensslbackend.dll"
    )
else()
    list(APPEND _tls_candidates
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/tls/qopensslbackend.dll"
        "${VCPKG_INSTALLED}/x64-windows/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/x64-windows/debug/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/Qt6/plugins/tls/qopensslbackendd.dll"
        "${VCPKG_INSTALLED}/debug/Qt6/plugins/tls/qopensslbackendd.dll"
    )
endif()

set(_tls_copied FALSE)
foreach(_src IN LISTS _tls_candidates)
    if(EXISTS "${_src}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${_tls_dst}/"
            RESULT_VARIABLE _copy_result
        )
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_src}" "${_plugins_tls_dst}/"
            RESULT_VARIABLE _copy_result_plugins
        )
        if(_copy_result EQUAL 0 AND _copy_result_plugins EQUAL 0)
            set(_tls_copied TRUE)
            message(STATUS "Copied Qt TLS plugin: ${_src}")
            break()
        endif()
    endif()
endforeach()

if(NOT _tls_copied)
    message(WARNING
        "Qt TLS plugin was not found. Tried:\n  ${_tls_candidates}\n"
        "HTTPS requests may fail with 'TLS initialization failed'."
    )
endif()
