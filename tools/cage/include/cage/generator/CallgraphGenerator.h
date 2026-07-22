/**
 * File: CallGraphGenerator.h
 * License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */
#include "cage/interface/CaGePlugin.h"
#include "cage/generator/CollectorPass.h"

#include "metacg/io/MCGWriter.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

#include <vector>

namespace cage {

enum PTAType { No, BySignature };

struct PassManager {
  void registerCollectorPass(std::unique_ptr<CollectorPass> pass) {
    collectorPasses.push_back(std::move(pass));
  }

  void registerConsumer(std::unique_ptr<CallGraphConsumer> consumer) {
    cosumers.push_back(std::move(consumer));
  }

  void run() {

  }

 private:
  std::vector<std::unique_ptr<CollectorPass>> collectorPasses;
  std::vector<std::unqiue_ptr<CallGraphConsumer>> consumers;
};

class Generator {
 public:
  explicit Generator(PTAType ptaType) : ptaType(ptaType) {};

  void addPlugin(std::unique_ptr<Plugin> consumer) { plugins.push_back(std::move(consumer)); }

  bool run(llvm::Module& M, llvm::ModuleAnalysisManager* MA);

 private:
  PTAType ptaType;
  std::vector<std::unique_ptr<Plugin>> plugins;
};

}  // namespace cage
