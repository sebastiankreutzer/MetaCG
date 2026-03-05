/**
* File: LinkageMD.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
*/
#ifndef CAGE_LINKAGEMD_H
#define CAGE_LINKAGEMD_H

#include "metadata/MetaData.h"

using namespace metacg;

namespace cage {

// Mirrors LLVM's linkage types
enum class Linkage : std::uint8_t {
  External,
  AvailableExternally,
  LinkOnceAny,
  LinkOnceODR,
  WeakAny,
  WeakODR,
  Appending,
  Internal,
  Private,
  ExternalWeak,
  Common,
  Unknown
};

enum class Visibility : std::uint8_t {
  Default,
  Hidden,
  Protected,
  Unknown
};

using LT = Linkage;
using VT = Visibility;

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

  explicit LinkageMD(LT linkage,
                     VT vis)
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

  void setLinkageType(LT linkage) {
    linkageType = linkage;
  }

  void setVisibility(VT vis) {
    visibility = vis;
  }

  LT getLinkageType() const {
    return linkageType;
  }

  VT getVisibility() const {
    return visibility;
  }

 private:
  // === Helper functions ===============================================

  static std::string linkageToString(LT l) {
    switch (l) {
      case LT::External: return "external";
      case LT::AvailableExternally: return "available_externally";
      case LT::LinkOnceAny: return "linkonce";
      case LT::LinkOnceODR: return "linkonce_odr";
      case LT::WeakAny: return "weak";
      case LT::WeakODR: return "weak_odr";
      case LT::Appending: return "appending";
      case LT::Internal: return "internal";
      case LT::Private: return "private";
      case LT::ExternalWeak: return "external_weak";
      case LT::Common: return "common";
      default: return "unknown";
    }
  }

  static std::string visibilityToString(VT v) {
    switch (v) {
      case VT::Default:   return "default";
      case VT::Hidden:    return "hidden";
      case VT::Protected: return "protected";
    }
    return "default";
  }

  static LT
  stringToLinkage(const std::string& s) {
    if (s == "external") return LT::External;
    if (s == "available_externally") return LT::AvailableExternally;
    if (s == "linkonce") return LT::LinkOnceAny;
    if (s == "linkonce_odr") return LT::LinkOnceODR;
    if (s == "weak") return LT::WeakAny;
    if (s == "weak_odr") return LT::WeakODR;
    if (s == "appending") return LT::Appending;
    if (s == "internal") return LT::Internal;
    if (s == "private") return LT::Private;
    if (s == "external_weak") return LT::ExternalWeak;
    if (s == "common") return LT::Common;

    return LT::External;
  }

  static VT
  stringToVisibility(const std::string& s) {
    if (s == "hidden")
      return VT::Hidden;
    if (s == "protected")
      return VT::Protected;

    return VT::Default;
  }

  static LT
  strongerLinkage(LT a,
                  LT b) {

    if (a == LT::External|| b == LT::External)
      return LT::External;
    if (a == LT::Internal || b == LT::Internal)
      return LT::Internal;

    return a;
  }

  static VT
  moreRestrictiveVisibility(VT a,
                            VT b) {

    if (a == VT::Hidden || b == VT::Hidden)
      return VT::Hidden;
    if (a == VT::Protected || b == VT::Protected)
      return VT::Protected;

    return VT::Default;
  }

 private:
  Linkage linkageType{LT::External};
  Visibility visibility{VT::Default};
};

}  // namespace cage

#endif  // CAGE_LINKAGEMD_H