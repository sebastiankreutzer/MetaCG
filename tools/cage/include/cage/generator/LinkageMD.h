/**
* File: LinkageMD.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
*/
#ifndef CAGE_LINKAGEMD_H
#define CAGE_LINKAGEMD_H

#include "metadata/MetaData.h"

#include "llvm/IR/GlobalValue.h"

using namespace metacg;

namespace cage {

using LT = llvm::GlobalValue::LinkageTypes;

class LinkageMD : public metacg::MetaData::Registrar<LinkageMD> {
 public:
  static constexpr const char* key = "linkage";

  // === Constructors ===================================================

  LinkageMD() = default;

  explicit LinkageMD(const nlohmann::json& j, StrToNodeMapping&) {
    metacg::MCGLogger::instance().getConsole()->trace("Reading LinkageMD from json");

    if (j.is_null()) {
      metacg::MCGLogger::instance().getConsole()->trace(
          "Could not retrieve meta data for {}", "LinkageMD");
      return;
    }

    linkageType = stringToLinkage(j.value("linkageType", "ExternalLinkage"));
    visibility   = stringToVisibility(j.value("visibility", "DefaultVisibility"));
  }

  explicit LinkageMD(llvm::GlobalValue::LinkageTypes linkage,
                     llvm::GlobalValue::VisibilityTypes vis)
      : linkageType(linkage), visibility(vis) {}

 private:
  LinkageMD(const LinkageMD& other)
      : linkageType(other.linkageType),
        visibility(other.visibility) {}

 public:
  // === Serialization ==================================================

  nlohmann::json toJson(NodeToStrMapping&) const final {
    return nlohmann::json{
        {"linkageType", linkageToString(linkageType)},
        {"visibility", visibilityToString(visibility)}
    };
  }

  const char* getKey() const override { return key; }

  // === Merge ==========================================================

  void merge(const MetaData& toMerge,
             std::optional<MergeAction> a,
             const GraphMapping&) final {
    assert(toMerge.getKey() == getKey() &&
           "Trying to merge LinkageMD with meta data of different types");

    const auto* other = static_cast<const LinkageMD*>(&toMerge);

    if (a.has_value() && a->replace) {
      linkageType = other->linkageType;
      visibility = other->visibility;
    } else {
//      metacg::MCGLogger::logWarnUnique("Merging functions with ")
    }

  }

  std::unique_ptr<MetaData> clone() const final {
    return std::unique_ptr<MetaData>(new LinkageMD(*this));
  }

  void applyMapping(const GraphMapping&) override {}

  // === Accessors ======================================================

  void setLinkageType(llvm::GlobalValue::LinkageTypes linkage) {
    linkageType = linkage;
  }

  void setVisibility(llvm::GlobalValue::VisibilityTypes vis) {
    visibility = vis;
  }

  llvm::GlobalValue::LinkageTypes getLinkageType() const {
    return linkageType;
  }

  llvm::GlobalValue::VisibilityTypes getVisibility() const {
    return visibility;
  }

 private:
  // === Helper functions ===============================================

  static std::string linkageToString(llvm::GlobalValue::LinkageTypes l) {
    switch (l) {
      case LT::ExternalLinkage: return "ExternalLinkage";
      case LT::AvailableExternallyLinkage: return "AvailableExternallyLinkage";
      case LT::LinkOnceAnyLinkage: return "LinkOnceAnyLinkage";
      case LT::LinkOnceODRLinkage: return "LinkOnceODRLinkage";
      case LT::WeakAnyLinkage: return "WeakAnyLinkage";
      case LT::WeakODRLinkage: return "WeakODRLinkage";
      case LT::AppendingLinkage: return "AppendingLinkage";
      case LT::InternalLinkage: return "InternalLinkage";
      case LT::PrivateLinkage: return "PrivateLinkage";
      case LT::ExternalWeakLinkage: return "ExternalWeakLinkage";
      case LT::CommonLinkage: return "CommonLinkage";
      default: return "Unknown";
    }
  }

  static std::string visibilityToString(llvm::GlobalValue::VisibilityTypes v) {
    switch (v) {
      case llvm::GlobalValue::DefaultVisibility:   return "DefaultVisibility";
      case llvm::GlobalValue::HiddenVisibility:    return "HiddenVisibility";
      case llvm::GlobalValue::ProtectedVisibility: return "ProtectedVisibility";
    }
    return "DefaultVisibility";
  }

  static llvm::GlobalValue::LinkageTypes
  stringToLinkage(const std::string& s) {
    if (s == "external") return LT::ExternalLinkage;
    if (s == "available_externally") return LT::AvailableExternallyLinkage;
    if (s == "linkonce") return LT::LinkOnceAnyLinkage;
    if (s == "linkonce_odr") return LT::LinkOnceODRLinkage;
    if (s == "weak") return LT::WeakAnyLinkage;
    if (s == "weak_odr") return LT::WeakODRLinkage;
    if (s == "appending") return LT::AppendingLinkage;
    if (s == "internal") return LT::InternalLinkage;
    if (s == "private") return LT::PrivateLinkage;
    if (s == "extern_weak") return LT::ExternalWeakLinkage;
    if (s == "common") return LT::CommonLinkage;

    return LT::ExternalLinkage;
  }

  static llvm::GlobalValue::VisibilityTypes
  stringToVisibility(const std::string& s) {
    using VT = llvm::GlobalValue::VisibilityTypes;

    if (s == "HiddenVisibility" || s == "hidden")
      return VT::HiddenVisibility;
    if (s == "ProtectedVisibility" || s == "protected")
      return VT::ProtectedVisibility;

    return VT::DefaultVisibility;
  }

  static llvm::GlobalValue::LinkageTypes
  strongerLinkage(llvm::GlobalValue::LinkageTypes a,
                  llvm::GlobalValue::LinkageTypes b) {

    if (a == LT::ExternalLinkage || b == LT::ExternalLinkage)
      return LT::ExternalLinkage;
    if (a == LT::InternalLinkage || b == LT::InternalLinkage)
      return LT::InternalLinkage;

    return a;
  }

  static llvm::GlobalValue::VisibilityTypes
  moreRestrictiveVisibility(llvm::GlobalValue::VisibilityTypes a,
                            llvm::GlobalValue::VisibilityTypes b) {
    using VT = llvm::GlobalValue::VisibilityTypes;

    if (a == VT::HiddenVisibility || b == VT::HiddenVisibility)
      return VT::HiddenVisibility;
    if (a == VT::ProtectedVisibility || b == VT::ProtectedVisibility)
      return VT::ProtectedVisibility;

    return VT::DefaultVisibility;
  }

 private:
  llvm::GlobalValue::LinkageTypes linkageType{
      llvm::GlobalValue::ExternalLinkage};
  llvm::GlobalValue::VisibilityTypes visibility{
      llvm::GlobalValue::DefaultVisibility};
};

}  // namespace cage

#endif  // CAGE_LINKAGEMD_H