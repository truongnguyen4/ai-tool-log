cmake_minimum_required(VERSION 3.19)
project(ToolLogPro LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(ToolLogPro
    WIN32 MACOSX_BUNDLE
    # Entry point
    src/main.cpp
    
    # UI
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui
    src/ui/valuedelegate.cpp
    src/ui/valuedelegate.h
    
    # Delegates
    src/delegates/highlightdelegate.cpp
    src/delegates/highlightdelegate.h
    
    # Managers
    src/managers/adbmanager.cpp
    src/managers/adbmanager.h
    src/managers/adbcommand.h
    src/managers/filemanager.cpp
    src/managers/filemanager.h
    
    # Models
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
    
    # Converters
    src/converters/threadtimelogconverter.cpp
    src/converters/threadtimelogconverter.h
    src/converters/brieflogconverter.cpp
    src/converters/brieflogconverter.h
    
    # Filters
    src/filters/logfilter.cpp
    src/filters/logfilter.h
    src/filters/configfilter.cpp
    src/filters/configfilter.h
    
    # Interfaces
    src/interfaces/ilogconverter.h
    src/interfaces/ilogfilter.h
    src/interfaces/iconfigfilter.h
    
    # Data
    src/data/propertydefinition.h
    src/data/logentry.h
    src/data/settingentry.h
    src/data/propertyentry.h
)

# Add include directories for better organization
target_include_directories(ToolLogPro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/delegates
    ${CMAKE_CURRENT_SOURCE_DIR}/src/converters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/filters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/managers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data
)

target_link_libraries(ToolLogPro
    PRIVATE
        Qt::Core
        Qt::Widgets
)

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
