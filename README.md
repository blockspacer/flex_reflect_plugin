# About

Plugin for [https://github.com/blockspacer/flextool](https://github.com/blockspacer/flextool)

Provides annotation methods. See [https://blockspacer.github.io/flex_docs/about/](https://blockspacer.github.io/flex_docs/about/) for details

## Before installation

- [installation guide](https://blockspacer.github.io/flex_docs/download/)

## Installation

```bash
export CXX=clang++-6.0
export CC=clang-6.0

# NOTE: change `build_type=Debug` to `build_type=Release` in production
# NOTE: use --build=missing if you got error `ERROR: Missing prebuilt package`
CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
    cmake -E time \
      conan create . conan/stable \
      -s build_type=Debug -s cling_conan:build_type=Release \
      --profile clang \
          -o flex_reflect_plugin:shared=True \
          -o flex_reflect_plugin:enable_clang_from_conan=False \
          -e flex_reflect_plugin:enable_tests=True
```

## Provided annotation methods

- Call function (arbitrary logic) by some name
- Execute C++ code at runtime using Cling C++ interpreter

```cpp
    // evaluates arbitrary C++ code line
    // does not support newline characters or spaces
    // may use `#include` or preprocessor macros
    // example:
    //   $executeStringWithoutSpaces("#include <cling/Interpreter/Interpreter.h>")
    // if you need to execute multiline C++ code line - use "executeCode"
    /**
      EXAMPLE:
        // will be replaced with empty string
        __attribute__((annotate("{gen};{executeStringWithoutSpaces};\
        printf(\"Hello world!\");"))) \
        int SOME_UNIQUE_NAME0
        ;
        // if nothing printed, then
        // replace printf with
        // LOG(INFO)<<\"Hello!\";"))) \
    **/
    "{executeStringWithoutSpaces};"

    // exports arbitrary C++ code, code can be multiline
    // unable to use `#include` or preprocessor macros
    /**
      EXAMPLE:
        // will be replaced with empty string
        __attribute__((annotate("{gen};{executeCode};\
        printf(\"Hello me!\");"))) \
        int SOME_UNIQUE_NAME1
        ;
        // if nothing printed, then
        // replace printf with
        // LOG(INFO)<<\"Hello!\";"))) \
    **/
    "{executeCode};"

    // embeds arbitrary C++ code
    /**
      EXAMPLE:
        // will be replaced with 1234
        __attribute__((annotate("{gen};{executeCodeAndReplace};\
        new llvm::Optional<std::string>{\"1234\"};")))
        int SOME_UNIQUE_NAME2
        ;
    **/
    "{executeCodeAndReplace};"

    /**
      EXAMPLE:
        #include <string>
        #include <vector>
        struct
          __attribute__((annotate("{gen};{funccall};make_reflect;")))
        SomeStructName {
         public:
          SomeStructName() {
            // ...
          }
         private:
          const int m_bar2 = 2;

          std::vector<std::string> m_VecStr2;
        };
        // handler for make_reflect must be registered by plugin!
    **/
    "{funccall};"
```

## For contibutors: conan editable mode

With the editable packages, you can tell Conan where to find the headers and the artifacts ready for consumption in your local working directory.
There is no need to run `conan create` or `conan export-pkg`.

See for details [https://docs.conan.io/en/latest/developing_packages/editable_packages.html](https://docs.conan.io/en/latest/developing_packages/editable_packages.html)

Build locally:

```bash
CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
  cmake -E time \
    conan install . \
    --install-folder local_build \
    -s build_type=Debug -s cling_conan:build_type=Release \
    --profile clang \
      -o flex_reflect_plugin:shared=True \
      -o flex_reflect_plugin:enable_clang_from_conan=False \
      -e flex_reflect_plugin:enable_tests=True

CONAN_REVISIONS_ENABLED=1 \
CONAN_VERBOSE_TRACEBACK=1 \
CONAN_PRINT_RUN_COMMANDS=1 \
CONAN_LOGGING_LEVEL=10 \
GIT_SSL_NO_VERIFY=true \
  cmake -E time \
    conan source . --source-folder local_build

conan build . \
  --build-folder local_build

conan package . \
  --build-folder local_build \
  --package-folder local_build/package_dir
```

Set package to editable mode:

```bash
conan editable add local_build/package_dir \
  flex_reflect_plugin/master@conan/stable
```

Note that `conanfile.py` modified to detect local builds via `self.in_local_cache`

After change source in folder local_build (run commands in source package folder):

```
conan build . \
  --build-folder local_build

conan package . \
  --build-folder local_build \
  --package-folder local_build/package_dir
```

Build your test project

In order to revert the editable mode just remove the link using:

```bash
conan editable remove \
  flex_reflect_plugin/master@conan/stable
```
