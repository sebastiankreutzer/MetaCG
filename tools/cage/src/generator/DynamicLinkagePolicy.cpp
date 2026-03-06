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

enum CtorDtorInfo {
  Ctor, Dtor, None
};

// TOOD: This is suuuper hacky!
static CtorDtorInfo detectCtorDtor(std::string_view name) {
  if (name.length() < 3 || name.substr(0, 3) != "_ZN")
    return None;

  for (size_t i = 0; i + 1 < name.size(); ++i) {
    if (name[i] == 'C' || name[i] == 'D') {
      char v = name[i + 1];
      if (v == '1' || v == '2' || v == '3')
        return name[i] == 'C' ? Ctor : Dtor;
    }
  }
  return None;
}

std::vector<std::string> generateCtorDtorAliases(std::string_view mangled, bool isDtor) {
  std::vector<std::string> aliases;

  for (size_t i = 0; i + 1 < mangled.size(); ++i) {
    if (mangled[i] == (isDtor ? 'D' : 'C')) {
      char v = mangled[i + 1];
      if (v == '1' || v == '2' || v == '3') {

        for (char alt : {'1','2','3'}) {
          if (alt == v)
            continue;

          std::string candidate(mangled);
          candidate[i + 1] = alt;
          aliases.push_back(candidate);
        }
        break;
      }
    }
  }

  return aliases;
}

}  // namespace



std::optional<MergeAction> DynamicLinkagePolicy::findMatchingNode(const Callgraph& targetCG,
                                                                  const CgNode& sourceNode) const {

  CgNode* targetNode{nullptr};

  std::string newName{""};

  auto& matches = targetCG.getNodes(sourceNode.getFunctionName());
  if (matches.empty()) {
    if (auto type = detectCtorDtor(sourceNode.getFunctionName()); type != None) {
      for (auto& alias : generateCtorDtorAliases(sourceNode.getFunctionName(), type == Dtor)) {
        auto& aliasMatches = targetCG.getNodes(alias);
        if (!aliasMatches.empty()) {
//          std::cout << "  Matching ctor/dtor alias found: " << alias << "\n";
          targetNode = targetCG.getNode(aliasMatches.front());
          // Rename
          newName = alias;
          break;
        }
      }
    }
    if (!targetNode) {
      return {};
    }
  } else {
    targetNode = targetCG.getNode(matches.front());
  }

  assert(targetNode);

  auto* srcMD = sourceNode.get<cage::LinkageMD>();
  auto* tgtMD = targetNode->get<cage::LinkageMD>();

  bool srcHasBody = sourceNode.getHasBody();
  bool tgtHasBody = targetNode->getHasBody();

  if (!srcMD || !tgtMD) {
    // fallback to body-based behavior
    if (tgtHasBody || !srcHasBody)
      return MergeAction(targetNode->getId(), false, newName);
    return MergeAction(targetNode->getId(), true, newName);
  }

  LT srcL = srcMD->getLinkageType();
  LT tgtL = tgtMD->getLinkageType();

  VT srcV = srcMD->getVisibility();
  VT tgtV = tgtMD->getVisibility();


  // 1. Hidden visibility ->never participate in global resolution
  if (!isExported(srcV) || !isExported(tgtV)) {
    return {};
  }

  // 2. Definition beats declaration
  if (srcHasBody && !tgtHasBody)
    return MergeAction(targetNode->getId(), true, newName);

  if (!srcHasBody && tgtHasBody)
    return MergeAction(targetNode->getId(), false, newName);

  // 3. Local linkage -> DSO-local
  if (isLocal(srcL) || isLocal(tgtL)) {
    return {};
  }

  // 4. Existing strong default-visible symbol is not preempted
  if (isStrong(tgtL) && isInterposable(tgtV))
    return MergeAction(targetNode->getId(), false, newName);

  // 5. Strong overrides weak
  if (isStrong(srcL) && isWeak(tgtL))
    return MergeAction(targetNode->getId(), true, newName);

  // 6. Weak vs weak -> keep earlier (target)
  if (isWeak(srcL) && isWeak(tgtL))
    return MergeAction(targetNode->getId(), false, newName);

  // Fallback -> keep target node
  return MergeAction(targetNode->getId(), false, newName);
}

}  // namespace cage
