qt_add_executable(sidebar_filter_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/yacreader_folders_view.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/yacreader_reading_lists_view.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db/comic_model.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db/reading_list_model.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db/reading_list_item.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes/theme_factory.cpp
    ${PROJECT_SOURCE_DIR}/common/themes/theme_manager.cpp
)
yacreader_apply_build_options(sidebar_filter_test)
target_compile_definitions(sidebar_filter_test PRIVATE YACREADER_LIBRARY)
target_compile_options(sidebar_filter_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(sidebar_filter_test PRIVATE
    ${PROJECT_SOURCE_DIR}/custom_widgets
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes
)
target_link_libraries(sidebar_filter_test PRIVATE Qt6::Widgets Qt6::Test custom_widgets_library db_helper library_common common_gui)
add_test(NAME sidebar_filter_test COMMAND sidebar_filter_test)
set_tests_properties(sidebar_filter_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
