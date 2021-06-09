flex_reflect_test_gtest(${ROOT_PROJECT_NAME}-gmock "gmock.test.cpp")

flex_reflect_test_gtest(${ROOT_PROJECT_NAME}-i18n "i18n.test.cpp")

# "i18n" is one of test program names
add_custom_command( TARGET ${ROOT_PROJECT_NAME}-i18n POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        ${CMAKE_CURRENT_SOURCE_DIR}/data
                        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME} )

list(APPEND flex_reflect_unittests
  #annotations/asio_guard_annotations_unittest.cc
)
list(APPEND flex_reflect_unittest_utils
  #"allocator/partition_allocator/arm_bti_test_functions.h"
)

list(REMOVE_DUPLICATES flex_reflect_unittests)
list(TRANSFORM flex_reflect_unittests PREPEND ${FLEX_REFLECT_SOURCES_PATH})

list(REMOVE_DUPLICATES flex_reflect_unittest_utils)
list(FILTER flex_reflect_unittest_utils EXCLUDE REGEX ".*_unittest.cc$")
list(TRANSFORM flex_reflect_unittest_utils PREPEND ${FLEX_REFLECT_SOURCES_PATH})

foreach(FILEPATH ${flex_reflect_unittests})
  set(test_sources
    "${FILEPATH}"
    ${flex_reflect_unittest_utils}
  )
  list(REMOVE_DUPLICATES flex_reflect_unittest_utils)
  get_filename_component(FILENAME_WITHOUT_EXT ${FILEPATH} NAME_WE)
  flex_reflect_test_gtest(${ROOT_PROJECT_NAME}-flex_reflect-${FILENAME_WITHOUT_EXT}
    "${test_sources}")
endforeach()
