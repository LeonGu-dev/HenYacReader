qt_add_executable(comic_metadata_editor_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_filename.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/staged_comic_rename.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_metadata_editor.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_metadata_editor.h
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_cover_label.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_cover_label.h
)
yacreader_apply_build_options(comic_metadata_editor_test)
target_compile_options(comic_metadata_editor_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(comic_metadata_editor_test PRIVATE ${PROJECT_SOURCE_DIR}/YACReaderLibrary ${PROJECT_SOURCE_DIR}/custom_widgets)
target_link_libraries(comic_metadata_editor_test PRIVATE Qt6::Widgets Qt6::Test common_all common_gui)
add_test(NAME comic_metadata_editor_test COMMAND comic_metadata_editor_test)
set_tests_properties(comic_metadata_editor_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(metadata_demo_library demo_library.cpp)
yacreader_apply_build_options(metadata_demo_library)
target_compile_options(metadata_demo_library PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(metadata_demo_library PRIVATE Qt6::Widgets library_common)
