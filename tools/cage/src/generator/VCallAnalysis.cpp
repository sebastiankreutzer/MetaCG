#include "VCallAnalysis.h"

using namespace llvm;
using namespace cage;

static Function* stripThunk(Function* F) {
  if (!F) return nullptr;

  if (F->getName().starts_with("_ZTh"))
    if (auto *BB = &F->front())
      for (auto &I : *BB)
        if (auto *CI = dyn_cast<CallInst>(&I))
          if (auto *Target = CI->getCalledFunction())
            return Target;

  return F;
}

/// Recursively prints the vtable layout of a global variable
void dumpVTableLayout(GlobalVariable *GV) {
  if (!GV || !GV->hasInitializer())
    return;

  Constant *Init = GV->getInitializer();
  outs() << "VTable: " << GV->getName() << " (" << Init->getType() << ")\n";

  SmallVector<std::pair<Constant*, uint64_t>, 16> Worklist;
  Worklist.push_back({Init, 0});

  while (!Worklist.empty()) {
    auto [C, Offset] = Worklist.pop_back_val();

    if (auto *CA = dyn_cast<ConstantArray>(C)) {
      for (unsigned i = 0; i < CA->getNumOperands(); ++i) {
        Constant *Elem = CA->getOperand(i)->stripPointerCasts();
        uint64_t ElemOffset = Offset + i * 8; // assume 64-bit pointers
        Worklist.push_back({Elem, ElemOffset});
      }
    } else if (auto *CS = dyn_cast<ConstantStruct>(C)) {
      uint64_t FieldOffset = 0;
      for (unsigned i = 0; i < CS->getNumOperands(); ++i) {
        Constant *Field = CS->getOperand(i)->stripPointerCasts();
        Worklist.push_back({Field, Offset + FieldOffset});
        // Approximate struct field offsets with DataLayout if you want exact
        FieldOffset += 8;
      }
    } else if (auto *F = dyn_cast<Function>(C)) {
      outs() << "  Offset " << Offset << " : Function " << F->getName() << "\n";
    } else if (auto *GVField = dyn_cast<GlobalVariable>(C)) {
      outs() << "  Offset " << Offset << " : GlobalVariable " << GVField->getName() << "\n";
    } else if (isa<ConstantPointerNull>(C)) {
      outs() << "  Offset " << Offset << " : null\n";
    } else {
      outs() << "  Offset " << Offset << " : Unknown " << *C << "\n";
    }
  }
}

VirtualCallAnalyzer::VirtualCallAnalyzer(Module &Mod, bool stripThunks) : M(Mod), stripThunks(stripThunks) {
  buildTypeIdMap();
}

void VirtualCallAnalyzer::buildTypeIdMap() {

  unsigned TypeMD = llvm::LLVMContext::MD_type;

  for (llvm::GlobalVariable &GV : M.globals()) {

    if (!GV.hasInitializer())
      continue;

    if (!GV.getName().starts_with("_ZTV"))
      continue;

//    dumpVTableLayout(&GV);

    llvm::SmallVector<llvm::MDNode*, 8> MDs;
    GV.getMetadata(TypeMD, MDs);

    if (MDs.empty())
      continue;

//    llvm::outs() << "Found vtable: " << GV.getName() << "\n";

    for (llvm::MDNode *MD : MDs) {

      if (MD->getNumOperands() != 2)
        continue;

      auto *OffsetConst =
          llvm::mdconst::dyn_extract<llvm::ConstantInt>(MD->getOperand(0));

      if (!OffsetConst)
        continue;

      uint64_t AddressPoint = OffsetConst->getZExtValue();

      llvm::Metadata *TypeID = MD->getOperand(1).get();

//      llvm::outs()
//          << "  TypeID entry at address point "
//          << AddressPoint << "\n";

      TypeIdMap[TypeID].push_back({&GV, AddressPoint});
    }
  }
}

SmallPtrSet<Function*, 4> collectCandidatesByTypeID(Metadata *TypeID, Module &M) {
  SmallPtrSet<Function*, 4> Candidates;
  for (Function &F : M.functions()) {
    if (F.isDeclaration()) continue;
    if (F.hasMetadata(LLVMContext::MD_type)) {
      llvm::SmallVector<llvm::MDNode*, 8> MDs;
      F.getMetadata(LLVMContext::MD_type, MDs);
      for (MDNode *MD : MDs) {
        if (MD->getOperand(1).get() == TypeID)
          Candidates.insert(&F);
      }
    }
  }
  return Candidates;
}


const VirtualCallAnalyzer::VTableSet* VirtualCallAnalyzer::getVTables(Metadata *TypeID) const {
  auto It = TypeIdMap.find(TypeID);
  return (It != TypeIdMap.end()) ? &It->second : nullptr;
}

SmallPtrSet<Function*, 4> VirtualCallAnalyzer::resolveVirtualCall(CallBase &CB,
                                             Metadata *TypeID,
                                             uint64_t Offset, bool debug) {
  SmallPtrSet<Function*, 4> Targets{};
  auto* VTableInfo = getVTables(TypeID);

  if (!VTableInfo) {
    if (debug)
      outs() << "No vtable found for TypeID\n";
    Targets = collectCandidatesByTypeID(TypeID, *CB.getModule());
    if (debug)
      outs() << " -> Found by TypeID only: " << Targets.size() << "\n";
    return Targets;
  }
  if (debug)
    outs() << "Found vtable for call " << CB << " at offset " << Offset <<  "\n";

  for (auto &Info : *VTableInfo) {
    uint64_t RealOffset = Info.AddressPoint + Offset;

    if (debug)
      outs() << "Real offset: " << RealOffset << "\n";

    auto [F, _] = llvm::getFunctionAtVTableOffset(Info.VTable, RealOffset, M);

    if (F) {
      if (debug)
        outs() << "Candidate target: " << F->getName() << "\n";
      Targets.insert(stripThunk(F));
    } else {
      if (debug)
        outs() << "Could not resolve function at given vtable offset\n";
    }
  }
  return Targets;

}

/// Scan a function for all devirtualizable calls and print candidate targets
SmallPtrSet<Function*, 4> VirtualCallAnalyzer::findVirtualCallTargets(Function &F) {
  if (F.isDeclaration()) return{};

  bool debug = F.getName() == "_ZN4Foam8fvMatrixIdE15solveSegregatedERKNS_10dictionaryE";

  DominatorTree DT;
  DT.recalculate(F);

  SmallVector<CallInst*, 8> TypeTestCalls;


//  llvm::outs() << "Finding type tests:\n" << F << "\n";

  // First collect all llvm.type.test calls in this function
  for (auto& BB : F) {
    for (Instruction& I : BB) {
      if (auto* CI = dyn_cast<CallInst>(&I)) {
        if (CI->getCalledFunction() && (CI->getCalledFunction()->getIntrinsicID() == Intrinsic::public_type_test
                                        || CI->getCalledFunction()->getIntrinsicID() == Intrinsic::type_test )) {
          TypeTestCalls.push_back(CI);
//          llvm::outs() << "Found type test: " << CI << "\n";
        }
      }
    }
  }

  SmallPtrSet<Function*, 4> AllTargets{};

  // Process each type.test call
  for (CallInst *CI : TypeTestCalls) {
    SmallVector<DevirtCallSite, 8> DevirtCalls;
    SmallVector<CallInst*, 8> Assumes;

    if(debug)
      llvm::outs() << "Trying to find call for type test: " << *CI << "\n";

    findDevirtualizableCallsForTypeTest(DevirtCalls, Assumes, CI, DT);

    for (auto &VCall : DevirtCalls) {
      if (debug)
        outs() << "VCall found: " << VCall.CB << "\n";
      Metadata *TypeID = nullptr;
      // Try to extract TypeID from the type.test call
      if (auto *MDV = dyn_cast<MetadataAsValue>(CI->getArgOperand(1)))
        TypeID = MDV->getMetadata();

      if (!TypeID) continue;

      if (debug) {
        outs() << "TypeID: " << cast<MDString>(TypeID)->getString() << "\n";
        outs() << "Slot offset: " << VCall.Offset << "\n";
      }

      auto Targets = resolveVirtualCall(VCall.CB, TypeID, VCall.Offset, debug);
      AllTargets.insert(Targets.begin(), Targets.end());
    }
  }
  return AllTargets;
}