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

  void process_eval_annotation(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_eval_annotation");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO)
      << "eval for processedAnnotaion: "
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
#endif // CLING_IS_ON
  }

  void process_export_annotation(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_export_annotation");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO) << "export for processedAnnotaion: "
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

      const std::string export_start_token = "$export";

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
    }
#endif // CLING_IS_ON
  }

  void process_embed_annotation(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_embed_annotation");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    DLOG(INFO)
      << "embed for processedAnnotaion: "
      << processedAnnotaion;
#if defined(CLING_IS_ON)
    std::ostringstream sstr;
    // scope begin
    sstr << "[](){";
    // vars begin
    sstr << "const clang::ast_matchers::MatchFinder::MatchResult& clangMatchResult = ";
    sstr << "*(const clang::ast_matchers::MatchFinder::MatchResult*)("
            // Pass a pointer into cling as a string.
         << std::hex << std::showbase
         << reinterpret_cast<size_t>(&matchResult) << ");";
    sstr << "clang::Rewriter& clangRewriter = ";
    sstr << "*(clang::Rewriter*)("
            // Pass a pointer into cling as a string.
         << std::hex << std::showbase
         << reinterpret_cast<size_t>(&rewriter) << ");";
    sstr << "const clang::Decl* clangDecl = ";
    sstr << "(const clang::Decl*)("
            // Pass a pointer into cling as a string.
         << std::hex << std::showbase
         << reinterpret_cast<size_t>(nodeDecl) << ");";
    // vars end
    sstr << "return ";
    sstr << processedAnnotaion << ";";
    // scope end
    sstr << "}();";


    // execute code stored in annotation
    cling::Value result;
    {
      cling::Interpreter::CompilationResult compilationResult
        = clingInterpreter_->processCodeWithResult(
            processedAnnotaion, result);
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
          if(resOption->hasValue() && !resOption->getValue().empty()) {
              rewriter.ReplaceText(
                          clang::SourceRange(startLoc, endLoc),
                          resOption->getValue());
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
#endif // CLING_IS_ON
  }

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
        DVLOG(9) << "segment: " << seg.func_with_args_as_string_;
        DVLOG(9) << "funcs_to_call1  func_name_: " << seg.parsed_func_.func_name_;

        if(!seg.parsed_func_.func_name_.empty()) {
            funcs_to_call.push_back(seg);
            //funcs_to_call.push_back(seg.parsed_func_.func_name_);
        }

        for (auto const& arg : seg.parsed_func_.args_.as_vec_) {
            DVLOG(9) << "    arg name: " << arg.name_;
            DVLOG(9) << "    arg value: " << arg.value_;
        }
        for (auto const& [key, values] : seg.parsed_func_.args_.as_name_to_value_) {
            DVLOG(9) << "    arg key: " << key;
            DVLOG(9) << "    arg values (" << values.size() <<"): ";
            for (auto const& val : values) {
                DVLOG(9) << "        " << val;
            }
        }
        DVLOG(9) << "\n";
    }

    DLOG(INFO) << "generator for processedAnnotaion: "
                 << processedAnnotaion;

    for (const ::cxxctp::parsed_func& func_to_call : funcs_to_call) {
        DVLOG(9) << "main_module task " << func_to_call.func_with_args_as_string_ << "... " << '\n';

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

  void process_funccall_annotation(
    const std::string& processedAnnotaion
    , clang::AnnotateAttr* annotateAttr
    , const clang_utils::MatchResult& matchResult
    , clang::Rewriter& rewriter
    , const clang::Decl* nodeDecl)
  {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT0("toplevel",
                 "plugin::AnnotationMethod::process_funccall_annotation");

#if defined(CLING_IS_ON)
    DCHECK(clingInterpreter_);
#endif // CLING_IS_ON

    std::vector<::cxxctp::parsed_func> funcs_to_call;
    std::vector<::cxxctp::parsed_func> parsedFuncs;
    parsedFuncs = ::cxxctp::split_to_funcs(processedAnnotaion);
    for (const ::cxxctp::parsed_func & seg : parsedFuncs) {
        DVLOG(9) << "segment: " << seg.func_with_args_as_string_;
        DVLOG(9) << "funcs_to_call1  func_name_: " << seg.parsed_func_.func_name_;

        if(!seg.parsed_func_.func_name_.empty()) {
            funcs_to_call.push_back(seg);
            //funcs_to_call.push_back(seg.parsed_func_.func_name_);
        }

        for (auto const& arg : seg.parsed_func_.args_.as_vec_) {
            DVLOG(9) << "    arg name: " << arg.name_;
            DVLOG(9) << "    arg value: " << arg.value_;
        }
        for (auto const& [key, values] : seg.parsed_func_.args_.as_name_to_value_) {
            DVLOG(9) << "    arg key: " << key;
            DVLOG(9) << "    arg values (" << values.size() <<"): ";
            for (auto const& val : values) {
                DVLOG(9) << "        " << val;
            }
        }
        DVLOG(9) << "\n";
    }

    DLOG(INFO) << "generator for code: "
                 << processedAnnotaion;

    for (const ::cxxctp::parsed_func& func_to_call : funcs_to_call) {
        DVLOG(9) << "main_module task "
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
        callback->second.Run(cxxctp_callback_args{
          func_to_call,
          matchResult,
          rewriter,
          nodeDecl,
          parsedFuncs
        });
    }
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

    // evaluates arbitrary C++ code
    annotationMethods["{eval};"] =
      base::BindRepeating(
        &AnnotationMethod::process_eval_annotation
        , base::Unretained(this));

    // exports arbitrary C++ code
    annotationMethods["{export};"] =
      base::BindRepeating(
        &AnnotationMethod::process_export_annotation
        , base::Unretained(this));

    // embeds arbitrary C++ code
    annotationMethods["{embed};"] =
      base::BindRepeating(
        &AnnotationMethod::process_embed_annotation
        , base::Unretained(this));

    annotationMethods["{codegen};"] =
      base::BindRepeating(
        &AnnotationMethod::process_codegen_annotation
        , base::Unretained(this));

    annotationMethods["{funccall};"] =
      base::BindRepeating(
        &AnnotationMethod::process_funccall_annotation
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
