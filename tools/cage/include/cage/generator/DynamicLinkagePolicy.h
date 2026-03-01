/**
* File: DynamicLinkagePolicy.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
 */
#ifndef METACG_DYNAMICLINKAGEPOLICY_H
#define METACG_DYNAMICLINKAGEPOLICY_H

#include "MergePolicy.h"

using namespace metacg;

namespace cage {

/**
 * This policy simulates static linking.
 */
struct DynamicLinkagePolicy : public MergePolicy {
  std::optional<MergeAction> findMatchingNode(const Callgraph& targetCG, const CgNode& sourceNode) const override;
};

}  // namespace cage


#endif  // METACG_DYNAMICLINKAGEPOLICY_H
