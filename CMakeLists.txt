cmake_minimum_required(VERSION 3.21)
project(PeakTech2275 VERSION 1.2.0 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network SerialPort Charts LinguistTools)
qt_standard_project_setup()
set(QT_I18N_SOURCE_LANGUAGE de)
set(CMAKE_AUTORCC ON)

qt_add_executable(PeakTech2275 WIN32
    main.cpp
    mainwindow.cpp mainwindow.h updates.cpp
    peaktech.cpp peaktech.h
    listtestpage.cpp listtestpage.h
    assets/app.qrc
)
if(WIN32)
    enable_language(RC)
    target_sources(PeakTech2275 PRIVATE assets/app.rc)
    target_include_directories(PeakTech2275 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()
target_compile_features(PeakTech2275 PRIVATE cxx_std_17)
target_compile_definitions(PeakTech2275 PRIVATE PEAKTECH_VERSION="${PROJECT_VERSION}")
target_link_libraries(PeakTech2275 PRIVATE Qt6::Widgets Qt6::Network Qt6::SerialPort Qt6::Charts)
if(MSVC)
    target_compile_options(PeakTech2275 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

qt_add_translations(PeakTech2275 TS_FILES
    translations/peaktech_en.ts
    translations/peaktech_fr.ts
)
foreach(language IN ITEMS de fr)
    set(qt_catalog "${Qt6_DIR}/../../../translations/qtbase_${language}.qm")
    if(EXISTS "${qt_catalog}")
        get_filename_component(qt_catalog "${qt_catalog}" ABSOLUTE)
        set_source_files_properties("${qt_catalog}" PROPERTIES QT_RESOURCE_ALIAS "qtbase_${language}.qm")
        qt_add_resources(PeakTech2275 "qt_${language}" PREFIX "/qt-i18n" FILES "${qt_catalog}")
    endif()
endforeach()
