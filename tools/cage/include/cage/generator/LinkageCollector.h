/**
* File: LinkageCollector.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
*/

#ifndef METACG_LINKAGECOLLECTOR_H
#define METACG_LINKAGECOLLECTOR_H

#include "MetaCollector.h"
#include "LinkageMD.h"

#include "llvm/IR/Function.h"

namespace cage {

class LinkageCollector : public FunctionLocalMetaCollector {
 public:
  std::unique_ptr<metacg::MetaData> runOnFunction(llvm::Function& F) override {
    return std::make_unique<LinkageMD>(
        F.getLinkage(),
        F.getVisibility());
  }
};

}  // namespace cage

#endif  // METACG_LINKAGECOLLECTOR_H
