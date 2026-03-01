/**
* File: StaticLinkageMergePolicy.cpp
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
 */

#include "MergePolicy.h"
#include "Callgraph.h"
#include "LoggerUtil.h"

#include "cage/generator/LinkageMD.h"
#include "cage/generator/StaticLinkagePolicy.h"

#include "llvm/IR/GlobalValue.h"

using namespace metacg;

namespace cage {

namespace {

using LT = llvm::GlobalValue::LinkageTypes;

bool isLocal(LT L) { return L == LT::InternalLinkage || L == LT::PrivateLinkage; }

bool isStrong(LT L) {
  switch (L) {
    case LT::ExternalLinkage:
    case LT::InternalLinkage:
    case LT::PrivateLinkage:
      return true;
    default:
      return false;  // weak/linkonce/common/etc.
  }
}

bool isWeak(LT L) {
  switch (L) {
    case LT::WeakAnyLinkage:
    case LT::WeakODRLinkage:
    case LT::LinkOnceAnyLinkage:
    case LT::LinkOnceODRLinkage:
    case LT::ExternalWeakLinkage:
      return true;
    default:
      return false;
  }
}

}  // namespace

std::optional<MergeAction> StaticLinkagePolicy::findMatchingNode(const Callgraph& targetCG,
                                                                 const CgNode& sourceNode) const {
  auto& matches = targetCG.getNodes(sourceNode.getFunctionName());
  if (matches.empty()) {
    return {};  // no match → copy
  }

  auto* targetNode = targetCG.getNode(matches.front());
  assert(targetNode);

  auto* srcMD = sourceNode.get<cage::LinkageMD>();
  auto* tgtMD = targetNode->get<cage::LinkageMD>();

  if (!srcMD || !tgtMD) {
    // fallback to body-based behavior
    if (targetNode->getHasBody() || !sourceNode.getHasBody())
      return MergeAction(targetNode->getId(), false);
    return MergeAction(targetNode->getId(), true);
  }

  LT srcL = srcMD->getLinkageType();
  LT tgtL = tgtMD->getLinkageType();

  bool srcHasBody = sourceNode.getHasBody();
  bool tgtHasBody = targetNode->getHasBody();

  // 1. Local symbols never merge across modules
  if (isLocal(srcL) || isLocal(tgtL)) {
    return {};
  }

  // 2. Definition beats declaration
  if (srcHasBody && !tgtHasBody)
    return MergeAction(targetNode->getId(), true);

  if (!srcHasBody && tgtHasBody)
    return MergeAction(targetNode->getId(), false);

  // 3. Strong beats weak
  bool srcStrong = isStrong(srcL);
  bool tgtStrong = isStrong(tgtL);

  if (srcStrong && !tgtStrong)
    return MergeAction(targetNode->getId(), true);

  if (!srcStrong && tgtStrong)
    return MergeAction(targetNode->getId(), false);

  // 4. Weak vs weak -> merge (ODR assumption)
  if (isWeak(srcL) && isWeak(tgtL))
    return MergeAction(targetNode->getId(), true);

  // 5. Strong vs strong
  if (srcStrong && tgtStrong && srcHasBody && tgtHasBody) {
    MCGLogger::logWarn("Multiple strong definitions for '{}' during static merge.", sourceNode.getFunctionName());
  }

  return MergeAction(targetNode->getId(), false);
}

}  // namespace cage
