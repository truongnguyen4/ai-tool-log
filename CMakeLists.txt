cmake_minimum_required(VERSION 3.19)
project(ToolLogPro LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent Sql)

qt_standard_project_setup()

qt_add_executable(ToolLogPro
    WIN32 MACOSX_BUNDLE
    # Entry point
    src/main.cpp
    
    # UI
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui
    src/ui/uimanager.cpp
    src/ui/uimanager_events.cpp
    src/ui/uimanager_fileio.cpp
    src/ui/uimanager_files.cpp
    src/ui/uimanager_filter.cpp
    src/ui/uimanager_misc.cpp
    src/ui/uimanager_theme.cpp
    src/ui/uimanager_setup.cpp
    src/ui/uimanager_signals.cpp
    src/ui/uimanager_table.cpp
    src/ui/uimanager_table_copy.cpp
    src/ui/uimanager.h
    src/ui/widgets/toggleswitch.cpp
    src/ui/widgets/toggleswitch.h
    src/ui/dialogs/settingsdialog.cpp
    src/ui/dialogs/settingsdialog.h
    src/ui/tooltips.h
    src/ui/tablestyler.h
    src/delegates/blinkdelegate.cpp
    src/delegates/blinkdelegate.h
    src/ui/controllers/cradlecontroller.cpp
    src/ui/controllers/cradlecontroller.h
    src/ui/controllers/dumpsyscontroller.cpp
    src/ui/controllers/dumpsyscontroller.h
    src/ui/controllers/devicestabcontroller.cpp
    src/ui/controllers/devicestabcontroller_config.cpp
    src/ui/controllers/devicestabcontroller_runtime.cpp
    src/ui/controllers/devicestabcontroller.h
    src/ui/controllers/logsplitcontroller.cpp
    src/ui/controllers/logsplitcontroller.h
    src/ui/dialogs/shortcutsdialog.cpp
    src/ui/dialogs/shortcutsdialog.h
    src/ui/dialogs/presetdialogs.cpp
    src/ui/dialogs/presetdialogs.h
    src/ui/colorscheme.cpp
    src/ui/colorscheme.h
    src/ui/widgetstyling.h
    src/ui/controllers/configurationcontroller.cpp
    src/ui/controllers/configurationcontroller_sdk.cpp
    src/ui/controllers/configurationcontroller_settings.cpp
    src/ui/controllers/configurationcontroller_propset.cpp
    src/ui/controllers/configurationcontroller.h
    src/ui/controllers/logfiltercontroller.cpp
    src/ui/controllers/logfiltercontroller.h
    src/ui/themes/themesheets.cpp
    src/ui/themes/themesheets.h

    # UI Components Library (centralized widget factories + design tokens)
    src/ui/components/components.h
    src/ui/components/palette.h
    src/ui/components/palette_light.cpp
    src/ui/components/palette_dark.cpp
    src/ui/components/uibutton.cpp
    src/ui/components/uibutton.h
    src/ui/components/uiinput.cpp
    src/ui/components/uiinput.h
    src/ui/components/uicard.cpp
    src/ui/components/uicard.h
    src/ui/components/uilabel.cpp
    src/ui/components/uilabel.h

    # Delegates
    src/delegates/highlightdelegate.cpp
    src/delegates/highlightdelegate.h
    src/delegates/rowactiondelegate.cpp
    src/delegates/rowactiondelegate.h
    
    # Managers
    src/managers/adbmanager.cpp
    src/managers/adbmanager.h
    src/managers/adbcommand.h
    src/managers/adbexecutor.cpp
    src/managers/adbexecutor.h
    src/managers/devicesmanager.cpp
    src/managers/devicesmanager.h
    src/managers/filemanager.cpp
    src/managers/filemanager.h
    src/managers/historymanager.cpp
    src/managers/historymanager.h
    src/managers/jsonfileio.cpp
    src/managers/jsonfileio.h
    src/managers/presetstore.h
    src/managers/sqlitepresetstore.cpp
    src/managers/sqlitepresetstore.h
    src/managers/qsettingspresetstore.cpp
    src/managers/qsettingspresetstore.h
    
    # Models
    src/models/logtablemodelbase.cpp
    src/models/logtablemodelbase.h
    src/models/valuetablesync.h
    src/models/logmodel.cpp
    src/models/logmodel.h
    src/models/marklogmodel.cpp
    src/models/marklogmodel.h
    src/models/settingsmodel.cpp
    src/models/settingsmodel.h
    src/models/propertiesmodel.cpp
    src/models/propertiesmodel.h
    src/models/propertydefinitionmodel.cpp
    src/models/propertydefinitionmodel.h
    src/models/blinksweep.h
    
    # Converters
    src/converters/threadtimelogconverter.cpp
    src/converters/threadtimelogconverter.h
    src/converters/brieflogconverter.cpp
    src/converters/brieflogconverter.h
    src/converters/dmesglogconverter.cpp
    src/converters/dmesglogconverter.h
    src/converters/propertydefinitionconverter.cpp
    src/converters/propertydefinitionconverter.h
    src/converters/propertydefinitionjson.cpp
    src/converters/propertydefinitionjson.h
    src/converters/devicelistconverter.cpp
    src/converters/devicelistconverter.h
    src/converters/settingslistconverter.cpp
    src/converters/settingslistconverter.h
    src/converters/propertieslistconverter.cpp
    src/converters/propertieslistconverter.h
    src/converters/dumpsysservicelistconverter.cpp
    src/converters/dumpsysservicelistconverter.h
    src/converters/devicedetailsconverter.cpp
    src/converters/devicedetailsconverter.h
    
    # Filters
    src/filters/logfilter.cpp
    src/filters/logfilter.h
    src/filters/valuefilter.cpp
    src/filters/valuefilter.h
    src/filters/filterengine.cpp
    src/filters/filterengine.h
    
    # Interfaces
    src/interfaces/ilogconverter.h
    src/interfaces/ilogfilter.h

    # Data
    src/data/propertydefinition.h
    src/data/logentry.h
    src/data/settingentry.h
    src/data/propertyentry.h
    src/data/tableconfig.h
)

# Add include directories for better organization
target_include_directories(ToolLogPro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/components
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/themes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/delegates
    ${CMAKE_CURRENT_SOURCE_DIR}/src/converters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/filters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/managers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket
)

# AutoUIC needs to find mainwindow.ui from non-ui/ source files (e.g.
# src/ui/controllers/logfiltercontroller.cpp pulls in ui_mainwindow.h to
# read UI fields).
set_target_properties(ToolLogPro PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

qt_add_resources(ToolLogPro "app_icons"
    PREFIX "/"
    BASE "src/resources"
    FILES
        src/resources/icons/edit-delete.svg
        src/resources/icons/document-save.svg
        src/resources/icons/folder-open.svg
        src/resources/icons/refresh.svg
        src/resources/icons/settings.svg
        src/resources/icons/arrow-left.svg
        src/resources/icons/arrow-right.svg
        src/resources/icons/arrow-up.svg
        src/resources/icons/arrow-down.svg
        src/resources/icons/plus.svg
        src/resources/icons/download.svg
        src/resources/icons/upload.svg
        src/resources/icons/auto-scroll.svg
        src/resources/icons/activity.svg
        src/resources/icons/tab-logcat.svg
        src/resources/icons/tab-android.svg
        src/resources/icons/tab-sdk.svg
        src/resources/icons/tab-devices.svg
)

target_link_libraries(ToolLogPro
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Concurrent
        Qt::Sql
)

# Resident-set sampling for the status bar uses the platform's own API.
if(WIN32)
    target_link_libraries(ToolLogPro PRIVATE psapi)
endif()

include(GNUInstallDirs)

install(TARGETS ToolLogPro
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ToolLogPro
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
