# Translations
# ============
add_subdirectory(locale)

# Cleanup: Remove stale Qt MOC files
# ==================================
# Autotools (and older build methods) generate .moc and moc_*.cpp files
# directly in the source tree. These conflict with CMake's AUTOMOC.
file(GLOB_RECURSE STALE_MOC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.moc"
    "${CMAKE_CURRENT_SOURCE_DIR}/moc_*.cpp"
)

if(STALE_MOC_FILES)
    message(STATUS "Detected stale Qt MOC files in source tree. Cleaning up...")
    foreach(STALE_FILE ${STALE_MOC_FILES})
        message(STATUS "  Removing: ${STALE_FILE}")
        file(REMOVE "${STALE_FILE}")
    endforeach()
endif()

# libgridcoinqt
# =============

add_library(gridcoinqt STATIC
    aboutdialog.cpp
    addressbookpage.cpp
    addresstablemodel.cpp
    askpassphrasedialog.cpp
    bantablemodel.cpp
    bitcoinaddressvalidator.cpp
    bitcoinamountfield.cpp
    bitcoingui.cpp
    bitcoinunits.cpp
    clicklabel.cpp
    clientmodel.cpp
    coincontroldialog.cpp
    coincontroltreewidget.cpp
    consolidateunspentdialog.cpp
    consolidateunspentwizard.cpp
    consolidateunspentwizardselectdestinationpage.cpp
    consolidateunspentwizardselectinputspage.cpp
    consolidateunspentwizardsendpage.cpp
    csvmodelwriter.cpp
    decoration.cpp
    diagnosticsdialog.cpp
    editaddressdialog.cpp
    editsidestakedialog.cpp
    favoritespage.cpp
    guiutil.cpp
    intro.cpp
    monitoreddatamapper.cpp
    mrcmodel.cpp
    mrcrequestpage.cpp
    noresult.cpp
    notificator.cpp
    optionsdialog.cpp
    optionsmodel.cpp
    overviewpage.cpp
    peertablemodel.cpp
    qtipcserver.cpp
    qvalidatedlineedit.cpp
    qvaluecombobox.cpp
    receivecoinspage.cpp
    researcher/projecttablemodel.cpp
    researcher/researchermodel.cpp
    researcher/researcherwizard.cpp
    researcher/researcherwizardauthpage.cpp
    researcher/researcherwizardbeaconpage.cpp
    researcher/researcherwizardemailpage.cpp
    researcher/researcherwizardnoncruncherpage.cpp
    researcher/researcherwizardmodedetailpage.cpp
    researcher/researcherwizardmodepage.cpp
    researcher/researcherwizardpoolpage.cpp
    researcher/researcherwizardpoolsummarypage.cpp
    researcher/researcherwizardprojectspage.cpp
    researcher/researcherwizardownershipproofpage.cpp
    researcher/researcherwizardsummarypage.cpp
    rpcconsole.cpp
    sendcoinsdialog.cpp
    syncoverlay.cpp
    sendcoinsentry.cpp
    sidestaketablemodel.cpp
    signverifymessagedialog.cpp
    trafficgraphwidget.cpp
    transactiondesc.cpp
    transactiondescdialog.cpp
    transactionfilterproxy.cpp
    transactionrecord.cpp
    transactiontablemodel.cpp
    transactionview.cpp
    updatedialog.cpp
    upgradeqt.cpp
    voting/additionalfieldstableview.cpp
    voting/additionalfieldstablemodel.cpp
    voting/poll_types.cpp
    voting/pollcard.cpp
    voting/pollcardview.cpp
    voting/polldetails.cpp
    voting/pollresultchoiceitem.cpp
    voting/pollresultdialog.cpp
    voting/polltab.cpp
    voting/polltablemodel.cpp
    voting/pollwizard.cpp
    voting/pollwizarddetailspage.cpp
    voting/pollwizardprojectpage.cpp
    voting/pollwizardsummarypage.cpp
    voting/pollwizardtypepage.cpp
    voting/votewizard.cpp
    voting/votewizardballotpage.cpp
    voting/votewizardsummarypage.cpp
    voting/votingmodel.cpp
    voting/votingpage.cpp
    walletmodel.cpp
    winshutdownmonitor.cpp

    bitcoin.qrc
    bitcoin_locale.qrc
)

if(WIN32)
    target_sources(gridcoinqt PRIVATE res/gridcoinresearch.rc)
    set_source_files_properties(res/gridcoinresearch.rc PROPERTIES
        COMPILE_DEFINITIONS WINDRES_PREPROC
    )
elseif(APPLE)
    target_sources(gridcoinqt PRIVATE
        macdockiconhandler.mm
        macnotificationhandler.mm
        macos_appnap.mm
    )
endif()

if(ENABLE_QRENCODE)
    target_sources(gridcoinqt PRIVATE qrcodedialog.cpp)
    target_compile_definitions(gridcoinqt PRIVATE ENABLE_QRENCODE)
endif()

set_target_properties(gridcoinqt PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON

    AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/src;${CMAKE_SOURCE_DIR}/src/qt/forms"
)

# These files include 'node/ui_interface.cpp', which AutoUIC tries to process
set_source_files_properties(
    bitcoin.cpp
    clientmodel.cpp
    mrcmodel.cpp
    qtipcserver.cpp
    researcher/researchermodel.cpp
    sidestaketablemodel.cpp
    transactiondesc.cpp
    transactiontablemodel.cpp
    voting/votingmodel.cpp
    walletmodel.cpp
    PROPERTIES
        SKIP_AUTOUIC ON
)

# Libraries to link
# =================

target_link_libraries(gridcoinqt PUBLIC
    ${QT}::Core
    ${QT}::Concurrent
    ${QT}::Gui
    ${QT}::Network
    ${QT}::Widgets
    ${QT}::Svg
    gridcoin_util
)

if(USE_QT6)
    target_link_libraries(gridcoinqt PUBLIC ${QT}::Core5Compat)
endif()
if(USE_DBUS)
    target_link_libraries(gridcoinqt PUBLIC ${QT}::DBus)
endif()
if(ENABLE_QRENCODE)
    target_link_libraries(gridcoinqt PUBLIC PkgConfig::QRENCODE)
endif()

if(APPLE)
    target_link_libraries(gridcoinqt PUBLIC
        "-framework Foundation"
        "-framework ApplicationServices"
        "-framework AppKit"
    )
elseif(WIN32)
    target_link_libraries(gridcoinqt PUBLIC shlwapi)
endif()

target_compile_definitions(gridcoinqt PUBLIC HAVE_CONFIG_H)
target_compile_definitions(gridcoinqt PUBLIC QT_DISABLE_DEPRECATED_UP_TO=0x050F00)

if(ENABLE_UPNP)
    if(DEFAULT_UPNP)
        target_compile_definitions(gridcoinqt PRIVATE USE_UPNP=1)
    else()
        target_compile_definitions(gridcoinqt PRIVATE USE_UPNP=0)
    endif()
endif()

add_dependencies(gridcoinqt gridcoinqt_l10n)


# Application
# ===========

# Prepare resources (Icons) for Windows
if(WIN32)
    set(GRIDCOIN_RC res/gridcoinresearch.rc)

    # Ensure windres can find the referenced icon files
    set_source_files_properties(${GRIDCOIN_RC} PROPERTIES
        COMPILE_DEFINITIONS WINDRES_PREPROC
        COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/res"
    )
endif()

# Define the executable
# Note: We add ${GRIDCOIN_RC} to the sources so the icon is compiled into the binary
add_executable(gridcoinresearch bitcoin.cpp ${GRIDCOIN_RC})

# macOS Bundle Resources
set(MACOSX_BUNDLE_RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/gridcoin.icns")
if(APPLE)
    target_sources(gridcoinresearch PRIVATE ${MACOSX_BUNDLE_RESOURCE_FILES})
    set_target_properties(gridcoinresearch PROPERTIES
        RESOURCE ${MACOSX_BUNDLE_RESOURCE_FILES}
    )
endif()

target_link_libraries(gridcoinresearch PRIVATE
    ${RUNTIME_LIBS}
    ${QT}::Widgets
    CURL::libcurl
    gridcoin_util
    gridcoinqt
)

# Linux Static Math Lib Workaround
if(UNIX AND NOT APPLE AND DEFINED DEP_LIB)
    # Ensure DEP_LIB is absolute so the link command works from any directory.
    get_filename_component(DEP_LIB "${DEP_LIB}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    # 1. The "Imposter" Math Target
    # We must link against the system's SHARED libm to avoid static linking issues.

    # Detect the location:
    if(EXISTS "/lib64/libm.so.6")
        # RHEL / CentOS / Fedora / OpenSUSE
        set(SYSTEM_LIBM "/lib64/libm.so.6")
    elseif(EXISTS "/lib/x86_64-linux-gnu/libm.so.6")
        # Debian / Ubuntu
        set(SYSTEM_LIBM "/lib/x86_64-linux-gnu/libm.so.6")
    elseif(EXISTS "/usr/lib/libm.so")
        # Alpine Linux (Musl)
        set(SYSTEM_LIBM "/usr/lib/libm.so")
    else()
        # Fallback: Ask CMake to find it.
        # We use CMAKE_FIND_ROOT_PATH_BOTH to allow breaking out of the cross-compile
        # sandbox to find the host system's libm.
        find_library(SYSTEM_LIBM NAMES m.so.6 m libm.so CMAKE_FIND_ROOT_PATH BOTH)
    endif()

    if(SYSTEM_LIBM)
        message(STATUS "Using System Math Lib: ${SYSTEM_LIBM}")
        add_library(m SHARED IMPORTED GLOBAL)
        set_target_properties(m PROPERTIES IMPORTED_LOCATION "${SYSTEM_LIBM}")
        target_link_libraries(gridcoinresearch PRIVATE m)
    else()
        message(FATAL_ERROR "Could not find system libm.so.6 (or libm.so). Required for static builds.")
    endif()

    # 2. THE AUTOMATED MANUAL FIX
    set(MY_LIBS_LIST
        "${DEP_LIB}/libfreetype.a"
        "${DEP_LIB}/libfontconfig.a"
        "${DEP_LIB}/libexpat.a"
        "${DEP_LIB}/libxcb-util.a"
    )
    string(REPLACE ";" " " MANUAL_STATIC_FIX "${MY_LIBS_LIST}")
    set(CMAKE_CXX_LINK_EXECUTABLE
        "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES> ${MANUAL_STATIC_FIX}"
    )
endif()

set_target_properties(gridcoinresearch PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/share/qt/Info.plist.cmake.in"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Windows Specifics
if(WIN32)
    target_link_libraries(gridcoinresearch PRIVATE
        ws2_32
        wsock32
        wldap32
        crypt32
        bcrypt
        shlwapi
        iphlpapi
    )
endif()

# Installation
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS gridcoinresearch COMPONENT gui
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/share/icons" COMPONENT gui
        DESTINATION "${CMAKE_INSTALL_DATADIR}"
    )
    install(FILES "${CMAKE_SOURCE_DIR}/contrib/gridcoinresearch.desktop" COMPONENT gui
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
    install(FILES "${CMAKE_SOURCE_DIR}/doc/gridcoinresearch.1" COMPONENT gui
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    )
    install(FILES "${CMAKE_SOURCE_DIR}/contrib/completions/bash/gridcoinresearch.bash" COMPONENT gui
        DESTINATION "${CMAKE_INSTALL_DATADIR}/bash-completion/completions"
        RENAME "gridcoinresearch"
    )
    execute_process(
        COMMAND git log -1 --format=%cs
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_COMMIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_COMMIT_DATE_RESULT
        ERROR_QUIET
    )
    if(NOT GIT_COMMIT_DATE_RESULT EQUAL 0 OR GIT_COMMIT_DATE STREQUAL "")
        string(TIMESTAMP GIT_COMMIT_DATE "%Y-%m-%d")
    endif()
    configure_file(
        "${CMAKE_SOURCE_DIR}/contrib/flatpak/us.gridcoin.GridcoinResearch.metainfo.xml.in"
        "${CMAKE_CURRENT_BINARY_DIR}/us.gridcoin.GridcoinResearch.metainfo.xml"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/us.gridcoin.GridcoinResearch.metainfo.xml" COMPONENT gui
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
    )
else()
    install(TARGETS gridcoinresearch COMPONENT gui
        RUNTIME DESTINATION bin
        BUNDLE  DESTINATION "."
    )
endif()

# macOS Deploy Target
# ===================
#
# Creates a self-contained, ad-hoc signed DMG using macdeployqtplus.
# Usage: cmake --build build --target deploy
#
if(APPLE)
    # Use Python3_EXECUTABLE if explicitly set (e.g. -DPython3_EXECUTABLE=...),
    # otherwise resolve via find_program at configure time, with a final
    # fallback to the literal "python3" string resolved via PATH at build time.
    if(NOT Python3_EXECUTABLE)
        find_program(Python3_EXECUTABLE NAMES python3)
    endif()
    if(NOT Python3_EXECUTABLE)
        set(Python3_EXECUTABLE "python3")
    endif()

    # Locate Qt translations directory and QTDIR for plugin discovery.
    # Try qtpaths first (the canonical Qt tool), then fall back to
    # deriving paths from the Qt6::Core imported target location.
    find_program(QTPATHS_EXECUTABLE NAMES qtpaths6 qtpaths)

    if(QTPATHS_EXECUTABLE)
        execute_process(
            COMMAND "${QTPATHS_EXECUTABLE}" --query QT_INSTALL_TRANSLATIONS
            OUTPUT_VARIABLE _qt_translations_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND "${QTPATHS_EXECUTABLE}" --query QT_INSTALL_PREFIX
            OUTPUT_VARIABLE _qt_prefix_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND "${QTPATHS_EXECUTABLE}" --query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE _qt_plugins_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    # Fallback: derive from Qt6::Core target location
    if(NOT _qt_translations_dir OR NOT _qt_prefix_dir)
        get_target_property(_qt_core_loc ${QT}::Core LOCATION)
        # e.g. /opt/homebrew/lib/QtCore.framework/Versions/A/QtCore
        #   -> /opt/homebrew
        get_filename_component(_qt_lib_dir "${_qt_core_loc}" DIRECTORY)
        # Walk up from lib/ (or framework internals) to the prefix
        string(REGEX REPLACE "/lib(/.*)?$" "" _qt_prefix_dir_fallback "${_qt_lib_dir}")
        if(NOT _qt_prefix_dir)
            set(_qt_prefix_dir "${_qt_prefix_dir_fallback}")
        endif()
        if(NOT _qt_translations_dir)
            set(_qt_translations_dir "${_qt_prefix_dir}/translations")
        endif()
    endif()

    set(_deploy_script "${CMAKE_SOURCE_DIR}/contrib/macdeploy/macdeployqtplus")
    set(_app_bundle "$<TARGET_BUNDLE_DIR:gridcoinresearch>")
    set(_dmg_name "gridcoin-${PROJECT_VERSION}-macos-${CMAKE_SYSTEM_PROCESSOR}")

    # Build the macdeployqtplus command.
    # appname ("Gridcoin") is the volume name; -dmg <basename> sets the output filename.
    set(_deploy_cmd
        ${Python3_EXECUTABLE} "${_deploy_script}"
        "${_app_bundle}" Gridcoin
        -dmg "${_dmg_name}"
    )

    # Only pass -plugin-dir if the directory actually exists
    if(_qt_plugins_dir AND EXISTS "${_qt_plugins_dir}")
        list(APPEND _deploy_cmd -plugin-dir "${_qt_plugins_dir}")
    endif()

    # Only pass -translations-dir if the directory actually exists
    if(_qt_translations_dir AND EXISTS "${_qt_translations_dir}")
        list(APPEND _deploy_cmd -translations-dir "${_qt_translations_dir}")
    endif()

    # Code signing: macdeployqtplus reads APPLE_SIGNING_IDENTITY directly
    # from the environment (inherited from the CI step or shell).  We do
    # NOT pass it through cmake -E env because the identity string
    # contains parentheses that /bin/sh cannot handle.  When the env var
    # is not set, macdeployqtplus falls back to ad-hoc signing on macOS.
    if(DEFINED ENV{APPLE_SIGNING_IDENTITY})
        set(_entitlements "${CMAKE_SOURCE_DIR}/contrib/macdeploy/entitlements.plist")
        if(EXISTS "${_entitlements}")
            list(APPEND _deploy_cmd -entitlements "${_entitlements}")
        endif()
    endif()

    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E env "QTDIR=${_qt_prefix_dir}" ${_deploy_cmd}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Deploying ${_dmg_name}.dmg via macdeployqtplus"
        DEPENDS gridcoinresearch
    )
endif()

# Tests
# =====

if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
