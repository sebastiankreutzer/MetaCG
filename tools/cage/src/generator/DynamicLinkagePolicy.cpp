/**
* File: DynamicLinkagePolicy.cpp
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
 */

#include "MergePolicy.h"
#include "Callgraph.h"
#include "LoggerUtil.h"

#include "cage/generator/LinkageMD.h"
#include "cage/generator/DynamicLinkagePolicy.h"

#include "MergePolicy.h"
#include "Callgraph.h"
#include "LoggerUtil.h"

using namespace metacg;

namespace cage {

namespace {

using LT = cage::Linkage;
using VT = cage::Visibility;

bool isLocal(LT L) { return L == LT::Internal || L == LT::Private; }

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

bool isStrong(LT L) { return !isWeak(L); }

bool isExported(VT V) { return V == VT::Default || V == VT::Protected; }

bool isInterposable(VT V) { return V == VT::Default; }

}  // namespace

std::optional<MergeAction> DynamicLinkagePolicy::findMatchingNode(const Callgraph& targetCG,
                                                                  const CgNode& sourceNode) const {
  auto& matches = targetCG.getNodes(sourceNode.getFunctionName());
  if (matches.empty()) {
    return {};  // no match → load symbol
  }

  auto* targetNode = targetCG.getNode(matches.front());
  assert(targetNode);

  auto* srcMD = sourceNode.get<cage::LinkageMD>();
  auto* tgtMD = targetNode->get<cage::LinkageMD>();

  if (!srcMD || !tgtMD) {
    return MergeAction(targetNode->getId(), false);
  }

  LT srcL = srcMD->getLinkageType();
  LT tgtL = tgtMD->getLinkageType();

  VT srcV = srcMD->getVisibility();
  VT tgtV = tgtMD->getVisibility();

  bool srcHasBody = sourceNode.getHasBody();
  bool tgtHasBody = targetNode->getHasBody();

  // 1. Hidden visibility ->never participate in global resolution
  if (!isExported(srcV) || !isExported(tgtV)) {
    return {};
  }

  // 2. Local linkage -> DSO-local
  if (isLocal(srcL) || isLocal(tgtL)) {
    return {};
  }

  // 3. Definition beats declaration
  if (srcHasBody && !tgtHasBody)
    return MergeAction(targetNode->getId(), true);

  if (!srcHasBody && tgtHasBody)
    return MergeAction(targetNode->getId(), false);

  // 4. Existing strong default-visible symbol is not preempted
  if (isStrong(tgtL) && isInterposable(tgtV))
    return MergeAction(targetNode->getId(), false);

  // 5. Strong overrides weak
  if (isStrong(srcL) && isWeak(tgtL))
    return MergeAction(targetNode->getId(), true);

  // 6. Weak vs weak -> keep earlier (target)
  if (isWeak(srcL) && isWeak(tgtL))
    return MergeAction(targetNode->getId(), false);

  return MergeAction(targetNode->getId(), false);
}

}  // namespace cage
