#include "config.h"

#include "CallgraphToJSON.h"
#include "MetaCollector.h"

#include "CallGraph.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <fstream>
#include <iostream>
#include <string>

static llvm::cl::OptionCategory cgc("CGCollector");
static llvm::cl::opt<bool> captureCtorsDtors("capture-ctors-dtors",
                                             llvm::cl::desc("Capture calls to Constructors and Destructors"),
                                             llvm::cl::cat(cgc));
static llvm::cl::opt<int> metacgFormatVersion("metacg-format-version",
                                              llvm::cl::desc("MetaCG file version to output, values={1,2}, default=1"),
                                              llvm::cl::cat(cgc));

typedef std::vector<MetaCollector *> MetaCollectorVector;

class CallGraphCollectorConsumer : public clang::ASTConsumer {
 public:
  CallGraphCollectorConsumer(MetaCollectorVector mcs, nlohmann::json &j) : _mcs(mcs), _json(j) {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
    callGraph.setCaptureCtorsDtors(captureCtorsDtors);
    callGraph.TraverseDecl(Context.getTranslationUnitDecl());

    for (const auto mc : _mcs) {
      mc->calculateFor(callGraph);
    }

    convertCallGraphToJSON(callGraph, _json, metacgFormatVersion);
  }

 private:
  CallGraph callGraph;
  MetaCollectorVector _mcs;
  nlohmann::json &_json;
};

class CallGraphCollectorFactory : clang::ASTFrontendAction {
 public:
  CallGraphCollectorFactory(MetaCollectorVector mcs, nlohmann::json &j) : _mcs(mcs), _json(j) {}

  std::unique_ptr<clang::ASTConsumer> newASTConsumer() {
    return std::unique_ptr<clang::ASTConsumer>(new CallGraphCollectorConsumer(_mcs, _json));
  }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer([[maybe_unused]] clang::CompilerInstance &compiler,
                                                        [[maybe_unused]] llvm::StringRef sr) {
    return std::unique_ptr<clang::ASTConsumer>(new CallGraphCollectorConsumer(_mcs, _json));
  }

 private:
  MetaCollectorVector _mcs;
  nlohmann::json &_json;
};

// TODO handle multiple inputs
int main(int argc, const char **argv) {
  if (argc < 2) {
    return -1;
  }
  metacgFormatVersion.setInitialValue(1);  // Have the old file format as default

  std::cout << "Running MetaCG::CGCollector (version " << CGCollector_VERSION_MAJOR << '.' << CGCollector_VERSION_MINOR
            << ")\nGit revision: " << MetaCG_GIT_SHA << std::endl;

  clang::tooling::CommonOptionsParser OP(argc, argv, cgc);
  clang::tooling::ClangTool CT(OP.getCompilations(), OP.getSourcePathList());

//  std::cout << "Sources: ";
//  for (auto src : OP.getSourcePathList()) {
//    std::cout << src << ", ";
//  }
//  std::cout << "\n";
//
//  auto cc = OP.getCompilations().getCompileCommands(OP.getSourcePathList().front());
//  std::cout << "Compile commands:\n";
//  for (auto c : cc) {
//    std::cout << c.Filename << ":\n";
//    for (auto cmd : c.CommandLine) {
//      std::cout << cmd << "\n";
//    }
//  }

  nlohmann::json j;
  auto noStmtsCollector = std::make_unique<NumberOfStatementsCollector>();
  auto foCollector = std::make_unique<FilePropertyCollector>();
  auto csCollector = std::make_unique<CodeStatisticsCollector>();
  auto mcCollector = std::make_unique<MallocVariableCollector>();
  auto pcCollector = std::make_unique<PointerCallCollector>();
  //auto tyCollector = std::make_unique<UniqueTypeCollector>();

  MetaCollectorVector mcs{noStmtsCollector.get()};
  mcs.push_back(foCollector.get());
  mcs.push_back(csCollector.get());
  mcs.push_back(mcCollector.get());
  mcs.push_back(pcCollector.get());
  //mcs.push_back(tyCollector.get());

  CT.run(
      clang::tooling::newFrontendActionFactory<CallGraphCollectorFactory>(new CallGraphCollectorFactory(mcs, j)).get());

  for (const auto mc : mcs) {
    addMetaInformationToJSON(j, mc->getName(), mc->getMetaInformation(), metacgFormatVersion);
  }

  std::string filename(*OP.getSourcePathList().begin());
  filename = filename.substr(0, filename.find_last_of(".")) + ".ipcg";

  std::ofstream file(filename);
  file << j << std::endl;

  return 0;
}
