cmake_minimum_required(VERSION 3.19)
project(ToolLogPro LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(ToolLogPro
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    adbmanager.cpp
    adbmanager.h
    ilogconverter.h
    threadtimelogconverter.cpp
    threadtimelogconverter.h
    brieflogconverter.cpp
    brieflogconverter.h
    filemanager.cpp
    filemanager.h
    logmodel.cpp
    logmodel.h
    ilogfilter.h
    logfilter.cpp
    logfilter.h
    marklogmodel.cpp
    marklogmodel.h
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
