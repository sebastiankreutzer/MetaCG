/**
 * File: CallGraphGenerator.cpp
 * License: Part of the MetaCG project. Licensed under BSD 3 clause license. See LICENSE.txt file at
 * https://github.com/tudasc/metacg/LICENSE.txt
 */
#include "cage/generator/CallgraphGenerator.h"
#include "cage/generator/NumInstructionsCollector.h"
#include "cage/generator/LinkageCollector.h"

#include "Callgraph.h"

#ifdef HAVE_METAVIRT
#include "metavirt/VirtCall.h"
#endif
#include "VCallAnalysis.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/WholeProgramDevirt.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Analysis/TypeMetadataUtils.h"


#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstVisitor.h"

#include <cxxabi.h>

using namespace llvm;

namespace cage {

struct CallBaseVisitor : public llvm::InstVisitor<CallBaseVisitor> {
  CallBaseVisitor(llvm::CallGraph* lcg, PTAType pta, bool useDevirtMD, bool printProgress=false) : lcg(lcg), pta(pta), useDevirtMD(useDevirtMD), printProgress(printProgress), mcg(std::make_unique<metacg::Callgraph>()) {
    Module& m = lcg->getModule();
    numFuncs = m.getFunctionList().size();
    llvm::DebugInfoFinder dbg_finder{};
    dbg_finder.processModule(m);
    metaDataAvail = dbg_finder.subprogram_count() != 0;

    for (const auto& i : dbg_finder.subprograms()) {
      if (auto f = m.getFunction(i->getName())) {
        // We found the function with its normal name (C-Style function)
        functionInfoMap[f] = i;
      } else if (auto f = m.getFunction(i->getLinkageName())) {
        // We found the function via the linkage name (mangled name / C++ function)
        functionInfoMap[f] = i;
      } else {
        // assert(false);
      }
    }

    // Only build signature map if required for PTA
    if (pta == BySignature) {
      for (const auto& func : m.getFunctionList()) {
        signatureFunctionMap[func.getFunctionType()].push_back(&func.getFunction());
      }
    }

    if (useDevirtMD) {
      // TODO: Do we ever want thunks in the CG? Maybe make this an option.
      vcallAnalyzer = std::make_unique<VirtualCallAnalyzer>(m, true);
    }
  }

  ~CallBaseVisitor() = default;

  void visitCallBase(llvm::CallBase& I) {
    // Helper function to retrieve the current parent function
    auto getCurrentNode = [&]() -> metacg::CgNode*{
      auto* currentFunction = I.getParent()->getParent();
      auto sourceName = currentFunction->getName();
      if (sourceName.empty()) {
        llvm::errs() << "Function has no name!\n";
        return {};
      }
      auto& matchingNodes = mcg->getNodes(sourceName.str());
      metacg::CgNode* currentNode;
      if (matchingNodes.empty()) {
        llvm::errs() << "Could not find node for function " << currentFunction->getName() << " - inserting now\n";
        currentNode = &getOrInsertNode(currentFunction);
      } else {
        if (matchingNodes.size() > 1) {
          llvm::errs() << "Encountered duplicate nodes for function " << sourceName << "\n";
        }
        currentNode = mcg->getNode(matchingNodes[0]);
        if (!currentNode) {
          llvm::errs() << "Could not retrieve node with ID=" << matchingNodes[0] << "\n";
          return {};
        }
        return currentNode;
      }
    };

    if (auto callee = I.getCalledFunction()) {
      // resolving OMP calls
      if (callee->getName().starts_with("__kmpc")) {
        llvm::outs() << "Encountered OMP call: " << callee->getName() << "\n";
      }
      Function* outlinedKernel = nullptr;
      if (callee->getName() == "__kmpc_fork_call") {
        // Argument index 0: loc (ident_t*)
        // Argument index 1: argc (int32)
        // Argument index 2: microtask (void*)
        Value *kernelVal = I.getArgOperand(2);
        // Strip bitcasts to find the actual Function object
        outlinedKernel = dyn_cast<Function>(kernelVal->stripPointerCasts());
      }  else if (callee->getName() == "__kmpc_omp_task_alloc") {
        Value *taskKernelVal = I.getArgOperand(5);
        outlinedKernel = dyn_cast<Function>(taskKernelVal->stripPointerCasts());
      } else if (callee->getName().starts_with("GOMP_parallel") || callee->getName().starts_with("GOMP_task")) {
        // For GOMP_parallel, the function pointer is Arg 0
        Value *KernelVal = I.getArgOperand(0);
        // Strip bitcasts to find the actual Function object
        outlinedKernel = dyn_cast<Function>(KernelVal->stripPointerCasts());
      }
      if (outlinedKernel) {
        if (outlinedKernel->getName().contains(".omp_task_entry")) {
          // Look through the instructions in the entry wrapper
          for (auto &BB : *outlinedKernel) {
            for (auto &I : BB) {
              if (auto *CB = dyn_cast<CallBase>(&I)) {
                Function *realBody = CB->getCalledFunction();
                if (realBody && realBody->getName().contains(".omp_outlined")) {
                  llvm::outs() << "Found actual task body: " << realBody->getName() << "\n";
                  outlinedKernel = realBody;
                }
              }
            }
          }
        }
        llvm::outs() << "Found OpenMP Kernel: " << outlinedKernel->getName() << "\n";
        auto* currentNode = getCurrentNode();
        if (!currentNode) {
          return;
        }
        metacg::CgNode& childNode = getOrInsertNode(outlinedKernel);
        insertEdge(*currentNode, childNode);
      }

      return;
    }

    // only pass non-resolved calls to metavirt
    if (metaDataAvail && !useDevirtMD) {
      auto* currentNode = getCurrentNode();
      if (!currentNode) {
        return;
      }
      size_t numAddedCalls = addVirtualCallTargets(I, *currentNode);
      // This function pointer was a virtual call base, so we do not need to run the overapproximation
      if (numAddedCalls != 0)
        return;

      // metavirt turned up with nothing
      // --> was function pointer, where we can not get the called function
      if (pta == PTAType::BySignature) {
        const auto& possibleFuncs = signatureFunctionMap[I.getFunctionType()];
        for (const auto& func : possibleFuncs) {
          assert(func);
          auto& childNode = getOrInsertNode(func);
          insertEdge(*currentNode, childNode);
        }
      }
    }


  }



  void visitFunction(llvm::Function& F) {
    if (F.isIntrinsic())
      return;

    llvm::outs() << "In function " << F.getName() << ":\n";
    if (F.getName() == "_ZN4Foam8fvMatrixIdE15solveSegregatedERKNS_10dictionaryE") {
      llvm::outs() << "--------------------------\n" << F << "\n--------------------------\n";
    }

    auto& currentNode = getOrInsertNode(&F);

    if (useDevirtMD) {
      auto targets = vcallAnalyzer->findVirtualCallTargets(F);
      for (auto* target : targets) {
//        outs() << "Inserting vcall to " << target->getName() <<"\n";
        metacg::CgNode& childNode = getOrInsertNode(target);
        insertEdge(currentNode, childNode);
      }
    }

    auto* lcgNode = lcg->operator[](&F);
    for (auto& [key, elem] : *lcgNode) {
      if (!key.has_value())
        continue;
      if (elem->getFunction() == nullptr)
        continue;
      if (elem->getFunction()->isIntrinsic())
        continue;
      const Function* childFunc = elem->getFunction();
      assert(childFunc->hasName());
      metacg::CgNode& childNode = getOrInsertNode(childFunc);
      insertEdge(currentNode, childNode);
    }

    if (printProgress) {
      numProcessed++;
      size_t ratioInFives = (20*numProcessed) / numFuncs;
      if (ratioInFives > lastProgressReport) {
        outs() << "Processed " << (ratioInFives * 5) << "% of functions...\n";
        lastProgressReport = ratioInFives;
      }
    }
  }

  std::unique_ptr<metacg::Callgraph> takeResult() { return std::move(mcg); }

 private:
  size_t addVirtualCallTargets(CallBase& I, const metacg::CgNode& currentNode) {
    // TODO: Improve this design if we want to support multiple virtual call resolution mechanisms, e.g. with
    //       template policy parameter.

#ifdef HAVE_METAVIRT
    auto vcallData = metavirt::vcall_data_for(&I);
    if (!vcallData.has_value())
      return 0;
    if (vcallData.value().call_targets.empty())
      return 0;

    for (const auto& dataPoints : metavirt::fn_names_and_origins(vcallData.value())) {
      if (dataPoints.name.empty()) {
          llvm::outs() << "metavirt returned empty name! Skipping... \n";
          continue;
      }
      auto& childNode = mcg->getOrInsertNode(dataPoints.name.str(), dataPoints.origin.str());
      insertEdge(currentNode, childNode);
//      if (childNode.getOrigin() != dataPoints.origin)
//          llvm::outs() << "Origin mismatch: " << childNode.getOrigin() << ", " << dataPoints.origin << "\n";
      //assert(childNode.getOrigin() == dataPoints.origin);
    }
    return metavirt::fn_names_and_origins(vcallData.value()).size();
#else
    return 0;
#endif
  }

  metacg::CgNode& getOrInsertNode(const llvm::Function* F) {
    bool hasBody = !F->isDeclaration();
    StringRef nameToUse = F->getName();
    std::optional<std::string> origin{};
    if (metaDataAvail && functionInfoMap[F] != nullptr) {
      auto linkageName = functionInfoMap[F]->getLinkageName();
      if (!linkageName.empty()) {
        nameToUse = linkageName;
      }
      origin = std::filesystem::path(functionInfoMap[F]->getDirectory().str()) / functionInfoMap[F]->getFilename().str();
    }

    return mcg->getOrInsertNode(nameToUse.str(), std::move(origin), false, hasBody);
  }

  bool insertEdge(const metacg::CgNode& a, const metacg::CgNode& b) {
    if (mcg->existsEdge(a, b)) {
      return false;
    }
    mcg->addEdge(a, b);
    return true;
  }

  std::unique_ptr<metacg::Callgraph> mcg;
  llvm::CallGraph* lcg;
  PTAType pta;
  bool useDevirtMD = false;
  bool metaDataAvail = false;
  bool printProgress = false;
  size_t numFuncs{0};
  size_t numProcessed{0};
  size_t lastProgressReport{0};
  std::unique_ptr<cage::VirtualCallAnalyzer> vcallAnalyzer;

  std::unordered_map<const Function*, const llvm::DISubprogram*> functionInfoMap;
  std::unordered_map<llvm::FunctionType*, std::vector<const Function*>> signatureFunctionMap;
};

bool Generator::run(Module& M, ModuleAnalysisManager* MA) {
  {

    const char* useDevirtMdEnv = std::getenv("CAGE_USE_DEVIRT_MD");
    bool useDevirtMd{false};
    if (useDevirtMdEnv) {
      llvm::outs() << "Using devirt MD for vcall resolution\n";
      useDevirtMd = true;
    }

#ifdef HAVE_METAVIRT
      llvm::outs() << "Using metavirt for vcall resolution\n";
#endif
    auto& cgResult = MA->getResult<CallGraphAnalysis>(M);
    auto cbv = CallBaseVisitor(&cgResult, ptaType, useDevirtMd, printProgress);
    cbv.visit(M);
    
    // Take resulting metacg call graph
    auto mcg = cbv.takeResult();

    // Run metadata collectors
    NumInstructionsCollector nic;
    nic.run(M, *mcg);

    LinkageCollector lc;
    lc.run(M, *mcg);

    // Run registered consumers
    for (auto& consumer : consumers) {
      consumer->consumeCallGraph(*mcg);
    }
  }
  return false;
}
}  // namespace cage
