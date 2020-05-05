from conans import ConanFile, CMake, tools, AutoToolsBuildEnvironment, RunEnvironment
from conans.errors import ConanInvalidConfiguration, ConanException
from conans.tools import os_info
import os, re, stat, fnmatch, platform, glob
from functools import total_ordering

class TestPackageConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "cmake_paths", "virtualenv", "cmake_find_package_multi"

    topics = ('c++')

    options = {
        "shared": [True, False],
        "use_system_boost": [True, False],
        "enable_tests": [True, False],
        "enable_clang_from_conan": [True, False],
        "enable_sanitizers": [True, False]
    }

    default_options = (
      #"*:shared=False",
      "shared=True",
      "enable_clang_from_conan=False",
      "use_system_boost=False",
      "enable_tests=False",
      "enable_sanitizers=False",
      # boost
      "boost:without_atomic=True",
      "boost:without_chrono=True",
      "boost:without_container=True",
      "boost:without_context=True",
      "boost:without_coroutine=True",
      "boost:without_graph=True",
      "boost:without_graph_parallel=True",
      "boost:without_log=True",
      "boost:without_math=True",
      "boost:without_mpi=True",
      "boost:without_serialization=True",
      "boost:without_test=True",
      "boost:without_timer=True",
      "boost:without_type_erasure=True",
      "boost:without_wave=True",
      # llvm
      "llvm:shared=False",
      "compiler-rt:shared=False",
      "clang:shared=False",
      "llvm_headers:shared=False",
      "clang_headers:shared=False",
      "clang_executables:shared=False",
      "llvm_demangle:shared=False",
      "llvm_support:shared=False",
      "llvm_binary_format:shared=False",
      "llvm_core:shared=False",
      "llvm_mc:shared=False",
      "llvm_bit_reader:shared=False",
      "llvm_mc_parser:shared=False",
      "llvm_object:shared=False",
      "llvm_profile_data:shared=False",
      "llvm_analysis:shared=False",
      "llvm_transform_utils:shared=False",
      "llvm_instcombine:shared=False",
      "llvm_bit_writer:shared=False",
      "llvm_target:shared=False",
      "llvm_scalar_opts:shared=False",
      "llvm_option:shared=False",
      "llvm_debuginfo_codeview:shared=False",
      "llvm_codegen:shared=False",
      "llvm_x86_utils:shared=False",
      "llvm_x86_asm_printer:shared=False",
      "llvm_mc_disassembler:shared=False",
      "llvm_debuginfo_msf:shared=False",
      "llvm_global_isel:shared=False",
      "llvm_asm_printer:shared=False",
      "llvm_x86_info:shared=False",
      "llvm_x86_asm_parser:shared=False",
      "llvm_x86_desc:shared=False",
      "llvm_selection_dag:shared=False",
      "clang_lex:shared=False",
      "clang_basic:shared=False",
      "llvm_x86_codegen:shared=False",
      "clang_analysis:shared=False",
      "clang_ast:shared=False",
      # flexlib
      "flexlib:shared=False",
      "flexlib:enable_clang_from_conan=False",
      "flexlib:enable_tests=True",
      # flextool
      #"flextool:shared=False",
      "flextool:enable_clang_from_conan=False",
      "flextool:enable_tests=True",
      # FakeIt
      "FakeIt:integration=catch",
      # openssl
      "openssl:shared=True",
      # chromium_base
      "chromium_base:use_alloc_shim=True",
    )

    def build(self):

        bin_path = ""
        for p in self.deps_cpp_info.bin_paths:
            bin_path = "%s%s%s" % (p, os.pathsep, bin_path)

        lib_path = ""
        for p in self.deps_cpp_info.lib_paths:
            lib_path = "%s%s%s" % (p, os.pathsep, lib_path)

        env = {
             "PATH": "%s:%s" % (bin_path, os.environ['PATH']),
             "LD_LIBRARY_PATH": "%s:%s" % (lib_path, os.environ['LD_LIBRARY_PATH'])
        }

        self.output.info("=================linux environment for %s=================\n" % (self.name))
        self.output.info('PATH = %s' % (env['PATH']))
        self.output.info('LD_LIBRARY_PATH = %s' % (env['LD_LIBRARY_PATH']))
        self.output.info('')

        with tools.environment_append(env):
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def test(self):
        if not tools.cross_building(self.settings):
            self.output.info('self.source_folder = %s' % (self.source_folder))
            ext = ".so" if os_info.is_linux else ".dll"
            flex_reflect_plugin_ROOT = self.deps_cpp_info["flex_reflect_plugin"].rootpath
            plugin_file = flex_reflect_plugin_ROOT
            plugin_file = os.path.join(plugin_file, "lib")
            plugin_file = os.path.join(plugin_file, "flex_reflect_plugin" + ext)
            self.output.info('plugin_file = %s' % (plugin_file))
            # cling_includes must point to cling/Interpreter/RuntimeUniverse.h
            cling_conan_ROOT = self.deps_cpp_info["cling_conan"].rootpath
            cling_includes = cling_conan_ROOT
            cling_includes = os.path.join(cling_includes, "include")
            self.output.info('cling_includes = %s' % (cling_includes))
            # clang_includes must point to stddef.h from lib/clang/5.0.0/include
            clang_includes = cling_conan_ROOT
            clang_includes = os.path.join(clang_includes, "lib")
            clang_includes = os.path.join(clang_includes, "clang")
            clang_includes = os.path.join(clang_includes, "5.0.0")
            clang_includes = os.path.join(clang_includes, "include")
            self.output.info('clang_includes = %s' % (clang_includes))
            flextool_cmd = "flextool" \
              " --outdir ." \
              " --srcdir ." \
              " --load_plugin {}" \
              " -extra-arg=-I{}" \
              " -extra-arg=-I{}" \
              " {}/main.cpp".format(
              plugin_file, cling_includes, clang_includes, self.source_folder)
            self.output.info('flextool_cmd = %s' % (flextool_cmd))
            self.run(flextool_cmd, run_environment=True)