cmake_minimum_required(VERSION 3.21)
project(PeakTech2275 VERSION 1.2.1 LANGUAGES CXX)

set(DEVICE_QT_MAJOR 6 CACHE STRING "Qt major version (5 or 6)")
set_property(CACHE DEVICE_QT_MAJOR PROPERTY STRINGS 5 6)
if(DEVICE_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Network SerialPort LinguistTools)
    set(CMAKE_AUTOMOC ON)
    # Use an installed Qt Charts module, or a separately built 5.15.19 module.
    set(QT5_CHARTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/qtcharts-x86" CACHE PATH "Qt 5 Charts")
    find_package(Qt5Charts 5.15 QUIET)
    if(NOT TARGET Qt5::Charts)
        find_path(qt5_charts_headers NAMES QtCharts/QChart HINTS "${QT5_CHARTS_ROOT}/include" REQUIRED)
        find_library(qt5_charts_library NAMES Qt5Charts HINTS "${QT5_CHARTS_ROOT}/lib" REQUIRED)
        find_file(qt5_charts_dll NAMES Qt5Charts.dll HINTS "${QT5_CHARTS_ROOT}" PATH_SUFFIXES bin lib REQUIRED)
        add_library(Qt5::Charts SHARED IMPORTED)
        set_target_properties(Qt5::Charts PROPERTIES
            IMPORTED_IMPLIB "${qt5_charts_library}"
            IMPORTED_LOCATION "${qt5_charts_dll}"
            INTERFACE_INCLUDE_DIRECTORIES "${qt5_charts_headers};${qt5_charts_headers}/QtCharts"
        )
    endif()
elseif(DEVICE_QT_MAJOR STREQUAL "6")
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network SerialPort Charts LinguistTools)
    qt_standard_project_setup()
else()
    message(FATAL_ERROR "DEVICE_QT_MAJOR must be 5 or 6.")
endif()
set(qt "Qt${DEVICE_QT_MAJOR}")
set(QT_I18N_SOURCE_LANGUAGE de)
set(CMAKE_AUTORCC ON)

if(DEVICE_QT_MAJOR STREQUAL "5")
    add_executable(PeakTech2275 WIN32)
else()
    qt_add_executable(PeakTech2275 WIN32)
endif()
target_sources(PeakTech2275 PRIVATE
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
target_link_libraries(PeakTech2275 PRIVATE ${qt}::Widgets ${qt}::Network ${qt}::SerialPort ${qt}::Charts)
if(MSVC)
    target_compile_options(PeakTech2275 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

set(ts_files translations/peaktech_en.ts translations/peaktech_fr.ts)
if(DEVICE_QT_MAJOR STREQUAL "5")
    qt5_add_translation(qm_files ${ts_files})
    set(translation_resources "<RCC><qresource prefix=\"/i18n\">\n")
    foreach(qm IN LISTS qm_files)
        get_filename_component(filename "${qm}" NAME)
        string(APPEND translation_resources "  <file alias=\"${filename}\">${qm}</file>\n")
    endforeach()
    string(APPEND translation_resources "</qresource><qresource prefix=\"/qt-i18n\">\n")
    foreach(locale IN ITEMS de fr)
        get_filename_component(qt_catalog "${Qt5_DIR}/../../../translations/qtbase_${locale}.qm" ABSOLUTE)
        if(EXISTS "${qt_catalog}")
            string(APPEND translation_resources "  <file alias=\"qtbase_${locale}.qm\">${qt_catalog}</file>\n")
        endif()
    endforeach()
    string(APPEND translation_resources "</qresource></RCC>\n")
    set(translations_qrc "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
    file(CONFIGURE OUTPUT "${translations_qrc}" CONTENT "${translation_resources}" @ONLY)
    qt5_add_resources(translation_sources "${translations_qrc}")
    target_sources(PeakTech2275 PRIVATE ${translation_sources})
else()
    qt_add_translations(PeakTech2275 TS_FILES ${ts_files})
    foreach(language IN ITEMS de fr)
        set(qt_catalog "${Qt6_DIR}/../../../translations/qtbase_${language}.qm")
        if(EXISTS "${qt_catalog}")
            get_filename_component(qt_catalog "${qt_catalog}" ABSOLUTE)
            set_source_files_properties("${qt_catalog}" PROPERTIES QT_RESOURCE_ALIAS "qtbase_${language}.qm")
            qt_add_resources(PeakTech2275 "qt_${language}" PREFIX "/qt-i18n" FILES "${qt_catalog}")
        endif()
    endforeach()
endif()

if(DEVICE_QT_MAJOR STREQUAL "5")
    add_custom_command(TARGET PeakTech2275 POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt5::Charts>" "$<TARGET_FILE_DIR:PeakTech2275>"
        VERBATIM
    )
endif()
