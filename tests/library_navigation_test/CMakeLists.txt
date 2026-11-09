qt_add_executable(library_navigation_test
    main.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_library_list_widget.cpp
    ${PROJECT_SOURCE_DIR}/custom_widgets/yacreader_library_item_widget.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes/theme_factory.cpp
    ${PROJECT_SOURCE_DIR}/common/themes/theme_manager.cpp
)
yacreader_apply_build_options(library_navigation_test)
target_include_directories(library_navigation_test PRIVATE
    ${PROJECT_SOURCE_DIR}/custom_widgets
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/themes
)
target_compile_definitions(library_navigation_test PRIVATE YACREADER_LIBRARY)
target_link_libraries(library_navigation_test PRIVATE Qt6::Widgets Qt6::Test common_gui naturalsort)
add_test(NAME library_navigation_test COMMAND library_navigation_test)
set_tests_properties(library_navigation_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
