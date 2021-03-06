get_filename_component(CURRENT_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
list(APPEND CMAKE_MODULE_PATH ${CURRENT_CMAKE_DIR})

#include(CMakeFindDependencyMacro) # use find_package instead

# NOTE: some packages may be optional (platform-specific, etc.)
# find_package(... REQUIRED)
find_package(chromium_base REQUIRED)
# see https://doc.magnum.graphics/corrade/corrade-cmake.html#corrade-cmake-subproject
find_package(Corrade REQUIRED PluginManager)
find_package(Cling)
find_package(flexlib)
find_package(flextool)

list(REMOVE_AT CMAKE_MODULE_PATH -1)

if(NOT TARGET CONAN_PKG::flex_reflect_plugin)
  message(FATAL_ERROR "Use flex_reflect_plugin from conan")
endif()
set(flex_reflect_plugin_LIB
  CONAN_PKG::flex_reflect_plugin
)
set(flex_reflect_plugin_FILE
  ${CONAN_FLEX_REFLECT_PLUGIN_ROOT}/lib/flex_reflect_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}
)
# conan package has '/include' dir
set(flex_reflect_plugin_HEADER_DIR
  ${CONAN_FLEX_REFLECT_PLUGIN_ROOT}/include
)
# used by https://docs.conan.io/en/latest/developing_packages/workspaces.html
if(TARGET flex_reflect_plugin)
  # name of created target
  set(flex_reflect_plugin_LIB
    flex_reflect_plugin
  )
  # no '/include' dir on local build
  set(flex_reflect_plugin_HEADER_DIR
    ${CONAN_FLEX_REFLECT_PLUGIN_ROOT}
  )
  # plugin file
  get_property(flex_reflect_plugin_LIBRARY_OUTPUT_DIRECTORY
    TARGET ${flex_reflect_plugin_LIB}
    PROPERTY LIBRARY_OUTPUT_DIRECTORY)
  message(STATUS "flex_reflect_plugin_LIBRARY_OUTPUT_DIRECTORY == ${flex_reflect_plugin_LIBRARY_OUTPUT_DIRECTORY}")
  set(flex_reflect_plugin_FILE
    ${flex_reflect_plugin_LIBRARY_OUTPUT_DIRECTORY}/flex_reflect_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}
  )
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/flex_reflect_plugin-config.cmake")
  # uses Config.cmake or a -config.cmake file
  # see https://gitlab.kitware.com/cmake/community/wikis/doc/tutorials/How-to-create-a-ProjectConfig.cmake-file
  # BELOW MUST BE EQUAL TO find_package(... CONFIG REQUIRED)
  # NOTE: find_package(CONFIG) not supported with EMSCRIPTEN, so use include()
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/flex_reflect_plugin-config.cmake)
endif()

message(STATUS "flex_reflect_plugin_HEADER_DIR=${flex_reflect_plugin_HEADER_DIR}")
