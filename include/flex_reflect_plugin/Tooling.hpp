#pragma once

#include <flexlib/clangUtils.hpp>
#include <flexlib/ToolPlugin.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <base/logging.h>
#include <base/sequenced_task_runner.h>

namespace plugin {

class Tooling {
public:
  Tooling(
    const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event
#if defined(CLING_IS_ON)
    , ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
  );

  ~Tooling();

  // execute code in Cling C++ interpreter
  // old code (executed code) may be replaced with ""
  void executeStringWithoutSpaces(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl);

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
  void funccall(
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

  DISALLOW_COPY_AND_ASSIGN(Tooling);
};

} // namespace plugin
