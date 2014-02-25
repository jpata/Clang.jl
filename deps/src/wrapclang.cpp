#include <cstring>
#include <vector>
#include <set>
#include <cstdio>

extern "C" {
#include "clang-c/Index.h"
}

#define wci_st(rtype) \
  static inline void wci_save_##rtype(rtype i, char* o) \
    { memcpy(o, &i, sizeof(rtype)); } \
  static inline rtype wci_get_##rtype(char* b) \
    { rtype c; memcpy(&c, b, sizeof(rtype)); return c; } \
  extern "C" unsigned int wci_size_##rtype() { return sizeof(rtype); }

// Struct helpers: memcpy shenanigans due to no structs byval
wci_st(CXSourceLocation)
wci_st(CXSourceRange)
wci_st(CXTUResourceUsageEntry)
wci_st(CXTUResourceUsage)
wci_st(CXCursor)
wci_st(CXType)
wci_st(CXToken)
wci_st(CXString)

typedef std::vector<CXCursor> CursorList;
typedef std::set<CursorList*> allcl_t;
allcl_t allCursorLists;

// to traverse AST with cursor visitor
// TODO: replace this with a C container
CXChildVisitResult wci_visitCB(CXCursor cur, CXCursor par, CXClientData data)
{
  CursorList* cl = (CursorList*)data;
  cl->push_back(cur);
  return CXChildVisit_Continue;
}

extern "C" {
#include "wrapclang.h"

unsigned int wci_getChildren(char* cuin, CursorList* cl)
{
  CXCursor cu = wci_get_CXCursor(cuin);
  clang_visitChildren(cu, wci_visitCB, cl);
  return 0;
}

void wci_getCursorFile(char* cuin, char* cxsout)
{
  CXCursor cu = wci_get_CXCursor(cuin);
  CXSourceLocation loc = clang_getCursorLocation( cu );
  CXFile cxfile;
  clang_getExpansionLocation(loc, &cxfile, 0, 0, 0);
  wci_save_CXString(clang_getFileName(cxfile), cxsout);
}

CursorList* wci_createCursorList()
{
  CursorList* cl = new CursorList();
  allCursorLists.insert(cl);
  return cl;
}

void wci_disposeCursorList(CursorList* cl)
{
  cl->clear();
  allCursorLists.erase(cl);
}

unsigned int wci_sizeofCursorList(CursorList* cl)
{
  return cl->size();
}

void wci_getCLCursor(char* cuout, CursorList* cl, int cuid)
{
  wci_save_CXCursor((*cl)[cuid], cuout);
}

const char* wci_getCString(char* csin )
{
  CXString cxstr = wci_get_CXString(csin);
  return clang_getCString(cxstr);
}

void wci_disposeString(char* csin)
{
  CXString cxstr = wci_get_CXString(csin);
  clang_disposeString(cxstr);
}

void wci_getCursorExtent(char* _cursor, char* sourcerange_) {
  CXCursor l1 = wci_get_CXCursor(_cursor);
  CXSourceRange rx = clang_getCursorExtent(l1);
  wci_save_CXSourceRange(rx, sourcerange_);
}

//CINDEX_LINKAGE void clang_tokenize(CXTranslationUnit TU, CXSourceRange Range,
//                                   CXToken **Tokens, unsigned *NumTokens);
unsigned wci_tokenize(CXTranslationUnit tu, char *_range, CXToken **Tokens) {
  CXSourceRange range = wci_get_CXSourceRange(_range);
  unsigned NumTokens;
  clang_tokenize(tu, range, Tokens, &NumTokens);

  return NumTokens;
}

CXTranslationUnit wci_Cursor_getTranslationUnit(char* _cursor) {
  CXCursor cursor = wci_get_CXCursor(_cursor);
  return clang_Cursor_getTranslationUnit(cursor);
}

void wci_debug_token(CXTranslationUnit tu, char* _token) {
  CXToken tok = wci_get_CXToken(_token);
  int kind = clang_getTokenKind(tok);
  CXString spelling = clang_getTokenSpelling(tu, tok);
  printf("kind: %d spelling: %s\n", kind, clang_getCString(spelling));
}

void wci_print_tokens(CXTranslationUnit tu, char* _range) {
  CXSourceRange range = wci_get_CXSourceRange(_range);
  unsigned NumTokens;
  CXToken* Tokens;
  clang_tokenize(tu, range, &Tokens, &NumTokens);
  
  for (unsigned int i=0; i < NumTokens; i++) {
    CXToken tok = Tokens[i];
    int kind = clang_getTokenKind(tok);
    CXString spelling = clang_getTokenSpelling(tu, tok);
    printf("kind: %d spelling: %s\n", kind, clang_getCString(spelling));
  }
}

} // extern

#ifdef USE_CLANG_CPP

/* 
    Optional extended functionality using Clang C++ API

    wci_getMethodVTableIndex(CXCursor): get the vtable index of
        a member function.
    wci_getCXXMethodMangledName(CXCursor): get the mangled name
        of a C++ class method.

    NOTE: if this section is enabled, the resulting library *must*
    be compiled against all Clang libraries or else random segfaults
    and "ld inconsistency detected" warnings will result. The default
    libclang.so build is *not* sufficient.
*/


#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "clang/AST/ASTContext.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/AST/Type.h"

#include "CXCursor.h"
#include <string>
#include <iostream>
using namespace clang;

//class llvm::raw_string_ostream;
#define WCI_CHECK_DECL(D) \
  if(!D) { printf("unable to get cursor Decl\n"); return -1; }

extern "C" {

int wci_getCXXMethodVTableIndex(char* cuin)
{
  CXCursor cu = wci_get_CXCursor(cuin);

  const Decl* MD = cxcursor::getCursorDecl(cu);
  WCI_CHECK_DECL(MD);

  const CXXMethodDecl* CXXMethod;
  if ( !(CXXMethod = dyn_cast<CXXMethodDecl>(MD)) )
  {
//    printf("failed cast to CXXMethodDecl\n");
    return -1;
  }

  ASTContext &astctx = CXXMethod->getASTContext();
  // TODO: perf, is this cached?
  VTableContext ctx = VTableContext(astctx);

  // Clang dies at assert for constructor or destructor, see GlobalDecl.h:32-33
  if (!CXXMethod->isVirtual())
    return -1;
  else
    return ctx.getMethodVTableIndex(CXXMethod);
}

int wci_getCXXMethodMangledName(char* cuin, char* outbuf)
{
  CXCursor cu = wci_get_CXCursor(cuin);
  const Decl* MD = cxcursor::getCursorDecl(cu);
  WCI_CHECK_DECL(MD);

  const CXXMethodDecl* CXXMethod;
  if ( !(CXXMethod = dyn_cast<CXXMethodDecl>(MD)) )
  {
//    printf("failed cast to CXXMethodDecl\n");
    return -1;
  }

  ASTContext &astctx = CXXMethod->getASTContext();
  VTableContext ctx = VTableContext(astctx);

  std::string sbuf; 
  llvm::raw_string_ostream os(sbuf);
  
  MangleContext* mc = astctx.createMangleContext();
  if (mc->shouldMangleDeclName( dyn_cast<NamedDecl>(MD)) ) {
    mc->mangleName( dyn_cast<NamedDecl>(MD), os);
  }
  else
  {
    return 0;
  }
  os.flush();

  std::strcpy(outbuf, sbuf.c_str());
  return sbuf.size();
}

} // extern C
#undef __STDC_LIMIT_MACROS
#undef __STDC_CONSTANT_MACROS

#endif // USE_CLANG_CPP

