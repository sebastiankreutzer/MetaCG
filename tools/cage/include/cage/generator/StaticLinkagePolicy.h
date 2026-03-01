/**
* File: StaticLinkageMergePolicy.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
 */
#ifndef METACG_STATICLINKAGEPOLICY_H
#define METACG_STATICLINKAGEPOLICY_H

#include "MergePolicy.h"

using namespace metacg;
namespace cage {

/**
 * This policy simulates static linking.
 */
struct StaticLinkagePolicy : public MergePolicy {
  std::optional<MergeAction> findMatchingNode(const Callgraph& targetCG, const CgNode& sourceNode) const override;
};

}  // namespace cage

#endif  // METACG_STATICLINKAGEPOLICY_H
