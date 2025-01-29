/**
* File: LoopMD.h
* License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
* https://github.com/tudasc/metacg/LICENSE.txt
 */

#ifndef LOOPMD_H
#define LOOPMD_H

#include "metadata/MetaData.h"

class LoopDepthMD : public metacg::MetaData::Registrar<LoopDepthMD> {
 public:
  static constexpr const char* key = "loopDepth";
  LoopDepthMD() = default;
  explicit LoopDepthMD(const nlohmann::json& j) {
    if (j.is_null()) {
      metacg::MCGLogger::instance().getConsole()->error("Could not retrieve meta data for {}", key);
      return;
    }
    loopDepth = j.get<int>();
  }

 private:
  LoopDepthMD(const LoopDepthMD& other) : loopDepth(other.loopDepth) {}

 public:
  nlohmann::json to_json() const final { return loopDepth; }

  virtual const char* getKey() const final { return key; }

  void merge(const MetaData& toMerge) final {
    assert(toMerge.getKey() == getKey() && "Trying to merge LoopDepthMD with meta data of different types");

    const LoopDepthMD* toMergeDerived = static_cast<const LoopDepthMD*>(&toMerge);

    loopDepth = std::max(toMergeDerived->loopDepth, loopDepth);
  }

  MetaData* clone() const final { return new LoopDepthMD(*this); }

  int loopDepth{0};
};

class GlobalLoopDepthMD : public metacg::MetaData::Registrar<GlobalLoopDepthMD> {
 public:
  GlobalLoopDepthMD() = default;
  static constexpr const char* key = "globalLoopDepth";

  explicit GlobalLoopDepthMD(const nlohmann::json& j) {
    if (j.is_null()) {
      metacg::MCGLogger::instance().getConsole()->error("Could not retrieve meta data for {}", key);
      return;
    }
    globalLoopDepth = j.get<int>();
  }

 private:
  GlobalLoopDepthMD(const GlobalLoopDepthMD& other) : globalLoopDepth(other.globalLoopDepth) {}

 public:
  nlohmann::json to_json() const final { return globalLoopDepth; }

  virtual const char* getKey() const final { return key; }

  void merge(const MetaData& toMerge) final {
    assert(toMerge.getKey() == getKey() && "Trying to merge GlobalLoopDepthMD with meta data of different types");

    metacg::MCGLogger::instance().getErrConsole()->warn(
        "GlobalLoopDepth can not be merged. In order to get the correct values, a seperate pass through the merged "
        "graph is needed.");

    // TODO: Implement global merge strategy. Fall back implementation below should be removed.

    const GlobalLoopDepthMD* toMergeDerived = static_cast<const GlobalLoopDepthMD*>(&toMerge);

    globalLoopDepth = std::max(globalLoopDepth, toMergeDerived->globalLoopDepth);
  }

  MetaData* clone() const final { return new GlobalLoopDepthMD(*this); }

  int globalLoopDepth{0};
};

class LoopCallDepthMD : public metacg::MetaData::Registrar<LoopCallDepthMD> {
 public:
  static constexpr const char* key = "loopCallDepth";
  LoopCallDepthMD() = default;
  explicit LoopCallDepthMD(const nlohmann::json& j) {
    metacg::MCGLogger::instance().getConsole()->trace("Running LoopCallDepthHandler::read from json");
    if (j.is_null()) {
      metacg::MCGLogger::instance().getConsole()->error("Could not retrieve meta data for {}", key);
      return;
    }

    loopFunctionMap = j.get<std::map<std::string, int>>();
  }

 private:
  LoopCallDepthMD(const LoopCallDepthMD& other) : loopFunctionMap(other.loopFunctionMap) {}

 public:
  nlohmann::json to_json() const final { return loopFunctionMap; }

  virtual const char* getKey() const final { return key; }

  void merge(const MetaData& toMerge) final {
    assert(toMerge.getKey() == getKey() && "Trying to merge LoopCallDepthMD with meta data of different types");

    const LoopCallDepthMD* toMergeDerived = static_cast<const LoopCallDepthMD*>(&toMerge);

    for (auto& [calledFunction, callDepths] : loopFunctionMap) {
      const auto other = toMergeDerived->loopFunctionMap.find(calledFunction);
      if (other != toMergeDerived->loopFunctionMap.end()) {
        const int otherv = other->second;
        if (otherv > callDepths) {
          callDepths = otherv;
        }
      }
    }
    for (const auto& [calledFunction, callDepths] : toMergeDerived->loopFunctionMap) {
      if (loopFunctionMap.find(calledFunction) == loopFunctionMap.end()) {
        loopFunctionMap[calledFunction] = callDepths;
      }
    }
  }

  MetaData* clone() const final { return new LoopCallDepthMD(*this); }

  std::map<std::string, int> loopFunctionMap;
};

#endif //LOOPMD_H