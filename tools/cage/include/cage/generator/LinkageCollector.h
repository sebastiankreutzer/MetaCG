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

static Linkage fromLLVMLinkage(llvm::GlobalValue::LinkageTypes llvmLinkage) {
  using LT = llvm::GlobalValue::LinkageTypes;

  switch (llvmLinkage) {
    case LT::ExternalLinkage:              return Linkage::External;
    case LT::AvailableExternallyLinkage:   return Linkage::AvailableExternally;
    case LT::LinkOnceAnyLinkage:           return Linkage::LinkOnceAny;
    case LT::LinkOnceODRLinkage:           return Linkage::LinkOnceODR;
    case LT::WeakAnyLinkage:               return Linkage::WeakAny;
    case LT::WeakODRLinkage:               return Linkage::WeakODR;
    case LT::AppendingLinkage:             return Linkage::Appending;
    case LT::InternalLinkage:              return Linkage::Internal;
    case LT::PrivateLinkage:               return Linkage::Private;
    case LT::ExternalWeakLinkage:          return Linkage::ExternalWeak;
    case LT::CommonLinkage:                return Linkage::Common;
    default:                               return Linkage::Unknown;
  }
}

static Visibility fromLLVMVisibility(llvm::GlobalValue::VisibilityTypes llvmVis) {
  using VT = llvm::GlobalValue::VisibilityTypes;

  switch (llvmVis) {
    case VT::DefaultVisibility:
      return Visibility::Default;
    case VT::HiddenVisibility:
      return Visibility::Hidden;
    case VT::ProtectedVisibility:
      return Visibility::Protected;
    default:
      return Visibility::Unknown;
  }
}

class LinkageCollector : public FunctionLocalMetaCollector {
 public:
  std::unique_ptr<metacg::MetaData> runOnFunction(llvm::Function& F) override {
    return std::make_unique<LinkageMD>(
        fromLLVMLinkage(F.getLinkage()),
        fromLLVMVisibility(F.getVisibility()));
  }
};

}  // namespace cage

#endif  // METACG_LINKAGECOLLECTOR_H
