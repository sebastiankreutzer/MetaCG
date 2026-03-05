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

using namespace metacg;

namespace cage {

namespace {

using LT = cage::Linkage;

bool isLocal(LT L) { return L == LT::Internal || L == LT::Private; }

bool isStrong(LT L) {
  switch (L) {
    case LT::External:
    case LT::Internal:
    case LT::Private:
      return true;
    default:
      return false;  // weak/linkonce/common/etc.
  }
}

bool isWeak(LT L) {
  switch (L) {
    case LT::WeakAny:
    case LT::WeakODR:
    case LT::LinkOnceAny:
    case LT::LinkOnceODR:
    case LT::ExternalWeak:
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
