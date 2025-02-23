/**
 * File: MCGReader.cpp
 * License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */
#include "MCGReader.h"
#include "MCGBaseInfo.h"
#include "Util.h"

#include "Timing.h"

#include "spdlog/spdlog.h"

#include <loadImbalance/LIMetaData.h>

using namespace pira;

namespace metacg {
namespace io {

/**
 * Base class
 */
MetaCGReader::FuncMapT::mapped_type &MetaCGReader::getOrInsert(const std::string &key) {
  if (functions.find(key) != functions.end()) {
    auto &fi = functions[key];
    return fi;
  } else {
    FunctionInfo fi;
    fi.functionName = key;
    functions.insert({key, fi});
    auto &rfi = functions[key];
    return rfi;
  }
}

template <typename FieldTy, typename JsonTy>
void MetaCGReader::setIfNotNull(FieldTy &field, const JsonTy &jsonValue, const std::string &key) {
  auto jsonField = jsonValue.value()[key];
  if (!jsonField.is_null()) {
    field = jsonField.template get<typename std::remove_reference<FieldTy>::type>();
  } else {
    spdlog::get("errconsole")->warn("Tried to read non-existing field {} for node.", key);
  }
}

void MetaCGReader::buildGraph(metacg::graph::MCGManager &cgManager, MetaCGReader::StrStrMap &potentialTargets) {
  metacg::RuntimeTimer rtt("buildGraph");
  auto console = spdlog::get("console");
  // Register nodes in the actual graph
  for (const auto &[k, fi] : functions) {
    console->trace("Inserting MetaCG node for function {}", k);
    auto node = cgManager.findOrCreateNode(k);  // node pointer currently unused
    node->setIsVirtual(fi.isVirtual);
    node->setHasBody(fi.hasBody);
    for (const auto &c : fi.callees) {
      cgManager.addEdge(k, c);
      auto &potTargets = potentialTargets[c];
      for (const auto &pt : potTargets) {
        cgManager.addEdge(k, pt);
      }
    }
  }
}

MetaCGReader::StrStrMap MetaCGReader::buildVirtualFunctionHierarchy(metacg::graph::MCGManager &cgManager) {
  metacg::RuntimeTimer rtt("buildVirtualFunctionHierarchy");
  // Now the functions map holds all the information
  std::unordered_map<std::string, std::unordered_set<std::string>> potentialTargets;
  for (const auto &[k, funcInfo] : functions) {
    if (!funcInfo.isVirtual) {
      // No virtual function, continue
      continue;
    }

    /*
     * The current function can: 1. override a function, or, 2. be overridden by a function
     *
     * (1) Add this function as potential target for any overridden function
     * (2) Add the overriding function as potential target for this function
     *
     */
    if (funcInfo.doesOverride) {
      for (const auto &overriddenFunction : funcInfo.overriddenFunctions) {
        // Adds this function as potential target to all overridden functions
        potentialTargets[overriddenFunction].insert(k);

        // In IPCG files, only the immediate overridden functions are stored currently.
        std::queue<std::string> workQ;
        std::set<std::string> visited;
        workQ.push(overriddenFunction);
        // Add this function as a potential target for all overridden functions
        while (!workQ.empty()) {
          const auto next = workQ.front();
          workQ.pop();

          const auto fi = functions[next];
          visited.insert(next);
          spdlog::get("console")->debug("In while: working on {}", next);

          potentialTargets[next].insert(k);
          for (const auto &om : fi.overriddenFunctions) {
            if (visited.find(om) == visited.end()) {
              spdlog::get("console")->debug("Adding {} to the list to process", om);
              workQ.push(om);
            }
          }
        }
      }
    }
  }

  for (const auto &[k, s] : potentialTargets) {
    std::string targets;
    for (const auto t : s) {
      targets += t + ", ";
    }
    spdlog::get("console")->debug("Potential call targets for {}: {}", k, targets);
  }

  return potentialTargets;
}

/**
 * Version one Reader
 */
void VersionOneMetaCGReader::read(metacg::graph::MCGManager &cgManager) {
  metacg::RuntimeTimer rtt("Version One Reader");

  spdlog::get("console")->trace("Reading");
  auto j = source.get();

  for (json::iterator it = j.begin(); it != j.end(); ++it) {
    spdlog::get("console")->trace("Inserting node for key {}", it.key());
    auto &fi = getOrInsert(it.key());

    /* This is structural and basic information */
    fi.functionName = it.key();
    setIfNotNull(fi.hasBody, it, "hasBody");
    setIfNotNull(fi.isVirtual, it, "isVirtual");
    setIfNotNull(fi.doesOverride, it, "doesOverride");

    std::set<std::string> callees;
    setIfNotNull(callees, it, "callees");
    fi.callees.insert(callees.begin(), callees.end());

    std::set<std::string> ofs;
    setIfNotNull(ofs, it, "overriddenFunctions");
    fi.overriddenFunctions.insert(ofs.begin(), ofs.end());

    std::set<std::string> overriddenBy;
    setIfNotNull(overriddenBy, it, "overriddenBy");
    fi.overriddenBy.insert(overriddenBy.begin(), overriddenBy.end());

    std::set<std::string> ps;
    setIfNotNull(ps, it, "parents");
    fi.parents.insert(ps.begin(), ps.end());

    /* Meta information, will be refactored any way */
    setIfNotNull(fi.numStatements, it, "numStatements");

    // this needs to be done in a more generic way in the future!
    if (!it.value()["meta"].is_null() && !it.value()["meta"]["LIData"].is_null()) {
      fi.visited = it.value()["meta"]["LIData"].value("visited", false);
      fi.irrelevant = it.value()["meta"]["LIData"].value("irrelevant", false);
    }
  }

  auto potentialTargets = buildVirtualFunctionHierarchy(cgManager);
  buildGraph(cgManager, potentialTargets);
  addNumStmts(cgManager);

  // set load imbalance flags in CgNode
  for (const auto pfi : functions) {
    std::optional<CgNodePtr> opt_f = cgManager.getCallgraph()->getNode(pfi.first);
    if (opt_f.has_value()) {
      CgNodePtr node = opt_f.value();
      node->get<LoadImbalance::LIMetaData>()->setVirtual(pfi.second.isVirtual);

      if (pfi.second.visited) {
        node->get<LoadImbalance::LIMetaData>()->flag(LoadImbalance::FlagType::Visited);
      }

      if (pfi.second.irrelevant) {
        node->get<LoadImbalance::LIMetaData>()->flag(LoadImbalance::FlagType::Irrelevant);
      }
    }
  }
}

void VersionOneMetaCGReader::addNumStmts(metacg::graph::MCGManager &cgm) {
  for (const auto &[k, fi] : functions) {
    auto g = cgm.getCallgraph();
    auto node = g->getNode(fi.functionName);
    assert(node != nullptr && "Nodes with #statements attached should be available");
    if (node->has<pira::PiraOneData>()) {
      auto pod = node->get<pira::PiraOneData>();
      pod->setNumberOfStatements(fi.numStatements);
      pod->setHasBody(fi.hasBody);
    } else {
      assert(false && "pira::PiraOneData metadata should be default-constructed atm.");
    }
  }
}

/**
 * Version two Reader
 */
void VersionTwoMetaCGReader::read(metacg::graph::MCGManager &cgManager) {
  metacg::RuntimeTimer rtt("VersionTwoMetaCGReader::read");
  MCGFileFormatInfo ffInfo{2, 0};

  auto j = source.get();

  auto mcgInfo = j[ffInfo.metaInfoFieldName];
  if (mcgInfo.is_null()) {
    spdlog::get("console")->error("Could not read version info from metacg file.");
    throw std::runtime_error("Could not read version info from metacg file");
  }
  /// XXX How to make that we can use the MCGGeneratorVersionInfo to access the identifiers
  auto mcgVersion = mcgInfo["version"].get<std::string>();
  auto generatorName = mcgInfo["generator"]["name"].get<std::string>();
  auto generatorVersion = mcgInfo["generator"]["version"].get<std::string>();
  MCGGeneratorVersionInfo genVersionInfo{generatorName, metacg::util::getMajorVersionFromString(generatorVersion),
                                         metacg::util::getMinorVersionFromString(generatorVersion), ""};
  spdlog::get("console")->info("The metacg (version {}) file was generated with {} (version: {})", mcgVersion,
                               generatorName, generatorVersion);
  {  // raii
    std::string metaReadersStr;
    int i = 1;
    for (const auto mh : cgManager.getMetaHandlers()) {
      metaReadersStr += std::to_string(i) + ") " + mh->toolName() + "  ";
      ++i;
    }
    spdlog::get("console")->info("Executing the meta readers: {}", metaReadersStr);
  }

  MCGFileInfo fileInfo{ffInfo, genVersionInfo};
  auto jsonCG = j[ffInfo.cgFieldName];
  if (jsonCG.is_null()) {
    spdlog::get("console")->error("The call graph in the metacg file was null.");
    throw std::runtime_error("CG in metacg file was null.");
  }

  for (json::iterator it = jsonCG.begin(); it != jsonCG.end(); ++it) {
    auto &fi = getOrInsert(it.key());  // new entry for function it.key

    fi.functionName = it.key();

    /** Bi-directional graph information */
    std::unordered_set<std::string> callees;
    setIfNotNull(callees, it, fileInfo.nodeInfo.calleesStr);
    fi.callees = callees;
    std::unordered_set<std::string> parents;
    setIfNotNull(parents, it, fileInfo.nodeInfo.callersStr);  // Different name compared to version 1.0
    fi.parents = parents;

    /** Overriding information */
    setIfNotNull(fi.isVirtual, it, fileInfo.nodeInfo.isVirtualStr);
    setIfNotNull(fi.doesOverride, it, fileInfo.nodeInfo.doesOverrideStr);
    std::unordered_set<std::string> overriddenFunctions;
    setIfNotNull(overriddenFunctions, it, fileInfo.nodeInfo.overridesStr);
    fi.overriddenFunctions.insert(overriddenFunctions.begin(), overriddenFunctions.end());
    std::unordered_set<std::string> overriddenBy;
    setIfNotNull(overriddenBy, it, fileInfo.nodeInfo.overriddenByStr);
    fi.overriddenBy.insert(overriddenBy.begin(), overriddenBy.end());

    /** Information relevant for analysis */
    setIfNotNull(fi.hasBody, it, fileInfo.nodeInfo.hasBodyStr);
  }

  auto potentialTargets = buildVirtualFunctionHierarchy(cgManager);
  buildGraph(cgManager, potentialTargets);

  for (json::iterator it = jsonCG.begin(); it != jsonCG.end(); ++it) {
    /**
     * Pass each attached meta reader the current json object, to see if it has meta data
     *  particular to that reader attached.
     */
    auto &jsonElem = it.value()[fileInfo.nodeInfo.metaStr];
    if (!jsonElem.is_null()) {
      for (const auto metaHandler : cgManager.getMetaHandlers()) {
        if (jsonElem.contains(metaHandler->toolName())) {
          metaHandler->read(jsonElem, it.key());
        }
      }
    }
  }
}
}  // namespace io
}  // namespace metacg
