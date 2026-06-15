if(NOT DEFINED BUNDLE_PATH)
    message(FATAL_ERROR "BUNDLE_PATH is required")
endif()

if(NOT DEFINED BUNDLE_SIGNATURE)
    message(FATAL_ERROR "BUNDLE_SIGNATURE is required")
endif()

string(LENGTH "${BUNDLE_SIGNATURE}" signature_length)
if(NOT signature_length EQUAL 4)
    message(FATAL_ERROR "BUNDLE_SIGNATURE must be exactly four characters")
endif()

set(contents_dir "${BUNDLE_PATH}/Contents")
set(info_plist "${contents_dir}/Info.plist")
set(pkg_info "${contents_dir}/PkgInfo")

if(NOT EXISTS "${contents_dir}")
    message(FATAL_ERROR "VST3 bundle contents not found: ${contents_dir}")
endif()

if(NOT EXISTS "${info_plist}")
    message(FATAL_ERROR "VST3 Info.plist not found: ${info_plist}")
endif()

execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Set :CFBundleSignature ${BUNDLE_SIGNATURE}" "${info_plist}"
    RESULT_VARIABLE set_signature_result
    OUTPUT_VARIABLE set_signature_output
    ERROR_VARIABLE set_signature_error)

if(NOT set_signature_result EQUAL 0)
    execute_process(
        COMMAND /usr/libexec/PlistBuddy -c "Add :CFBundleSignature string ${BUNDLE_SIGNATURE}" "${info_plist}"
        RESULT_VARIABLE add_signature_result
        OUTPUT_VARIABLE add_signature_output
        ERROR_VARIABLE add_signature_error)

    if(NOT add_signature_result EQUAL 0)
        message(FATAL_ERROR
            "Could not write CFBundleSignature in ${info_plist}\n"
            "${set_signature_output}${set_signature_error}"
            "${add_signature_output}${add_signature_error}")
    endif()
endif()

file(WRITE "${pkg_info}" "BNDL${BUNDLE_SIGNATURE}")

execute_process(
    COMMAND /usr/bin/codesign --force --deep --sign - "${BUNDLE_PATH}"
    RESULT_VARIABLE codesign_result
    OUTPUT_VARIABLE codesign_output
    ERROR_VARIABLE codesign_error)

if(NOT codesign_result EQUAL 0)
    message(FATAL_ERROR
        "Could not ad-hoc sign ${BUNDLE_PATH}\n"
        "${codesign_output}${codesign_error}")
endif()
