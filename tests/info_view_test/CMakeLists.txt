qt_add_executable(info_view_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_info_object.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_info_object.h
    ${PROJECT_SOURCE_DIR}/custom_widgets/metadata_chip_colors.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/metadata_chip_colors.h
)
yacreader_apply_build_options(info_view_test)
target_compile_definitions(info_view_test PRIVATE YACREADER_LIBRARY INFO_QML_DIR="${PROJECT_SOURCE_DIR}/YACReaderLibrary/qml")
target_include_directories(info_view_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db
    ${PROJECT_SOURCE_DIR}/custom_widgets
)
target_link_libraries(info_view_test PRIVATE Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Test db_helper library_common common_gui)
add_test(NAME info_view_test COMMAND info_view_test)
set_tests_properties(info_view_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{WINDIR}/Fonts" info_test_fonts)
    set_property(TEST info_view_test APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=${info_test_fonts}")
endif()
