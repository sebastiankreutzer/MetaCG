//
// Created by ui72hona on 3/7/26.
//

#ifndef METACG_VCALLANALYSIS_H
#define METACG_VCALLANALYSIS_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/TypeMetadataUtils.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>

namespace cage {

class VirtualCallAnalyzer {
 public:
  VirtualCallAnalyzer(llvm::Module &M, bool stripThunks=true);

  /// Finds all virtual calls in the given function and returns candidate targets
  llvm::SmallPtrSet<llvm::Function*, 4> findVirtualCallTargets(llvm::Function &F);

 private:
  llvm::Module &M;
  bool stripThunks;

  struct VTableInfo {
    llvm::GlobalVariable *VTable;
    uint64_t AddressPoint;
  };

  using VTableSet = llvm::SmallVector<VTableInfo, 4>;

  llvm::DenseMap<const llvm::Metadata*, VTableSet> TypeIdMap;

  void buildTypeIdMap();
//  void buildVTableMap();

  const VTableSet* getVTables(llvm::Metadata *TypeID) const;

  llvm::SmallPtrSet<llvm::Function*, 4>  resolveVirtualCall(llvm::CallBase &CB, llvm::Metadata *TypeID,
                          uint64_t Offset, bool debug);
};

} // namespace cage

#endif  // METACG_VCALLANALYSIS_H
