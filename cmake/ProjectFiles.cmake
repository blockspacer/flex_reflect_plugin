include_guard( DIRECTORY )

list(APPEND flex_reflect_plugin_SOURCES
  ${flex_reflect_plugin_src_DIR}/plugin_main.cc
  ${flex_reflect_plugin_src_DIR}/EventHandler.hpp
  ${flex_reflect_plugin_src_DIR}/EventHandler.cc
  ${flex_reflect_plugin_src_DIR}/Tooling.hpp
  ${flex_reflect_plugin_src_DIR}/Tooling.cc
)
