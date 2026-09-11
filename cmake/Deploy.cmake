if(APPLE OR (WIN32 AND NOT STATIC))
    add_custom_target(deploy)
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

    if(APPLE AND NOT IOS)
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
        find_program(_macos_otool NAMES otool REQUIRED)

        # Seed libraries missed by Qt 5 before deployment, so macdeployqt
        # also rewrites their dependencies to bundled libraries.
        set(_extra_macos_runtimes)
        if(USE_DEVICE_TREZOR)
            list(APPEND _extra_macos_runtimes utf8_validity)
        endif()
        if(NOT STATIC)
            list(APPEND _extra_macos_runtimes sharpyuv)
        endif()
        set(_saved_library_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
        set(CMAKE_FIND_LIBRARY_SUFFIXES .dylib)
        foreach(_runtime IN LISTS _extra_macos_runtimes)
            find_library(_macos_${_runtime}_runtime NAMES ${_runtime})
            if(_macos_${_runtime}_runtime)
                get_filename_component(_runtime_real "${_macos_${_runtime}_runtime}" REALPATH)
                # The load name can differ from the full versioned filename.
                execute_process(COMMAND "${_macos_otool}" -D "${_runtime_real}"
                    OUTPUT_VARIABLE _runtime_install_names OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY)
                string(REGEX REPLACE ".*\n" "" _runtime_install_name "${_runtime_install_names}")
                string(STRIP "${_runtime_install_name}" _runtime_install_name)
                get_filename_component(_runtime_name "${_runtime_install_name}" NAME)
                add_custom_command(TARGET deploy POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:salvium-wallet-gui>/../Frameworks"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_runtime_real}" "$<TARGET_FILE_DIR:salvium-wallet-gui>/../Frameworks/${_runtime_name}"
                    COMMENT "Copying ${_runtime} runtime library")
            endif()
        endforeach()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ${_saved_library_suffixes})

        # workaround for a Qt bug that requires manually adding libqsvg.dylib to bundle
        # Try to locate libqsvg.dylib in any known Qt plugin directory
        set(_qt_plugin_search_paths)
        if(CMAKE_PREFIX_PATH)
            foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
                list(APPEND _qt_plugin_search_paths "${_prefix}/plugins/imageformats")
            endforeach()
        endif()
        if(DEFINED QT_INSTALL_PREFIX)
            list(APPEND _qt_plugin_search_paths "${QT_INSTALL_PREFIX}/plugins/imageformats")
        endif()
        if(DEFINED Qt5Svg_DIR)
            list(APPEND _qt_plugin_search_paths "${Qt5Svg_DIR}/../../../plugins/imageformats")
        endif()

        list(REMOVE_DUPLICATES _qt_plugin_search_paths)

        find_file(_qt_svg_dylib "libqsvg.dylib"
                  PATHS ${_qt_plugin_search_paths}
                  NO_DEFAULT_PATH)
        
        if(_qt_svg_dylib)
            add_custom_command(TARGET deploy POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:salvium-wallet-gui>/../PlugIns/imageformats"
                COMMAND ${CMAKE_COMMAND} -E copy "${_qt_svg_dylib}" "$<TARGET_FILE_DIR:salvium-wallet-gui>/../PlugIns/imageformats/"
                COMMENT "Copying libqsvg.dylib..."
            )
        endif()
        
        if(NOT STATIC)
            # macdeployqt misses Boost dependencies referenced via @loader_path.
            # Filesystem needs Atomic; ProgramOptions, Serialization and Thread
            # also need Container and DateTime in current Homebrew builds.
            find_package(Boost CONFIG REQUIRED COMPONENTS atomic container date_time)
            foreach(_boost_runtime atomic container date_time)
                get_target_property(_boost_runtime_path Boost::${_boost_runtime} LOCATION)
                if(EXISTS "${_boost_runtime_path}")
                    add_custom_command(TARGET deploy POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:salvium-wallet-gui>/../Frameworks"
                        COMMAND ${CMAKE_COMMAND} -E copy "${_boost_runtime_path}" "$<TARGET_FILE_DIR:salvium-wallet-gui>/../Frameworks/"
                        COMMENT "Copying Boost.${_boost_runtime} runtime library")
                endif()
            endforeach()
        endif()

        add_custom_command(TARGET deploy POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "${CMAKE_BINARY_DIR}/bin/salvium-wallet-gui.app" -always-overwrite -qmldir="${CMAKE_SOURCE_DIR}"
            COMMENT "Running macdeployqt...")

        if(NOT STATIC)
            find_package(Python3 REQUIRED COMPONENTS Interpreter)
            add_custom_command(TARGET deploy POST_BUILD
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/share/fix_qt_paths.py" "${CMAKE_BINARY_DIR}/bin/salvium-wallet-gui.app"
                COMMENT "Fixing bundled Qt paths")
        endif()

        # Apple Silicon requires all binaries to be codesigned
        find_program(CODESIGN_EXECUTABLE NAMES codesign)
        if(CODESIGN_EXECUTABLE)
            add_custom_command(TARGET deploy
                            POST_BUILD
                            COMMAND "${CODESIGN_EXECUTABLE}" --force --deep --sign - "${CMAKE_BINARY_DIR}/bin/salvium-wallet-gui.app"
                            COMMENT "Running codesign..."
            )
        endif()

    elseif(WIN32)
        find_program(QMAKE_EXECUTABLE qmake HINTS "${_qt_bin_dir}")
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
        if(NOT QMAKE_EXECUTABLE OR NOT WINDEPLOYQT_EXECUTABLE)
            message(WARNING "Deploy requires qmake.exe and windeployqt.exe (no -qt5 suffix) in ${_qt_bin_dir}")
        endif()
        add_custom_command(TARGET deploy POST_BUILD
                           COMMAND "${CMAKE_COMMAND}" -E env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:salvium-wallet-gui>" -no-translations -qmldir="${CMAKE_SOURCE_DIR}"
                           COMMENT "Running windeployqt..."
        )
        set(WIN_DEPLOY_DLLS
            libboost_chrono-mt.dll
            libboost_filesystem-mt.dll
            libboost_locale-mt.dll
            libboost_program_options-mt.dll
            libboost_serialization-mt.dll
            libboost_thread-mt.dll
            libprotobuf.dll
            libbrotlicommon.dll
            libbrotlidec.dll
            libusb-1.0.dll
            zlib1.dll
            libzstd.dll
            libwinpthread-1.dll
            libtiff-6.dll
            libstdc++-6.dll
            libpng16-16.dll
            libpcre16-0.dll
            libpcre-1.dll
            libmng-2.dll
            liblzma-5.dll
            liblcms2-2.dll
            libjpeg-8.dll
            libintl-8.dll
            libiconv-2.dll
            libharfbuzz-0.dll
            libgraphite2.dll
            libglib-2.0-0.dll
            libfreetype-6.dll
            libbz2-1.dll
            libpcre2-16-0.dll
            libhidapi-0.dll
            libdouble-conversion.dll
            libgcrypt-20.dll
            libgpg-error-0.dll
            libsodium-26.dll
            libzmq.dll
            #platform files
            libgcc_s_seh-1.dll
            #openssl files
            libssl-3-x64.dll
            libcrypto-3-x64.dll
            #icu
            libicudt78.dll
            libicuin78.dll
            libicuio78.dll
            libicutu78.dll
            libicuuc78.dll
        )

        # Boost Regex is header-only since 1.77
        if (Boost_VERSION_STRING VERSION_LESS 1.77.0)
            list(APPEND WIN_DEPLOY_DLLS libboost_regex-mt.dll)
        endif()

        list(TRANSFORM WIN_DEPLOY_DLLS PREPEND "$ENV{MSYSTEM_PREFIX}/bin/")
        add_custom_command(TARGET deploy
                           POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy ${WIN_DEPLOY_DLLS} "$<TARGET_FILE_DIR:salvium-wallet-gui>"
                           COMMENT "Copying DLLs to target folder"
        )
    endif()
endif()
