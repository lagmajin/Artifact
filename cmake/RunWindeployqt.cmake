if(NOT DEFINED WINDEPLOYQT_EXECUTABLE OR WINDEPLOYQT_EXECUTABLE STREQUAL "")
    message(WARNING "windeployqt path is empty. Skipping Qt deployment.")
    return()
endif()

if(NOT DEFINED TARGET_FILE OR TARGET_FILE STREQUAL "")
    message(WARNING "TARGET_FILE is empty. Skipping Qt deployment.")
    return()
endif()

if(NOT DEFINED VCPKG_INSTALLED OR VCPKG_INSTALLED STREQUAL "")
    message(WARNING "VCPKG_INSTALLED is empty. Skipping Qt deployment.")
    return()
endif()

set(_qt_bin_release "${VCPKG_INSTALLED}/bin")
set(_qt_bin_debug "${VCPKG_INSTALLED}/debug/bin")

if(CONFIG STREQUAL "Debug")
    set(_deploy_mode "debug")
    set(ENV{PATH} "${_qt_bin_debug};${_qt_bin_release};$ENV{PATH}")
else()
    set(_deploy_mode "release")
    set(ENV{PATH} "${_qt_bin_release};${_qt_bin_debug};$ENV{PATH}")
endif()

execute_process(
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" "--${_deploy_mode}" --no-translations "${TARGET_FILE}"
    RESULT_VARIABLE _deploy_result
    OUTPUT_VARIABLE _deploy_out
    ERROR_VARIABLE _deploy_err
)

if(NOT _deploy_result EQUAL 0)
    string(STRIP "${_deploy_err}" _deploy_err)
    string(STRIP "${_deploy_out}" _deploy_out)
    message(WARNING
        "windeployqt failed (code ${_deploy_result}) for ${TARGET_FILE}.\n"
        "stderr: ${_deploy_err}\n"
        "stdout: ${_deploy_out}\n"
        "Build will continue."
    )
endif()
