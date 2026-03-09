if(NOT DEFINED TARGET_FILE_DIR OR TARGET_FILE_DIR STREQUAL "")
    message(WARNING "TARGET_FILE_DIR is empty. Skipping vcpkg runtime deployment.")
    return()
endif()

if(NOT DEFINED VCPKG_INSTALLED OR VCPKG_INSTALLED STREQUAL "")
    message(WARNING "VCPKG_INSTALLED is empty. Skipping vcpkg runtime deployment.")
    return()
endif()

if(NOT EXISTS "${TARGET_FILE_DIR}")
    file(MAKE_DIRECTORY "${TARGET_FILE_DIR}")
endif()

set(_runtime_bin "")
if(CONFIG STREQUAL "Debug")
    if(EXISTS "${VCPKG_INSTALLED}/debug/bin")
        set(_runtime_bin "${VCPKG_INSTALLED}/debug/bin")
    elseif(EXISTS "${VCPKG_INSTALLED}/bin")
        set(_runtime_bin "${VCPKG_INSTALLED}/bin")
    endif()
else()
    if(EXISTS "${VCPKG_INSTALLED}/bin")
        set(_runtime_bin "${VCPKG_INSTALLED}/bin")
    endif()
endif()

if(_runtime_bin STREQUAL "")
    message(WARNING "No vcpkg runtime bin directory found under ${VCPKG_INSTALLED}.")
    return()
endif()

file(GLOB _runtime_dlls "${_runtime_bin}/*.dll")
if(NOT _runtime_dlls)
    message(STATUS "No runtime DLLs found in ${_runtime_bin}.")
    return()
endif()

foreach(_dll IN LISTS _runtime_dlls)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_dll}" "${TARGET_FILE_DIR}/"
        RESULT_VARIABLE _copy_result
    )
    if(NOT _copy_result EQUAL 0)
        message(WARNING "Failed to copy runtime DLL: ${_dll}")
    endif()
endforeach()

message(STATUS "Copied ${_runtime_dlls} runtime DLLs from ${_runtime_bin} to ${TARGET_FILE_DIR}")
