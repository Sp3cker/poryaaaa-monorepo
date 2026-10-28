cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
project(poryaaaa_quicklook VERSION 0.1.0 LANGUAGES C)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../poryaaaa/plugin/porydaw"
                 "${CMAKE_CURRENT_BINARY_DIR}/engine")

add_library(poryaaaa_quicklook_bank STATIC src/quicklook_bank.c)
target_include_directories(poryaaaa_quicklook_bank PUBLIC include)
target_link_libraries(poryaaaa_quicklook_bank PUBLIC poryaaaa::engine)

add_executable(bake_voicegroup tools/bake_voicegroup.c)
target_link_libraries(bake_voicegroup PRIVATE poryaaaa::engine)

add_executable(test_quicklook_bank tests/test_quicklook_bank.c)
target_link_libraries(test_quicklook_bank PRIVATE poryaaaa_quicklook_bank)
add_test(NAME quicklook_bank COMMAND test_quicklook_bank
         "${CMAKE_CURRENT_SOURCE_DIR}/assets/hanabi.pbank")

if(APPLE)
    find_program(PQL_SWIFTC swiftc REQUIRED)
    execute_process(COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE PQL_SDK OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND xcrun --find swiftc
        OUTPUT_VARIABLE PQL_TOOLCHAIN_SWIFTC OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    get_filename_component(PQL_TOOLCHAIN_BIN "${PQL_TOOLCHAIN_SWIFTC}" DIRECTORY)
    set(PQL_TEST_FRAMEWORKS "${PQL_SDK}/../../../Developer/Library/Frameworks")
    set(PQL_MODULES "${CMAKE_CURRENT_BINARY_DIR}/swift-modules")
    file(MAKE_DIRECTORY "${PQL_MODULES}")
    set(PQL_BANK_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/poryaaaa_quicklook_bank.h")
    set(PQL_BLOB_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/swift/interop/embedded_hanabi.h")
    get_target_property(PQL_ENGINE_INCLUDES poryaaaa_engine INTERFACE_INCLUDE_DIRECTORIES)
    list(GET PQL_ENGINE_INCLUDES 0 PQL_ENGINE_INCLUDE)
    set(PQL_ENGINE_HEADER "${PQL_ENGINE_INCLUDE}/m4a_engine.h")
    configure_file(swift/interop/module.modulemap.in "${PQL_MODULES}/module.modulemap" @ONLY)

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_hanabi.c"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/assets/hanabi.pbank"
            "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded_hanabi.c"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_blob.cmake"
        DEPENDS assets/hanabi.pbank cmake/embed_blob.cmake
        VERBATIM)
    add_library(pql_embedded_hanabi STATIC "${CMAKE_CURRENT_BINARY_DIR}/embedded_hanabi.c")
    target_include_directories(pql_embedded_hanabi PRIVATE swift/interop)

    # Invoke the Swift driver explicitly: the existing Unix Makefiles build tree cannot
    # enable CMake's Swift language (which requires Ninja or Xcode). All outputs/deps
    # remain CMake-owned; native MACOSX_BUNDLE targets own packaging and post-build signing.
    set(PQL_SWIFT_FLAGS -swift-version 6 -strict-concurrency=complete -O
        -sdk "${PQL_SDK}" -target "${CMAKE_SYSTEM_PROCESSOR}-apple-macosx${CMAKE_OSX_DEPLOYMENT_TARGET}"
        -I "${PQL_MODULES}" -Xcc "-I${CMAKE_CURRENT_SOURCE_DIR}/include")
    foreach(include_dir IN LISTS PQL_ENGINE_INCLUDES)
        list(APPEND PQL_SWIFT_FLAGS -Xcc "-I${include_dir}")
    endforeach()
    set(PQL_SWIFT_SOURCES
        swift/StreamEvent.swift swift/MidiArena.swift swift/MidiStreamReader.swift swift/MidiNoteIndex.swift
        swift/EventFIFO.swift swift/StreamEngineCore.swift swift/StreamPlayer.swift swift/LivePlayer.swift)
    list(TRANSFORM PQL_SWIFT_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
    set(PQL_PLAYER_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/libQuickLookPlayer.a")
    set(PQL_PLAYER_MODULE "${PQL_MODULES}/QuickLookPlayer.swiftmodule")
    add_custom_command(
        OUTPUT "${PQL_PLAYER_ARCHIVE}" "${PQL_PLAYER_MODULE}"
        COMMAND "${PQL_SWIFTC}" ${PQL_SWIFT_FLAGS} -parse-as-library
            -emit-library -static -emit-module -module-name QuickLookPlayer
            -emit-module-path "${PQL_PLAYER_MODULE}" ${PQL_SWIFT_SOURCES} -o "${PQL_PLAYER_ARCHIVE}"
        DEPENDS ${PQL_SWIFT_SOURCES} "${PQL_MODULES}/module.modulemap"
            "${PQL_BANK_HEADER}" "${PQL_BLOB_HEADER}" "${PQL_ENGINE_HEADER}"
        VERBATIM)
    add_custom_target(quicklook_player ALL DEPENDS "${PQL_PLAYER_ARCHIVE}" "${PQL_PLAYER_MODULE}")
    set(PQL_C_LIBRARIES poryaaaa_quicklook_bank pql_embedded_hanabi poryaaaa_engine m4a_driver hw_audio)
    set(PQL_LINK_FILES "${PQL_PLAYER_ARCHIVE}")
    foreach(library IN LISTS PQL_C_LIBRARIES)
        list(APPEND PQL_LINK_FILES "$<TARGET_FILE:${library}>")
    endforeach()

    function(pql_swift_tool target source)
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}.stamp"
            BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${target}"
            COMMAND "${PQL_SWIFTC}" ${PQL_SWIFT_FLAGS} ${ARGN}
                -parse-as-library "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
                ${PQL_LINK_FILES} -framework AVFoundation -o "${CMAKE_CURRENT_BINARY_DIR}/${target}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/${target}.stamp"
            DEPENDS "${source}" quicklook_player "${PQL_PLAYER_ARCHIVE}" "${PQL_PLAYER_MODULE}" ${PQL_C_LIBRARIES}
            VERBATIM)
        add_custom_target(${target} ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${target}.stamp")
    endfunction()
    pql_swift_tool(quicklook_render tools/quicklook_render.swift)
    pql_swift_tool(test_stream tests/test_stream.swift
        -F "${PQL_TEST_FRAMEWORKS}" -framework Testing
        -load-plugin-library "${PQL_TOOLCHAIN_BIN}/../lib/swift/host/plugins/testing/libTestingMacros.dylib"
        -Xlinker -rpath -Xlinker "${PQL_TEST_FRAMEWORKS}")
    add_test(NAME stream COMMAND "${CMAKE_CURRENT_BINARY_DIR}/test_stream")
    set_tests_properties(stream PROPERTIES TIMEOUT 120
        ENVIRONMENT "PQL_TEST_MIDI=${CMAKE_CURRENT_SOURCE_DIR}/../ccomidi/mus_shop.mid")

    function(pql_swift_bundle target source)
        set(sources "${source}" ${ARGN})
        list(TRANSFORM sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
        set(object "${CMAKE_CURRENT_BINARY_DIR}/${target}.o")
        add_custom_command(
            OUTPUT "${object}"
            COMMAND "${PQL_SWIFTC}" ${PQL_SWIFT_FLAGS} -parse-as-library
                -emit-object -whole-module-optimization -module-name "${target}" ${sources} -o "${object}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${object}"
            DEPENDS ${sources} quicklook_player "${PQL_PLAYER_MODULE}"
            VERBATIM)
        set_source_files_properties("${object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
        add_executable(${target} MACOSX_BUNDLE "${object}")
        set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)
        target_link_directories(${target} PRIVATE "${PQL_TOOLCHAIN_BIN}/../lib/swift/macosx" "${PQL_SDK}/usr/lib/swift")
        target_link_options(${target} PRIVATE "LINKER:-rpath,/usr/lib/swift")
    endfunction()

    pql_swift_bundle(PoryaaaaPreview swift/QuickLookProvider.swift swift/PianoRollView.swift)
    set_target_properties(PoryaaaaPreview PROPERTIES
        BUNDLE_EXTENSION appex MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/swift/PreviewInfo.plist")
    target_link_libraries(PoryaaaaPreview PRIVATE "${PQL_PLAYER_ARCHIVE}" ${PQL_C_LIBRARIES}
        "-framework Foundation" "-framework AppKit" "-framework QuickLookUI" "-framework AVFoundation")
    target_link_options(PoryaaaaPreview PRIVATE "LINKER:-e,_NSExtensionMain" "LINKER:-application_extension")
    set_property(TARGET PoryaaaaPreview APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/swift/PreviewInfo.plist"
        "${CMAKE_CURRENT_SOURCE_DIR}/swift/QuickLookPreview.entitlements")

    pql_swift_bundle(PoryaaaaQuickLook swift/HostApp.swift)
    set_target_properties(PoryaaaaQuickLook PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/swift/HostInfo.plist")
    target_link_libraries(PoryaaaaQuickLook PRIVATE "-framework AppKit")
    add_dependencies(PoryaaaaQuickLook PoryaaaaPreview)
    set_property(TARGET PoryaaaaQuickLook APPEND PROPERTY LINK_DEPENDS
        "$<TARGET_FILE:PoryaaaaPreview>" "${CMAKE_CURRENT_SOURCE_DIR}/swift/HostInfo.plist")
    add_custom_command(TARGET PoryaaaaQuickLook POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:PoryaaaaQuickLook>/Contents/PlugIns"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_BUNDLE_DIR:PoryaaaaPreview>"
            "$<TARGET_BUNDLE_DIR:PoryaaaaQuickLook>/Contents/PlugIns/PoryaaaaPreview.appex"
        COMMAND codesign --force --sign - --entitlements
            "${CMAKE_CURRENT_SOURCE_DIR}/swift/QuickLookPreview.entitlements"
            "$<TARGET_BUNDLE_DIR:PoryaaaaQuickLook>/Contents/PlugIns/PoryaaaaPreview.appex"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:PoryaaaaQuickLook>"
        VERBATIM)
    add_custom_command(TARGET PoryaaaaPreview POST_BUILD
        COMMAND codesign --force --sign - --entitlements
            "${CMAKE_CURRENT_SOURCE_DIR}/swift/QuickLookPreview.entitlements"
            "$<TARGET_BUNDLE_DIR:PoryaaaaPreview>"
        VERBATIM)
endif()
