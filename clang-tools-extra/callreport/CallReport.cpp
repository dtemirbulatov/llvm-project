#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Format/Format.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Core/Replacement.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/RefactoringCallbacks.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"

using namespace clang;
using namespace llvm;
using namespace llvm::opt;
using namespace clang::ast_matchers;

// Option to specify a file name for a list of header files to check.
static cl::list<std::string>
    ListFileNames(cl::Positional, cl::value_desc("list"),
                  cl::desc("<list of one or more header list files>"),
                  cl::CommaSeparated);

static cl::opt<std::string>
    CallReportOutput("call-report-output",
                     llvm::cl::desc("The destanation file for call report"),
                     llvm::cl::init("calls_report.json"));

static cl::opt<std::string> SourcePath(
    "call-report-sourcepath",
    llvm::cl::desc("Part of full path to collect files for the call report"),
    llvm::cl::init(""));

namespace clang {

llvm::DenseMap<std::pair<llvm::StringRef, llvm::StringRef>,
               llvm::DenseSet<llvm::StringRef>>
    CallsDiag;

class CallExprTool : public ast_matchers::MatchFinder::MatchCallback {
public:
  explicit CallExprTool()
      : CurFunName(StringRef("")), CurFname(StringRef("")) {}

  ~CallExprTool() override = default;

  void registerMatchers(ast_matchers::MatchFinder *Finder) {
    Finder->addMatcher(functionDecl().bind("function"), this);
    Finder->addMatcher(callExpr(callee(namedDecl())).bind("call"), this);
  }

  void run(const ast_matchers::MatchFinder::MatchResult &Result) override {
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
      const Expr *Fn = Call->getCallee();
      if (Fn && Fn->getReferencedDeclOfCallee())
        if (const auto *ND =
                dyn_cast<NamedDecl>(Fn->getReferencedDeclOfCallee());
            ND->getDeclName().isIdentifier()) {
          StringRef Name = ND->getName();
          SourceLocation Loc = ND->getLocation();
#ifdef RDEBUG
          raw_ostream &OS = llvm::outs();
          OS << "Filename: " << CurFname << ", CurrentFunction: " << CurFunName
             << ", Call to: " << Name << ", SourcePath: " << SourcePath << "\n";
#endif
          const auto &SM = ND->getASTContext().getSourceManager();
          PresumedLoc PLoc =
              SM.getPresumedLoc(SM.getExpansionRange(Loc).getEnd());

          if (PLoc.isValid() &&
              std::strstr(PLoc.getFilename(), SourcePath.c_str()))
            CallsDiag[std::make_pair(CurFname, CurFunName)].insert(Name);
        }
    } else if (const auto *Function =
                   Result.Nodes.getNodeAs<FunctionDecl>("function");
               Function->hasBody() && Function->getDeclName().isIdentifier()) {
      CurFunName = Function->getName();
      SourceLocation Loc = Function->getLocation();
      const auto &SM = Function->getASTContext().getSourceManager();
      PresumedLoc PLoc = SM.getPresumedLoc(SM.getExpansionRange(Loc).getEnd());
      CurFname = (PLoc.isValid()) ? PLoc.getFilename() : StringRef("");
    }
  }

private:
  StringRef CurFunName;
  StringRef CurFname;
};

class CallExprAction : public clang::ASTFrontendAction {
public:
  CallExprAction() : Tool() { Tool.registerMatchers(&MatchFinder); }

  ~CallExprAction() override = default;

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler,
                    llvm::StringRef InFile) override;

private:
  ast_matchers::MatchFinder MatchFinder;
  CallExprTool Tool;
};

class CallExprActionFactory : public tooling::FrontendActionFactory {
public:
  CallExprActionFactory() {}

  std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<CallExprAction>();
  }
};

std::unique_ptr<ASTConsumer>
CallExprAction::CreateASTConsumer(CompilerInstance &Compiler, StringRef File) {
  return MatchFinder.newASTConsumer();
}

} // namespace clang

cl::OptionCategory ClangReorderFieldsCategory("clangreport files");
const char Usage[] =
    "Call reporter usage\n clangreport [list of file to report]\n";

int main(int argc, const char **argv) {
  auto ExpectedParser = tooling::CommonOptionsParser::create(
      argc, argv, ClangReorderFieldsCategory, cl::OneOrMore, Usage);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  CallExprActionFactory Factory;

  tooling::CommonOptionsParser &OP = ExpectedParser.get();

  auto Files = OP.getSourcePathList();
  tooling::RefactoringTool Tool(OP.getCompilations(), Files);

  int CodeStatus = Tool.run(&Factory);
  if (CodeStatus)
    return CodeStatus;

  if (CallsDiag.size()) {
    llvm::SmallString<128> Report(CallReportOutput);
    std::error_code EC;
    llvm::raw_fd_ostream ROS(Report, EC, llvm::sys::fs::CD_OpenExisting,
                             llvm::sys::fs::FA_Write,
                             llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
    if (!EC) {
      llvm::json::OStream JOS(ROS);
      JOS.object([&]() {
        for (const auto &pair : CallsDiag) {
          JOS.attributeObject(pair.first.first.str().c_str(), [&]() {
            for (const auto &pair : CallsDiag) {
              JOS.attributeArray(pair.first.second, [&]() {
                for (llvm::StringRef str : pair.second)
                  JOS.value(str.str().c_str());
              });
            }
          });
        }
      });
    }
  }

  return 0;
}
