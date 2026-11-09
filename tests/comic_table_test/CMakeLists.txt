qt_add_executable(comic_table_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/metadata_filter_panel.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_titled_toolbar.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_busy_widget.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/metadata_chip_colors.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db/comic_model.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_table_layout.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/metadata_chip_delegate.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_table_view.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes/theme_factory.cpp
    ${PROJECT_SOURCE_DIR}/common/themes/theme_manager.cpp
)
yacreader_apply_build_options(comic_table_test)
target_compile_definitions(comic_table_test PRIVATE YACREADER_LIBRARY)
target_compile_options(comic_table_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(comic_table_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes
    ${PROJECT_SOURCE_DIR}/custom_widgets
)
target_link_libraries(comic_table_test PRIVATE Qt6::Widgets Qt6::Test db_helper library_common common_gui)
add_test(NAME comic_table_test COMMAND comic_table_test)
set_tests_properties(comic_table_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
