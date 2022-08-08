#include "helper/common.h"
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/Mangle.h>

#include <iostream>

std::vector<std::string> mangleCtorDtor(const clang::FunctionDecl *const nd, clang::MangleContext *mc) {
  if (llvm::isa<clang::CXXConstructorDecl>(nd) || llvm::isa<clang::CXXDestructorDecl>(nd)) {
    std::vector<std::string> manglings;

    const auto mangleCXXCtorAs = [&](clang::CXXCtorType type, const clang::CXXConstructorDecl *nd) {
      std::string functionName;
      llvm::raw_string_ostream out(functionName);
#if LLVM_VERSION_MAJOR == 10
      mc->mangleCXXCtor(nd, type, out);
#else
      const clang::GlobalDecl GD(nd, type);
      mc->mangleName(GD, out);
#endif
      return out.str();
    };

    const auto mangleCXXDtorAs = [&](clang::CXXDtorType type, const clang::CXXDestructorDecl *nd) {
      std::string functionName;
      llvm::raw_string_ostream out(functionName);
#if LLVM_VERSION_MAJOR == 10
      mc->mangleCXXDtor(nd, type, out);
#else
      const clang::GlobalDecl GD(nd, type);
      mc->mangleName(GD, out);
#endif
      return out.str();
    };

    // We generate all potential manglings for Ctor/Dtor, as we don't know which one is the actual mangling.
    // Idk if we can figure it out at some point.
    if (const auto ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(nd)) {
      manglings.push_back(mangleCXXCtorAs(clang::CXXCtorType::Ctor_Complete, ctor));
      manglings.push_back(mangleCXXCtorAs(clang::CXXCtorType::Ctor_Base, ctor));
      manglings.push_back(mangleCXXCtorAs(clang::CXXCtorType::Ctor_Comdat, ctor));
      manglings.push_back(mangleCXXCtorAs(clang::CXXCtorType::Ctor_CopyingClosure, ctor));
      manglings.push_back(mangleCXXCtorAs(clang::CXXCtorType::Ctor_DefaultClosure, ctor));
    }
    if (const auto dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(nd)) {
      manglings.push_back(mangleCXXDtorAs(clang::CXXDtorType::Dtor_Complete, dtor));
      manglings.push_back(mangleCXXDtorAs(clang::CXXDtorType::Dtor_Deleting, dtor));
      manglings.push_back(mangleCXXDtorAs(clang::CXXDtorType::Dtor_Base, dtor));
      manglings.push_back(mangleCXXDtorAs(clang::CXXDtorType::Dtor_Comdat, dtor));
    }
    return manglings;
  }
  return {};
}

std::vector<std::string> getMangledName(clang::NamedDecl const *const nd) {
  if (!nd) {
    std::cerr << "NamedDecl was nullptr" << std::endl;
    assert(nd && "NamedDecl and MangleContext must not be nullptr");
    return {"__NO_NAME__"};
  }
  clang::ASTNameGenerator NG(nd->getASTContext());

  if (llvm::isa<clang::CXXRecordDecl>(nd) || llvm::isa<clang::CXXMethodDecl>(nd) ||
      llvm::isa<clang::ObjCInterfaceDecl>(nd) || llvm::isa<clang::ObjCImplementationDecl>(nd)) {
    return NG.getAllManglings(nd);
  }
  return {NG.getName(nd)};
}
