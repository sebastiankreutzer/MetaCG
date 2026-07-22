/**
* File: CollectorPass.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
*/
#ifndef METACG_CAGE_COLLECTORPASS_H
#define METACG_CAGE_COLLECTORPASS_H

#include "metacg/Callgraph.h"
#include "llvm/IR/Module.h"

namespace cage {

class CollectorPass {
 public:
  virtual void run(llvm::Module&, metacg::Callgraph&) = 0;
};

class FunctionMetaCollector : public CollectorPass {
 public:
  virtual std::unique_ptr<metacg::MetaData> runOnFunction(llvm::Function&) = 0;

  void run(llvm::Module& M, metacg::Callgraph& cg) override {
    for (auto& F : M) {
      auto node = cg.getFirstNode(F.getName().str());
      if (!node) {
        continue;
      }
      if (auto md = runOnFunction(F)) {
        node->addMetaData(std::move(md));
      }
    }
  }
};

}  // namespace cage

#endif  // METACG_CAGE_COLLECTORPASS_H
