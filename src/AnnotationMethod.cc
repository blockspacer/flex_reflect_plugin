#include <flexlib/ToolPlugin.hpp>

#include "flexlib/utils.hpp"

#include "flexlib/funcParser.hpp"
#include "flexlib/inputThread.hpp"

#include "flexlib/clangUtils.hpp"

#include "flexlib/clangPipeline.hpp"

#include "flexlib/annotation_parser.hpp"
#include "flexlib/annotation_match_handler.hpp"

#include "flexlib/matchers/annotation_matcher.hpp"

#include "flexlib/options/ctp/options.hpp"

#if defined(CLING_IS_ON)
#include "flexlib/ClingInterpreterModule.hpp"
#endif // CLING_IS_ON

#include <base/logging.h>
#include <base/cpu.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/debug/alias.h>
#include <base/debug/stack_trace.h>
#include <base/memory/ptr_util.h>
#include <base/sequenced_task_runner.h>
#include <base/strings/string_util.h>
#include <base/trace_event/trace_event.h>

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/ASTContext.h>
#include <clang/Lex/Preprocessor.h>

#include <type_traits>

static const std::string kPluginDebugLogName = "(AnnotationMethod plugin)";

static const std::string kVersion = "v0.0.1";

static const std::string kVersionCommand = "/version";

#if !defined(APPLICATION_BUILD_TYPE)
#define APPLICATION_BUILD_TYPE "local build"
#endif

namespace plugin {

class AnnotationMethod
  final
  : public ::plugin::ToolPlugin {
 public:
  explicit AnnotationMethod(
    ::plugin::AbstractManager& manager
    , const std::string& plugin)
    : ::plugin::ToolPlugin{manager, plugin}
  {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  std::string title() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("title");
  }

  std::string author() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("author");
  }

  std::string description() const override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return metadata()->data().value("description");
  }

  bool load() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::load()");

    DLOG(INFO)
      << "loaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

  void disconnect_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::disconnect_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .disconnect<
          &AnnotationMethod::handle_event_StringCommand>(this);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .disconnect<
          &AnnotationMethod::handle_event_RegisterAnnotationMethods>(this);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .disconnect<
          &AnnotationMethod::handle_event_RegisterClingInterpreter>(this);
#endif // CLING_IS_ON
  }

  void connect_to_dispatcher(
    entt::dispatcher &event_dispatcher) override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::connect_to_dispatcher()");

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::StringCommand>()
        .connect<&AnnotationMethod::handle_event_StringCommand>(this);

    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterAnnotationMethods>()
        .connect<
          &AnnotationMethod::handle_event_RegisterAnnotationMethods>(this);

#if defined(CLING_IS_ON)
    event_dispatcher.sink<
      ::plugin::ToolPlugin::Events::RegisterClingInterpreter>()
        .connect<
          &AnnotationMethod::handle_event_RegisterClingInterpreter>(this);
#endif // CLING_IS_ON
  }

  void handle_event_StringCommand(
    const ::plugin::ToolPlugin::Events::StringCommand& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::handle_event(StringCommand)");

    if(event.split_parts.size() == 1)
    {
      if(event.split_parts[0] == kVersionCommand) {
        LOG(INFO)
          << kPluginDebugLogName
          << " application version: "
          << kVersion;
        LOG(INFO)
          << kPluginDebugLogName
          << " application build type: "
          << APPLICATION_BUILD_TYPE;
      }
    }
  }

#if defined(CLING_IS_ON)
  void handle_event_RegisterClingInterpreter(
    const ::plugin::ToolPlugin::Events::RegisterClingInterpreter& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::handle_event(RegisterClingInterpreter)");

    DCHECK(event.clingInterpreter);
    clingInterpreter_ = event.clingInterpreter;
  }
#endif // CLING_IS_ON

  void process_executeStringWithoutSpaces(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_executeStringWithoutSpaces");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO)
      << "started processing of annotation: "
      << processedAnnotaion;

#if defined(CLING_IS_ON)
    // execute code stored in annotation
    {
      cling::Interpreter::CompilationResult compilationResult
        = clingInterpreter_->executeCodeNoResult(processedAnnotaion);
      if(compilationResult
         != cling::Interpreter::Interpreter::kSuccess)
      {
        LOG(ERROR)
          << "ERROR while running cling code:"
          << processedAnnotaion.substr(0, 1000);
      }
    }

    // remove annotation from source file
    {
      clang::SourceLocation startLoc = nodeDecl->getLocStart();
      clang::SourceLocation endLoc = nodeDecl->getLocEnd();

      clang_utils::expandLocations(startLoc, endLoc, rewriter);

      rewriter.ReplaceText(
                  clang::SourceRange(startLoc, endLoc),
                  "");
    }
#else
  LOG(WARNING)
    << "Unable to execute C++ code at runtime: "
    << "Cling is disabled.";
#endif // CLING_IS_ON
  }

  //clang::Poss getPosition(const clang::SourceManager& sm
  //  , const clang::SourceLocation& loc)
  //{
  //  unsigned p{};
  //  p.line = sm.getSpellingLineNumber(loc) - 1;
  //  p.column = sm.getSpellingColumnNumber(loc) - 1;
  //  return p;
  //}

  void process_executeCode(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_executeCode");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO) << "started processing of annotation: "
                 << processedAnnotaion;

#if defined(CLING_IS_ON)
    // execute code stored in annotation
    {
      cling::Interpreter::CompilationResult compilationResult
        = clingInterpreter_->executeCodeNoResult(
            processedAnnotaion);
      if(compilationResult
         != cling::Interpreter::Interpreter::kSuccess)
      {
        LOG(ERROR)
          << "ERROR while running cling code:"
          << processedAnnotaion.substr(0, 1000);
      }
    }

    // remove annotation from source file
    {
      clang::SourceLocation startLoc = nodeDecl->getLocStart();
      clang::SourceLocation endLoc = nodeDecl->getLocEnd();

      clang_utils::expandLocations(startLoc, endLoc, rewriter);

      clang::ASTContext *context = matchResult.Context;
      DCHECK(context);

      rewriter.ReplaceText(
                  clang::SourceRange(startLoc, endLoc),
                  "");

      //r.start = getPosition(
      //    context->getSourceManager()
      //    , clang::Lexer::GetBeginningOfToken(
      //        startLoc, context->getSourceManager(), context->getLangOpts()));
      //r.end = getPosition(
      //    context->getSourceManager()
      //    , clang::Lexer::getLocForEndOfToken(
      //        endLoc, 0, context->getSourceManager(), context->getLangOpts()));
#if 0
      const std::string export_start_token = "$executeCode";

      clang::ASTContext *Context = matchResult.Context;
      // find '('
      auto l_paren_loc = clang::Lexer::findLocationAfterToken(
                  startLoc.getLocWithOffset(export_start_token.length() - 1),
                  clang::tok::l_paren,
                  Context->getSourceManager(),
                  Context->getLangOpts(),
                  /*skipWhiteSpace=*/true);

      rewriter.ReplaceText(
                  clang::SourceRange(
                      startLoc,
                      l_paren_loc
                      ),
                  "");
      const std::string export_end_token = ")";
      rewriter.ReplaceText(
                  clang::SourceRange(
                      endLoc,
                      endLoc.getLocWithOffset(export_end_token.length())
                      ),
                  "");
#endif // 0
    }

#else
  LOG(WARNING)
    << "Unable to execute C++ code at runtime: "
    << "Cling is disabled.";
#endif // CLING_IS_ON
  }

  void process_executeCodeAndReplace(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_executeCodeAndReplace");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO)
      << "started processing of annotation: "
      << processedAnnotaion;

#if defined(CLING_IS_ON)
    std::ostringstream sstr;
    // populate variables that can be used by interpreted code:
    //   clangMatchResult, clangRewriter, clangDecl
    ///
    ///
    /// \todo convert multiple variables to single struct or tuple
    ///
    ///
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
      sstr << processedAnnotaion << ";";

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
          << processedAnnotaion.substr(0, 1000);
      }
    }

    // remove annotation from source file
    // replacing it with |cling::Value result|
    {
      clang::SourceLocation startLoc = nodeDecl->getLocStart();
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
                          clang::SourceRange(startLoc, endLoc),
                          resOption->getValue());
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
                      "for processedAnnotaion: "
                      << sstr.str();
      }
    }
#else
  LOG(WARNING)
    << "Unable to execute C++ code at runtime: "
    << "Cling is disabled.";
#endif // CLING_IS_ON
  }

#if 0
  void process_codegen_annotation(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_codegen_annotation");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    std::vector<::cxxctp::parsed_func> funcs_to_call;

    funcs_to_call.push_back(
      ::cxxctp::parsed_func{
        "call_codegen",
        ::cxxctp::parsed_func_detail{
          "call_codegen",
          ::cxxctp::args{}
        }
      });

    std::vector<::cxxctp::parsed_func> parsedFuncs;
    parsedFuncs = ::cxxctp::split_to_funcs(processedAnnotaion);
    for (const ::cxxctp::parsed_func & seg : parsedFuncs) {
        VLOG(9) << "segment: " << seg.func_with_args_as_string_;
        VLOG(9) << "funcs_to_call1  func_name_: " << seg.parsed_func_.func_name_;

        if(!seg.parsed_func_.func_name_.empty()) {
            funcs_to_call.push_back(seg);
            //funcs_to_call.push_back(seg.parsed_func_.func_name_);
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

    DLOG(INFO) << "generator for processedAnnotaion: "
                 << processedAnnotaion;

    for (const ::cxxctp::parsed_func& func_to_call : funcs_to_call) {
        VLOG(9) << "main_module task " << func_to_call.func_with_args_as_string_ << "... " << '\n';

        auto callback = sourceTransformRules_->find(
          func_to_call.parsed_func_.func_name_);
        if(callback == sourceTransformRules_->end())
        {
          LOG(WARNING)
            << "Unable to find callback for source transform rule: "
            << func_to_call.func_with_args_as_string_;
          continue;
        }

        DCHECK(callback->second);
        callback->second.Run(cxxctp_callback_args{
          func_to_call,
          matchResult,
          rewriter,
          nodeDecl,
          parsedFuncs
        });
    }
  }
#endif // 0

  void process_funccall(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_funccall");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    std::vector<::cxxctp::parsed_func> funcs_to_call;
    std::vector<::cxxctp::parsed_func> parsedFuncs;
    parsedFuncs = ::cxxctp::split_to_funcs(processedAnnotaion);
    for (const ::cxxctp::parsed_func & seg : parsedFuncs) {
        VLOG(9) << "segment: " << seg.func_with_args_as_string_;
        VLOG(9) << "funcs_to_call1  func_name_: " << seg.parsed_func_.func_name_;

        if(!seg.parsed_func_.func_name_.empty()) {
            funcs_to_call.push_back(seg);
            //funcs_to_call.push_back(seg.parsed_func_.func_name_);
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
      << processedAnnotaion;

    for (const ::cxxctp::parsed_func& func_to_call : funcs_to_call) {
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
          continue;
        }

        DCHECK(callback->second);
        cxxctp_callback_result result
          = callback->second.Run(cxxctp_callback_args{
              func_to_call,
              matchResult,
              rewriter,
              nodeDecl,
              parsedFuncs
            });

      // remove annotation from source file
      // replacing it with callback result
      {
        clang::SourceLocation startLoc = nodeDecl->getLocStart();
        clang::SourceLocation endLoc = nodeDecl->getLocEnd();

        clang_utils::expandLocations(startLoc, endLoc, rewriter);
        if(result.replacer != nullptr) {
          //NOTIMPLEMENTED();
          //NOTREACHED();
          rewriter.ReplaceText(
                      clang::SourceRange(startLoc, endLoc),
                      result.replacer);
        }
      }
    } // for
  }

  void handle_event_RegisterAnnotationMethods(
    const ::plugin::ToolPlugin::Events::RegisterAnnotationMethods& event)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::handle_event(RegisterAnnotationMethods)");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DCHECK(event.annotationMethods);
    ::cxxctp::AnnotationMethods& annotationMethods
      = *event.annotationMethods;

    DCHECK(event.sourceTransformPipeline);
    ::clang_utils::SourceTransformPipeline& sourceTransformPipeline
      = *event.sourceTransformPipeline;

    sourceTransformRules_
      = &sourceTransformPipeline.sourceTransformRules;

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
    annotationMethods["{executeStringWithoutSpaces};"] =
      base::BindRepeating(
        &AnnotationMethod::process_executeStringWithoutSpaces
        , base::Unretained(this));

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
    annotationMethods["{executeCode};"] =
      base::BindRepeating(
        &AnnotationMethod::process_executeCode
        , base::Unretained(this));

    // embeds arbitrary C++ code
    /**
      EXAMPLE:
        // will be replaced with 1234
        __attribute__((annotate("{gen};{executeCodeAndReplace};\
        new llvm::Optional<std::string>{\"1234\"};")))
        int SOME_UNIQUE_NAME2
        ;
    **/
    annotationMethods["{executeCodeAndReplace};"] =
      base::BindRepeating(
        &AnnotationMethod::process_executeCodeAndReplace
        , base::Unretained(this));

#if 0
    annotationMethods["{codegen};"] =
      base::BindRepeating(
        &AnnotationMethod::process_codegen_annotation
        , base::Unretained(this));
#endif // 0

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
    annotationMethods["{funccall};"] =
      base::BindRepeating(
        &AnnotationMethod::process_funccall
        , base::Unretained(this));
  }

  bool unload() override
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::unload()");

    DLOG(INFO)
      << "unloaded plugin with title = "
      << title()
      << " and description = "
      << description().substr(0, 100)
      << "...";

    return true;
  }

private:
  ::clang_utils::SourceTransformRules* sourceTransformRules_;

#if defined(CLING_IS_ON)
  ::cling_utils::ClingInterpreter* clingInterpreter_;
#endif // CLING_IS_ON

  DISALLOW_COPY_AND_ASSIGN(AnnotationMethod);
};

} // namespace plugin

REGISTER_PLUGIN(/*name*/ AnnotationMethod
    , /*className*/ plugin::AnnotationMethod
    // plugin interface version checks to avoid unexpected behavior
    , /*interface*/ "backend.ToolPlugin/1.0")
