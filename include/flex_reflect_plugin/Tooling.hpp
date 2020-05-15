#pragma once

#include <flexlib/clangUtils.hpp>
#include <flexlib/ToolPlugin.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <base/logging.h>
#include <base/sequenced_task_runner.h>

namespace plugin {

/// \note class name must not collide with
/// class names from other loaded plugins
class ReflectTooling {
public:
  ReflectTooling(
    const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event
#if defined(CLING_IS_ON)
    , ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
  );

  ~ReflectTooling();

  // execute code in Cling C++ interpreter
  // old code (executed code) may be replaced with ""
  void executeCode(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl);

  // execute code in Cling C++ interpreter
  // old code (executed code) may be replaced using return value
  void executeCodeAndReplace(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl);

  // call some function (argitrary logic) by name.
  // can accept arguments
  void callFuncBySignature(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl);

private:
  ::clang_utils::SourceTransformRules* sourceTransformRules_;

#if defined(CLING_IS_ON)
  ::cling_utils::ClingInterpreter* clingInterpreter_;
#endif // CLING_IS_ON

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ReflectTooling);
};

} // namespace plugin
