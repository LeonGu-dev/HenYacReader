qt_add_executable(descriptive_metadata_test main.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/staged_comic_rename.cpp
    ${PROJECT_SOURCE_DIR}/YACReaderLibrary/comic_filename.cpp)
yacreader_apply_build_options(descriptive_metadata_test)
target_link_libraries(descriptive_metadata_test PRIVATE Qt6::Test Qt6::Sql db_helper library_common)
add_test(NAME descriptive_metadata_test COMMAND descriptive_metadata_test)

target_compile_options(descriptive_metadata_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
