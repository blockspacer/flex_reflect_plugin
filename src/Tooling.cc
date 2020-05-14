#include <flex_reflect_plugin/Tooling.hpp> // IWYU pragma: associated

#include <flexlib/ToolPlugin.hpp>
#include <flexlib/core/errors/errors.hpp>
#include <flexlib/utils.hpp>
#include <flexlib/funcParser.hpp>
#include <flexlib/inputThread.hpp>
#include <flexlib/clangUtils.hpp>
#include <flexlib/clangPipeline.hpp>
#include <flexlib/annotation_parser.hpp>
#include <flexlib/annotation_match_handler.hpp>
#include <flexlib/matchers/annotation_matcher.hpp>
#include <flexlib/options/ctp/options.hpp>
#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

namespace plugin {

namespace {

template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

} // namespace

Tooling::Tooling(
  const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event
#if defined(CLING_IS_ON)
  , ::cling_utils::ClingInterpreter* clingInterpreter
#endif // CLING_IS_ON
) : clingInterpreter_(clingInterpreter)
{
  DCHECK(clingInterpreter_);

  DCHECK(event.sourceTransformPipeline);
  ::clang_utils::SourceTransformPipeline& sourceTransformPipeline
    = *event.sourceTransformPipeline;

  sourceTransformRules_
    = &sourceTransformPipeline.sourceTransformRules;

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Tooling::~Tooling()
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Tooling::executeCode(
  const std::string& processedAnnotation
  , clang::AnnotateAttr* annotateAttr
  , const clang_utils::MatchResult& matchResult
  , clang::Rewriter& rewriter
  , const clang::Decl* nodeDecl)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexReflect::process_executeCode");

#if defined(CLING_IS_ON)
  DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

  DLOG(INFO) << "started processing of annotation: "
               << processedAnnotation;

#if defined(CLING_IS_ON)
  // execute code stored in annotation
  {
    cling::Interpreter::CompilationResult compilationResult
      = clingInterpreter_->executeCodeNoResult(
          processedAnnotation);
    if(compilationResult
       != cling::Interpreter::Interpreter::kSuccess)
    {
      LOG(ERROR)
        << "ERROR while running cling code:"
        << processedAnnotation.substr(0, 1000);
    }
  }

  // remove annotation from source file
  {
    clang::SourceLocation startLoc = nodeDecl->getLocStart();
    // Note Stmt::getLocEnd() returns the source location prior to the
    // token at the end of the line.  For instance, for:
    // var = 123;
    //      ^---- getLocEnd() points here.
    clang::SourceLocation endLoc = nodeDecl->getLocEnd();

    clang_utils::expandLocations(startLoc, endLoc, rewriter);

    clang::ASTContext *context = matchResult.Context;
    DCHECK(context);

    rewriter.ReplaceText(
      clang::SourceRange(startLoc, endLoc)
      , "");
  }

#else
  LOG(WARNING)
    << "Unable to execute C++ code at runtime: "
    << "Cling is disabled.";
#endif // CLING_IS_ON
}

void Tooling::executeCodeAndReplace(
  const std::string& processedAnnotation
  , clang::AnnotateAttr* annotateAttr
  , const clang_utils::MatchResult& matchResult
  , clang::Rewriter& rewriter
  , const clang::Decl* nodeDecl)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexReflect::process_executeCodeAndReplace");

#if defined(CLING_IS_ON)
  DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

  DLOG(INFO)
    << "started processing of annotation: "
    << processedAnnotation;

#if defined(CLING_IS_ON)
  std::ostringstream sstr;
  // populate variables that can be used by interpreted code:
  //   clangMatchResult, clangRewriter, clangDecl
  ///
  /// \todo convert multiple variables to single struct or tuple
  {
    // scope begin
    sstr << "[](){";

    sstr << "const clang::ast_matchers::MatchFinder::MatchResult&"
            " clangMatchResult = ";
    sstr << cling_utils::passCppPointerIntoInterpreter(
      reinterpret_cast<void*>(&const_cast<clang_utils::MatchResult&>(matchResult))
      , "*(const clang::ast_matchers::MatchFinder::MatchResult*)");
    sstr << ";";

    sstr << "clang::Rewriter&"
            " clangRewriter = ";
    sstr << cling_utils::passCppPointerIntoInterpreter(
      reinterpret_cast<void*>(&rewriter)
      , "*(clang::Rewriter*)");
    sstr << ";";

    sstr << "const clang::Decl*"
            " clangDecl = ";
    sstr << cling_utils::passCppPointerIntoInterpreter(
      reinterpret_cast<void*>(const_cast<clang::Decl*>(nodeDecl))
      , "(const clang::Decl*)");
    sstr << ";";

    // vars end
    sstr << "return ";
    sstr << processedAnnotation << ";";

    // scope end
    sstr << "}();";
  }

  // execute code stored in annotation
  cling::Value result;
  {
    cling::Interpreter::CompilationResult compilationResult
      = clingInterpreter_->processCodeWithResult(
          sstr.str(), result);
    if(compilationResult
       != cling::Interpreter::Interpreter::kSuccess)
    {
      LOG(ERROR)
        << "ERROR while running cling code:"
        << processedAnnotation.substr(0, 1000);
    }
  }

  // remove annotation from source file
  // replacing it with |cling::Value result|
  {
    clang::SourceLocation startLoc = nodeDecl->getLocStart();
    // Note Stmt::getLocEnd() returns the source location prior to the
    // token at the end of the line.  For instance, for:
    // var = 123;
    //      ^---- getLocEnd() points here.
    clang::SourceLocation endLoc = nodeDecl->getLocEnd();

    clang_utils::expandLocations(startLoc, endLoc, rewriter);

    if(result.hasValue() && result.isValid()
        && !result.isVoid()) {
      void* resOptionVoid = result.getAs<void*>();
      auto resOption =
        static_cast<llvm::Optional<std::string>*>(resOptionVoid);
      if(resOption) {
        if(resOption->hasValue()) {
            rewriter.ReplaceText(
              clang::SourceRange(startLoc, endLoc)
              , resOption->getValue());
        } else {
          VLOG(9)
            << "ExecuteCodeAndReplace: kept old code."
            << " Nothing provided to perform rewriter.ReplaceText";
        }
        delete resOption; /// \note frees resOptionVoid memory
      }
    } else {
      DLOG(INFO) << "ignored invalid "
                    "Cling result "
                    "for processedAnnotation: "
                    << sstr.str();
    }
  }
#else
  LOG(WARNING)
    << "Unable to execute C++ code at runtime: "
    << "Cling is disabled.";
#endif // CLING_IS_ON
}

void Tooling::callFuncBySignature(
  const std::string& processedAnnotation
  , clang::AnnotateAttr* annotateAttr
  , const clang_utils::MatchResult& matchResult
  , clang::Rewriter& rewriter
  , const clang::Decl* nodeDecl)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel",
               "plugin::FlexReflect::callFuncBySignature");

#if defined(CLING_IS_ON)
  DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

  std::vector<::flexlib::parsed_func> funcs_to_call;
  std::vector<::flexlib::parsed_func> parsedFuncs;

  parsedFuncs = ::flexlib::split_to_funcs(processedAnnotation);

  for (const ::flexlib::parsed_func & seg : parsedFuncs) {
      VLOG(9) << "segment: " << seg.func_with_args_as_string_;
      VLOG(9) << "funcs_to_call1  func_name_: " << seg.parsed_func_.func_name_;

      if(!seg.parsed_func_.func_name_.empty()) {
          funcs_to_call.push_back(seg);
      }

      for (auto const& arg : seg.parsed_func_.args_.as_vec_) {
          VLOG(9) << "    arg name: " << arg.name_;
          VLOG(9) << "    arg value: " << arg.value_;
      }
      for (auto const& [key, values] : seg.parsed_func_.args_.as_name_to_value_) {
          VLOG(9) << "    arg key: " << key;
          VLOG(9) << "    arg values (" << values.size() <<"): ";
          for (auto const& val : values) {
              VLOG(9) << "        " << val;
          }
      }
      VLOG(9) << "\n";
  }

  DLOG(INFO)
    << "generator for code: "
    << processedAnnotation;

  for (const ::flexlib::parsed_func& func_to_call : funcs_to_call) {
      VLOG(9) << "main_module task "
                 << func_to_call.func_with_args_as_string_
                 << "... " << '\n';

      DCHECK(sourceTransformRules_);
      auto callback = sourceTransformRules_->find(
        func_to_call.parsed_func_.func_name_);
      if(callback == sourceTransformRules_->end())
      {
        LOG(WARNING)
          << "Unable to find callback for source transform rule: "
          << func_to_call.func_with_args_as_string_;
        std::vector<std::string> registeredRules;
        registeredRules.reserve(sourceTransformRules_->size());
        for(const auto& rule: (*sourceTransformRules_)) {
          registeredRules.push_back(rule.first);
        }
        VLOG(1)
          << "Registered source transform rules: "
          << registeredRules;
        continue;
      }

      DCHECK(callback->second);
      clang_utils::SourceTransformResult result
        = callback->second.Run(clang_utils::SourceTransformOptions{
            func_to_call
            , matchResult
            , rewriter
            , nodeDecl
            , parsedFuncs
          });

    // remove annotation from source file
    // replacing it with callback result
    {
      clang::SourceLocation startLoc = nodeDecl->getLocStart();
      // Note Stmt::getLocEnd() returns the source location prior to the
      // token at the end of the line.  For instance, for:
      // var = 123;
      //      ^---- getLocEnd() points here.
      clang::SourceLocation endLoc = nodeDecl->getLocEnd();

      clang_utils::expandLocations(startLoc, endLoc, rewriter);

      /// \note if result.replacer is nullptr, than we will keep old code
      if(result.replacer != nullptr) {
        rewriter.ReplaceText(
          clang::SourceRange(startLoc, endLoc)
          , result.replacer);
      }
    }
  } // for
}

} // namespace plugin
