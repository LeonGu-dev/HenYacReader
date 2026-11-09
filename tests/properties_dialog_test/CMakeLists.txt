qt_add_executable(properties_dialog_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/properties_dialog.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/properties_dialog.h
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_metadata_editor.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_metadata_editor.h
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_filename.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/staged_comic_rename.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_cover_label.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_cover_label.h
)
yacreader_apply_build_options(properties_dialog_test)
target_compile_options(properties_dialog_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(properties_dialog_test PRIVATE
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/db
    ${PROJECT_SOURCE_DIR}/custom_widgets
)
target_link_libraries(properties_dialog_test PRIVATE Qt6::Widgets Qt6::Test library_common db_helper common_gui)
add_test(NAME properties_dialog_test COMMAND properties_dialog_test)
set_tests_properties(properties_dialog_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
