/* ==========================================================================
 *       Filename:  npi_code_gen.cpp
 *    Description:  
 *        Version:  1.0
 *        Created:  01/02/14 09:48:47 CST
 *       Revision:  none
 *        Company:  Synopsys
 * ========================================================================== */

#ifndef  NPI_CODE_GEN_CPP
#define  NPI_CODE_GEN_CPP

#include "npi_code_gen.h"
#include <assert.h>
#include <string.h>

//------------------------------------------------------------------------------
int npi_code_gen_extract_line_no(npiHandle hdl)
{
  if (!hdl)
    return npiUndefined;
  switch (npi_get(npiType, hdl)) {
    case npiModule:
    case npiInterface:
    case npiPackage:
      return npi_get(npiDefLineNo, hdl);
    case npiVhRoot:
    case npiVhComponent:
      return npi_get(npiVhArchLineNo, hdl);
    case npiVhEntity:
    case npiVhPackage:
    case npiVhPackBody:
    default:
      break;
  }
  return npi_get(npiLineNo, hdl);
}
//------------------------------------------------------------------------------
bool npi_code_gen_line_no_compare_func(npiHandle hdl1, npiHandle hdl2)
{
  assert(hdl1 && hdl2);
  int lineNo1 = npi_code_gen_extract_line_no(hdl1);
  int lineNo2 = npi_code_gen_extract_line_no(hdl2);
  return (lineNo1 < lineNo2)? true:false;
}
//------------------------------------------------------------------------------
npi_code_gen_t::npi_code_gen_t(const char* outputDir)
{
  _outputDir=outputDir?outputDir:"npiCodeGen";
  _hdlSet = new hdlSet_t;
  _archNameSet = new strSet_t;
  memset(_strBuf, '\0', sizeof(char)*STR_BUF_SIZE);
}
//------------------------------------------------------------------------------
npi_code_gen_t::~npi_code_gen_t()
{
  hdlSet_t::iterator hItr;
  for (hItr = _hdlSet->begin(); hItr != _hdlSet->end(); hItr++)
    npi_release_handle(*hItr);
  delete _hdlSet;
  delete _archNameSet;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::start_code_gen()
{
  // -- obtain all hierarchy instances -----------------------------------------
  hdlVec_t instHdlVec;
  npi_hier_tree_trv_inst(NULL/*from top*/, instHdlVec/*O*/);
  if (instHdlVec.empty())
    return false;

  // -- decompose instance to entity and instance(archBody or module) by file --
  file2ObjMap_t fileObjMap;
  if (!categorize_inst_vec_to_file_obj_map(instHdlVec, fileObjMap/*O*/))
    return false;

  // -- create code-gen directory ----------------------------------------------
  const char* dirName = get_output_dir_name();
  char* strBuf = get_str_buffer();
  sprintf(strBuf, "rm -rf %s", dirName);
  system(strBuf);
  sprintf(strBuf, "mkdir %s", dirName);
  system(strBuf);

  // -- generate code per file -------------------------------------------------
  file2ObjMap_t::iterator itr;
  for (itr = fileObjMap.begin(); itr != fileObjMap.end(); itr++)
    gen_code_per_file(itr);

  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::find_or_insert_file_obj(int fileProp, npiHandle hdl, file2ObjMap_t &fileObjMap)
{
  string fileName = npi_get_str(fileProp, hdl);
  hdlList_t* hdlList = NULL;
  hdlSet_t* hdlSet = get_handle_set();
  file2ObjMap_t::iterator foMapItr = fileObjMap.find(fileName);
  // new file
  if (foMapItr == fileObjMap.end()) {
    hdlList = new hdlList_t;
    hdlList->push_back(hdl);
    fileObjMap.insert(fileHdlListPair_t(fileName, hdlList));
    hdlSet->insert(hdl);
    return true;
  }
  // existing file
  hdlList = foMapItr->second;
  if (hdlSet->find(hdl) == hdlSet->end()) {
    hdlSet->insert(hdl);
    hdlList->push_back(hdl);
  }
  return false;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::categorize_inst_vec_to_file_obj_map(hdlVec_t &hdlVec, file2ObjMap_t &fileObjMap)
{
  npiHandle hdl = NULL;
  npiHandle extHdl = NULL;
  hdlVec_t::iterator itr;
  file2ObjMap_t::iterator foMapItr;
  for (itr = hdlVec.begin(); itr != hdlVec.end(); itr++) {
    hdl = *itr;
    switch (npi_get(npiType, hdl)) {
#if 0
      // SV
      case npiModule:
      case npiInterface:
      case npiPackage:
        find_or_insert_file_obj(npiDefFile, hdl, fileObjMap);
        break;
#endif
      // VHDL
      case npiVhRoot:
      case npiVhComponent:
        // -- check for architecture body --------------------------------------
        find_or_insert_file_obj(npiVhArchFile, hdl, fileObjMap);
        // -- check for entity -------------------------------------------------
        extHdl = npi_handle(npiVhEntity, hdl);
        find_or_insert_file_obj(npiFile, extHdl, fileObjMap);
        break;
      case npiVhPackage:
        // -- check for package (decl) -----------------------------------------
        find_or_insert_file_obj(npiFile, hdl, fileObjMap);
        // -- check for package body -------------------------------------------
        extHdl = npi_handle(npiVhPackBody, hdl);
        if (extHdl)
          find_or_insert_file_obj(npiFile, extHdl, fileObjMap);
        break;
      default:
        break;
    }
  }
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_code_per_file(file2ObjMap_t::iterator fileObj)
{
  string fileName = fileObj->first.data();
  extract_file_name(fileName/*IO*/);
  // -- sort handle list by lineNo ---------------------------------------------
  fileObj->second->sort(npi_code_gen_line_no_compare_func);

  // -- create file to write code ----------------------------------------------
  char* strBuf = get_str_buffer();
  sprintf(strBuf, "./%s/%s", get_output_dir_name(), fileName.data());
  FILE* fp = fopen(strBuf, "w");
  if (!fp) {
    assert(0);
    return false;
  }

  // -- itertate handles from list and generate code ---------------------------
  hdlList_t::iterator itr;
  strSet_t* strSet = NULL;
  string archName;
  for (itr = fileObj->second->begin(); itr != fileObj->second->end(); itr++) {
    switch (npi_get(npiType, *itr)) {
      case npiModule:
      case npiInterface:
      case npiPackage:
        // TODO
        break;
      case npiVhEntity:
        gen_vh_entity(fp, *itr, 0);
        break;
      case npiVhRoot:
      case npiVhComponent:
        // check if architecture has been generated
        strSet = get_arch_name_set();
        archName.assign(npi_get_str(npiVhArchName, *itr));
        archName += npi_get_str(npiVhEntityName, *itr);
        if (strSet->find(archName) != strSet->end())
          break; // ]
        strSet->insert(archName);
        gen_vh_architecture(fp, *itr, 0);
        break;
      case npiVhPackage:
        gen_vh_package(fp, *itr, 0);
        break;
      default:
        break;
    }
  }

  fclose(fp);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_entity(FILE* fp, npiHandle hdl, int indent)
{
  npiHandle libItr = npi_iterate(npiVhLibraryDecl, hdl);
  npiHandle useItr = npi_iterate(npiVhUseClause, hdl);
  npiHandle libDeclHdl = NULL, useClauseHdl = NULL;

  // -- gen library decl -------------------------------------------------------
  while ((libDeclHdl = npi_scan(libItr)))
    fprintf(fp, "library %s;\n", npi_get_str(npiName, libDeclHdl));
  // -- gen use clause ---------------------------------------------------------
  while ((useClauseHdl = npi_scan(useItr)))
    fprintf(fp, "use %s;\n", npi_get_str(npiName, useClauseHdl));
  fprintf(fp, "\n");
  // -- gen entity -------------------------------------------------------------
  fprintf(fp, "entity %s is\n", npi_get_str(npiName, hdl));
  // -- gen header -------------------------------------------------------------
  gen_vh_header(fp, hdl, indent);
  // -- gen decl ---------------------------------------------------------------
  gen_vh_declarative_part(fp, hdl, indent);
  // -- gen stmt ---------------------------------------------------------------
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  if (!itr) {
    fprintf(fp, "end;\n\n");
    return true;
  }
  int i;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "begin\n");
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");

  fprintf(fp, "end;\n\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_header(FILE* fp, npiHandle hdl, int indent, bool isGenMap)
{
  // gen generic clause
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhGeneric, hdl);
  int objCnt = 0;
  int objNum = npi_get(npiSize, itr);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_generic(fp, subHdl, indent+1, ++objCnt, objNum);
    npi_release_handle(subHdl);
  }
  // gen port clause
  npiHandle meHdl = NULL;
  npiHandle meItr = NULL;
  itr = npi_iterate(npiPort, hdl);
  objCnt = 0;
  objNum = npi_get(npiSize, itr);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_port(fp, subHdl, indent+1, ++objCnt, objNum);
    npi_release_handle(subHdl);
  }
  if (!isGenMap)
    return true;
  // [ TODO: gen generic map for block (KDB bug; task: 6100006881)
  int i = 0;
  int loopCnt = 0;
  itr = npi_iterate(npiVhGenericMap, hdl);
  if (itr) {
    for (i=0; i<indent; i++)
      fprintf(fp, "  ");
    fprintf(fp, "generic map(");
    while ((subHdl = npi_scan(itr))) {
      if (loopCnt++ > 0)
        fprintf(fp, ", ");
      fprintf(fp, "%s", get_vh_map_elem(subHdl));
      npi_release_handle(subHdl);
    }
    fprintf(fp, ");\n");
  }
  // ]
  // [ TODO: gen port map for block (KDB bug; task: 6100006881)
  loopCnt = 0;
  itr = npi_iterate(npiPort, hdl);
  if (!itr)
    return true;
  while ((subHdl = npi_scan(itr))) {
    if (loopCnt == 0) {
      for (i=0; i<indent; i++)
        fprintf(fp, "  ");
      fprintf(fp, " port map(");
    }
    meItr = npi_iterate(npiVhMapElem, subHdl);
    if (meItr) {
      while ((meHdl = npi_scan(meItr))) {
        if (loopCnt > 0)
          fprintf(fp, ", ");
        fprintf(fp, "%s", get_vh_map_elem(meHdl));
        npi_release_handle(meHdl);
      }
    }
    loopCnt++;
    npi_release_handle(subHdl);
  }
  fprintf(fp, ");\n"); // ]
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_declarative_part(FILE* fp, npiHandle hdl, int indent)
{
  int subIndent = indent + 1;
  // collect decl
  hdlList_t hdlList;
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhDecl, hdl);
  while ((subHdl = npi_scan(itr)))
    hdlList.push_back(subHdl);
  // collect spec
  itr = npi_iterate(npiVhSpec, hdl);
  while ((subHdl = npi_scan(itr)))
    hdlList.push_back(subHdl);
  // sort and gen code
  hdlList.sort(npi_code_gen_line_no_compare_func);
  hdlList_t::iterator hItr;
  int hdlType = npiUndefined;
  for (hItr = hdlList.begin(); hItr != hdlList.end(); hItr++) {
    hdlType = npi_get(npiType, *hItr);
    switch (hdlType) {
      case npiVhConst:
      case npiVhSig:
      case npiVhVar:
      case npiVhFuncDecl:
      case npiVhProcDecl:
      case npiVhFunction:
      case npiVhProcedure:
      case npiVhPackDecl:
      case npiVhPackBody:
      case npiVhEnumTypeDecl:
      case npiVhPhysTypeDecl:
      case npiVhIntTypeDecl:
      case npiVhFloatTypeDecl:
      case npiVhArrayTypeDecl:
      case npiVhRecordTypeDecl:
      case npiVhSubTypeDecl:
      case npiVhAttrDecl:
      case npiVhCompDecl:
        gen_vh_decl(fp, *hItr, subIndent);
        break;
      // [ non-decl
      case npiVhAttrSpec:
        gen_vh_attr_spec(fp, *hItr, subIndent);
        break;
      case npiVhUseClause:
        gen_vh_use_clause(fp, *hItr, subIndent);
        break;
      // ]
      case npiNIY:
        gen_niy_comment(fp, *hItr, subIndent);
        break;
      default:
        // TODO
        assert(0);
        break;
    }
    npi_release_handle(*hItr);
  }
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_niy_comment(FILE* fp, npiHandle hdl, int indent)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "-- NIY OBJ (KDB_TODO);\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_lang_interface(FILE* fp, npiHandle hdl, int indent)
{
  bool res = true;
  npiHandle actHdl = npi_handle(npiActual, hdl);
  if (!actHdl) {
    assert(0);
    return false;
  }
  switch (npi_get(npiLangType, actHdl)) {
    // actual is SV object (located at VHDL)
    case npiSvLang: {
      gen_vh_component(fp, actHdl, indent, hdl/*lang interface*/);
      break;
    }
    case npiVhdlLang:
    default:
      res = false;
      break;
  }
  npi_release_handle(actHdl);
  return res;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_architecture(FILE* fp, npiHandle hdl, int indent)
{
  // -- gen entity -------------------------------------------------------------
  npiHandle archBody = npi_handle(npiVhArchBody, hdl);
  fprintf(fp, "architecture %s ", npi_get_str(npiName, archBody));
  fprintf(fp, "of %s is\n", npi_get_str(npiVhEntityName, hdl));
  // -- gen decl ---------------------------------------------------------------
  gen_vh_declarative_part(fp, archBody, indent);
  // gen stmt
  fprintf(fp, "begin\n");
  npiHandle subHdl;
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  fprintf(fp, "end;\n\n");
  npi_release_handle(archBody);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_component(FILE* fp, npiHandle hdl, int indent, npiHandle langInterfaceHdl)
{
  int i = 0;
  const char* name = npi_get_str(npiName, hdl);
  int compIndent = strlen(name)+2;
  fprintf(fp, "%s: ", name);
  name = npi_get_str(langInterfaceHdl?npiDefName:npiVhEntityName, hdl);
  fprintf(fp, "%s", name);
  compIndent += strlen(name);
  npiHandle subHdl = NULL;
  int loopCnt = 0;
  npiHandle itr = NULL;
  if (langInterfaceHdl)
    itr = npi_iterate(npiParamMap, langInterfaceHdl);
  else
    itr = npi_iterate(npiVhGenericMap, hdl);
  if (itr) {
    fprintf(fp, " generic map(");
    while ((subHdl = npi_scan(itr))) {
      if (loopCnt++ > 0)
        fprintf(fp, ", ");
      fprintf(fp, "%s", get_vh_map_elem(subHdl));
      npi_release_handle(subHdl);
    }
    fprintf(fp, ")\n");
  }
  itr = npi_iterate(npiPort, hdl);
  if (npi_get(npiSize, itr) > 0) {
    npiHandle meItr = NULL;
    npiHandle meHdl = NULL;
    // if generic exists, add indent
    if (loopCnt > 0) {
      for (i=0; i<indent; i++)
        fprintf(fp, "  ");
      for (i=0; i<compIndent; i++)
        fprintf(fp, " ");
    }
    fprintf(fp, " port map(");
    loopCnt = 0;
    while ((subHdl = npi_scan(itr))) {
      meItr = npi_iterate(npiVhMapElem, subHdl);
      // open port
      if (!meItr) {
        if (loopCnt++ > 0)
          fprintf(fp, ", ");
        fprintf(fp, "%s => open", npi_get_str(npiName, subHdl));
        npi_release_handle(subHdl);
        continue;
      }
      // general map element
      while ((meHdl = npi_scan(meItr))) {
        if (loopCnt++ > 0)
          fprintf(fp, ", ");
        fprintf(fp, "%s", get_vh_map_elem(meHdl));
        npi_release_handle(meHdl);
      }
      npi_release_handle(subHdl);
    }
    fprintf(fp, ")");
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_pack_decl(FILE* fp, npiHandle hdl, int indent)
{
  // -- gen entity -------------------------------------------------------------
  fprintf(fp, "package %s is\n", npi_get_str(npiName, hdl));
  // -- gen header -------------------------------------------------------------
  gen_vh_header(fp, hdl, indent);
  // -- gen decl ---------------------------------------------------------------
  gen_vh_declarative_part(fp, hdl, indent);
  fprintf(fp, "end;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_pack_body(FILE* fp, npiHandle hdl, int indent)
{
  fprintf(fp, "package body %s is\n", npi_get_str(npiName, hdl));
  gen_vh_declarative_part(fp, hdl, indent);
  fprintf(fp, "end;\n\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_package(FILE* fp, npiHandle hdl, int indent)
{
  npiHandle libItr=NULL, useItr=NULL, libDeclHdl=NULL, useClauseHdl=NULL;
  // -- gen pack decl ----------------------------------------------------------
  npiHandle packDeclHdl = npi_handle(npiVhPackDecl, hdl);
  if (packDeclHdl) {
    libItr = npi_iterate(npiVhLibraryDecl, packDeclHdl);
    useItr = npi_iterate(npiVhUseClause, packDeclHdl);
    // gen library decl
    while ((libDeclHdl = npi_scan(libItr)))
      fprintf(fp, "library %s;\n", npi_get_str(npiName, libDeclHdl));
    // gen use clause
    while ((useClauseHdl = npi_scan(useItr)))
      fprintf(fp, "use %s;\n", npi_get_str(npiName, useClauseHdl));
    if (libItr)
      fprintf(fp, "\n");
    // gen pack decl
    gen_vh_pack_decl(fp, packDeclHdl, indent);
    fprintf(fp, "\n");
    npi_release_handle(packDeclHdl);
  }
  // -- gen pack body ----------------------------------------------------------
  npiHandle bodyHdl = npi_handle(npiVhPackBody, hdl);
  if (!bodyHdl)
    return true;
  libItr = npi_iterate(npiVhLibraryDecl, bodyHdl);
  useItr = npi_iterate(npiVhUseClause, bodyHdl);
  // gen library decl
  while ((libDeclHdl = npi_scan(libItr)))
    fprintf(fp, "library %s;\n", npi_get_str(npiName, libDeclHdl));
  // gen use clause
  while ((useClauseHdl = npi_scan(useItr)))
    fprintf(fp, "use %s;\n", npi_get_str(npiName, useClauseHdl));
  if (libItr)
    fprintf(fp, "\n");
  // gen pack body
  gen_vh_pack_body(fp, bodyHdl, indent+1);
  npi_release_handle(bodyHdl);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_process(FILE* fp, npiHandle hdl, int indent)
{
  string indentStr = "";
  int i = 0;
  for (i=0; i<indent; i++)
    indentStr.append("  ");

  static int isGenUnnamedProcessLabel = npi_util_getenv_int("TNPI_L1_GEN_UNNAMED_PROCESS_LABEL", 1);
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name) {
    if (isGenUnnamedProcessLabel > 0)
      fprintf(fp, "%s: ", name);
    else { // check if label name is automatically generated
      bool isAutoGenLab = true;
      int len = strlen(name);
      // automatically generated label naming rule: _P[0-9]+
      for (i=0; i<len; i++) {
        if (i == 0 && name[i] == '_')
          continue;
        if (i == 1 && name[i] == 'P')
          continue;
        if (name[i] >= '0' && name[i] <= '9')
          continue;
        isAutoGenLab = false;
        break;
      }
      if (!isAutoGenLab)
        fprintf(fp, "%s: ", name);
    }
  }
  // [ TODO: postponed // ]
  fprintf(fp, "process");
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhSensitivities, hdl);
  int sensCnt = 0;
  int sensNum = npi_get(npiSize, itr);
  if (sensNum > 0)
    fprintf(fp, "(");
  while ((subHdl = npi_scan(itr))) {
    if (sensCnt++ > 0)
      fprintf(fp, ", ");
    fprintf(fp, "%s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  if (sensNum > 0)
    fprintf(fp, ")");
  fprintf(fp, "\n");
  // decl
  itr = npi_iterate(npiVhDecl, hdl);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_decl(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  // stmt
  fprintf(fp, "%sbegin\n", indentStr.data());
  itr = npi_iterate(npiVhStmt, hdl);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  // end
  fprintf(fp, "%send process;\n", indentStr.data());
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_port(FILE* fp, npiHandle hdl, int indent, int portCnt, int portNum)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  // print port or port-indent
  assert(portCnt > 0);
  if (portCnt == 1)
    fprintf(fp, "port(");
  else
    fprintf(fp, "     ");
  // print port info
  fprintf(fp, "%s : ", npi_get_str(npiName, hdl));
  switch (npi_get(npiDirection, hdl)) {
    case npiInput:
      fprintf(fp, "in ");
      break;
    case npiOutput:
      fprintf(fp, "out ");
      break;
    case npiInout:
      fprintf(fp, "inout ");
      break;
    case npiVhBuffer:
      fprintf(fp, "buffer ");
      break;
    case npiVhLinkage:
      fprintf(fp, "linkage ");
      break;
    default:
      assert(0);
      break;
  }
  // type
  npiHandle typeHdl = npi_handle(npiVhType, hdl);
  if (typeHdl) {
    fprintf(fp, "%s", get_vh_type(typeHdl));
    npi_release_handle(typeHdl);
  }
  // init expr
  npiHandle initHdl = npi_handle(npiVhInitExpr, hdl);
  if (initHdl) {
    fprintf(fp, " := %s", get_vh_expr(initHdl));
    npi_release_handle(initHdl);
  }
  // print ending
  if (portCnt == portNum)
    fprintf(fp, ");\n");
  else
    fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_generic(FILE* fp, npiHandle hdl, int indent, int genericCnt, int genericNum)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  // print generic or generic-indent
  assert(genericCnt > 0);
  if (genericCnt == 1)
    fprintf(fp, "generic(");
  else
    fprintf(fp, "        ");
  // print generic info
  fprintf(fp, "%s : ", npi_get_str(npiName, hdl));
  // type
  npiHandle typeHdl = npi_handle(npiVhType, hdl);
  if (typeHdl) {
    fprintf(fp, "%s", get_vh_type(typeHdl));
    npi_release_handle(typeHdl);
  }
  // init expr
  npiHandle initHdl = npi_handle(npiVhInitExpr, hdl);
  if (initHdl) {
    fprintf(fp, " := %s", get_vh_expr(initHdl));
    npi_release_handle(initHdl);
  }
  // print ending
  if (genericCnt == genericNum)
    fprintf(fp, ");\n");
  else
    fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_decl(FILE* fp, npiHandle hdl, int indent)
{
  switch (npi_get(npiType, hdl)) {
    case npiVhConst:
    case npiVhSig:
    case npiVhVar:
      gen_vh_obj_decl(fp, hdl, indent);
      break;
    case npiVhFuncDecl:
    case npiVhProcDecl:
      gen_vh_subp_decl(fp, hdl, indent);
      break;
    case npiVhFunction:
    case npiVhProcedure:
      gen_vh_subp(fp, hdl, indent);
      break;
    case npiVhEnumTypeDecl:
    case npiVhPhysTypeDecl:
    case npiVhIntTypeDecl:
    case npiVhFloatTypeDecl:
    case npiVhArrayTypeDecl:
    case npiVhRecordTypeDecl:
    case npiVhSubTypeDecl:
      gen_vh_type(fp, hdl, indent);
      break;
    case npiVhAttrDecl:
      gen_vh_attr_decl(fp, hdl, indent);
      break;
    case npiVhCompDecl:
      gen_vh_comp_decl(fp, hdl, indent);
      break;
    case npiNIY:
      gen_niy_comment(fp, hdl, indent);
      break;
    case npiVhPackDecl:
    case npiVhPackBody:
    default:
      assert(0);
      return false;
  }

  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_obj_decl(FILE* fp, npiHandle hdl, int indent)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  switch (npi_get(npiType, hdl)) {
    case npiVhConst:
      fprintf(fp, "constant ");
      break;
    case npiVhSig:
      fprintf(fp, "signal ");
      break;
    case npiVhVar:
      fprintf(fp, "%svariable ", npi_get(npiVhShared, hdl)==1? "shared ":"");
      break;
    default:
      assert(0);
      return false;
  }
  fprintf(fp, "%s: ", npi_get_str(npiName, hdl));
  // type
  npiHandle typeHdl = npi_handle(npiVhType, hdl);
  if (typeHdl) {
    fprintf(fp, "%s", get_vh_type(typeHdl));
    npi_release_handle(typeHdl);
  }
  // init expr
  npiHandle initHdl = npi_handle(npiVhInitExpr, hdl);
  if (initHdl) {
    fprintf(fp, " := %s", get_vh_expr(initHdl));
    npi_release_handle(initHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_comp_decl(FILE* fp, npiHandle hdl, int indent)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "component %s\n", npi_get_str(npiName, hdl));
  npiHandle subHdl = NULL;
  // gen generic
  npiHandle itr = npi_iterate(npiVhGeneric, hdl);
  int objCnt = 0;
  int objNum = npi_get(npiSize, itr);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_generic(fp, subHdl, indent+1, ++objCnt, objNum);
    npi_release_handle(subHdl);
  }
  // gen port
  itr = npi_iterate(npiPort, hdl);
  objCnt = 0;
  objNum = npi_get(npiSize, itr);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_port(fp, subHdl, indent+1, ++objCnt, objNum);
    npi_release_handle(subHdl);
  }
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end component;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_attr_decl(FILE* fp, npiHandle hdl, int indent)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "attribute %s: ", npi_get_str(npiName, hdl));
  npiHandle typeHdl = npi_handle(npiVhType, hdl);
  fprintf(fp, "%s;\n", get_vh_type(typeHdl));
  npi_release_handle(typeHdl);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_attr_spec(FILE* fp, npiHandle hdl, int indent)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "%s;\n", KDB_TODO_MSG);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_use_clause(FILE* fp, npiHandle hdl, int indent)
{
  // TODO
  assert(0);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_subp_decl(FILE* fp, npiHandle hdl, int indent, bool isForBody)
{
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  int type = npi_get(npiType, hdl);
  switch (type) {
    case npiVhFunction:
      assert(isForBody);
    case npiVhFuncDecl:
      fprintf(fp, "function ");
      break;
    case npiVhProcedure:
      assert(isForBody);
    case npiVhProcDecl:
      fprintf(fp, "procedure ");
      break;
    default:
      assert(0);
      return false;
  }
  fprintf(fp, "%s(", npi_get_str(npiName, hdl));
  npiHandle paramItr = npi_iterate(npiVhParam, hdl);
  npiHandle param = NULL;
  npiHandle initHdl = NULL;
  int paramCnt = 0;
  while ((param = npi_scan(paramItr))) {
    if (paramCnt++ > 0)
      fprintf(fp, "; ");
    switch (npi_get(npiType, param)) {
      case npiVhSigParam:
        fprintf(fp, "signal ");
        break;
      case npiVhVarParam:
        fprintf(fp, "variable ");
        break;
      case npiVhConstParam:
        fprintf(fp, "constant ");
        break;
      case npiNIY:
        fprintf(fp, "%s ", KDB_TODO_MSG);
        break;
      default:
        assert(0);
        return false;
    }
    fprintf(fp, "%s: ", npi_get_str(npiName, param));
    switch (npi_get(npiDirection, param)) {
      case npiInput:
        fprintf(fp, "in ");
        break;
      case npiOutput:
        fprintf(fp, "out ");
        break;
      case npiInout:
        fprintf(fp, "inout ");
        break;
      case npiVhBuffer:
        fprintf(fp, "buffer ");
        break;
      case npiVhLinkage:
        fprintf(fp, "linkage ");
        break;
      case npiUndefined:
        break;
      default:
        assert(0);
        return false;
    }
    // type
    npiHandle typeHdl = npi_handle(npiVhType, param);
    if (typeHdl) {
      fprintf(fp, "%s", get_vh_type(typeHdl));
      npi_release_handle(typeHdl);
    }
    // init expr
    initHdl = npi_handle(npiVhInitExpr, param);
    if (initHdl) {
      fprintf(fp, " := %s", get_vh_expr(initHdl));
      npi_release_handle(initHdl);
    }
    npi_release_handle(param);
  }
  fprintf(fp, ")");
  // ending for func return
  if (type == npiVhFuncDecl) {
    npiHandle returnType = npi_handle(npiVhReturnType, hdl);
    assert(returnType);
    fprintf(fp, " return %s", get_vh_type(returnType));
  }
  fprintf(fp, isForBody? " is\n":";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_subp(FILE* fp, npiHandle hdl, int indent)
{
  npiHandle subDeclHdl = npi_handle(npiVhSubpDecl, hdl);
  assert(subDeclHdl);
  gen_vh_subp_decl(fp, subDeclHdl, indent, true/*forBody*/);
  npi_release_handle(subDeclHdl);
  int subIndent = indent + 1;
  int i = 0;
  // decl
  npiHandle decl = NULL;
  npiHandle itr = npi_iterate(npiVhDecl, hdl);
  while ((decl = npi_scan(itr))) {
    gen_vh_decl(fp, decl, subIndent);
    npi_release_handle(decl);
  }
  // begin
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "begin\n");
  // stmt
  itr = npi_iterate(npiVhStmt, hdl);
  npiHandle stmt = NULL;
  while ((stmt = npi_scan(itr))) {
    gen_vh_stmt(fp, stmt, subIndent);
    npi_release_handle(stmt);
  }
  // end
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_stmt(FILE* fp, npiHandle hdl, int indent)
{
  int hdlType = npi_get(npiType, hdl);
  // special treatment for generate
  if (hdlType == npiVhGenerate) {
    gen_vh_generate(fp, hdl, indent);
    return true;
  }

  // general flow
  bool isToDoStmt = false;
  int i;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");

  switch (hdlType) {
    case npiVhComponent:
      gen_vh_component(fp, hdl, indent);
      break;
    case npiVhBlock:
      gen_vh_block(fp, hdl, indent);
      break;
    case npiLangInterface:
      if (!gen_lang_interface(fp, hdl, indent))
        isToDoStmt = true;
      break;
    case npiVhSimpleSigAssign:
    case npiVhCondSigAssign:
    case npiVhSelectSigAssign:
    case npiVhSeqSigAssign:
      gen_vh_sig_assign(fp, hdl, indent);
      break;
    case npiVhConcProcCall:
    case npiVhSeqProcCall:
      gen_vh_proc_call(fp, hdl, indent);
      break;
    case npiVhConcAssert:
    case npiVhSeqAssert:
      gen_vh_assert(fp, hdl, indent);
      break;
    case npiVhProcess:
      gen_vh_process(fp, hdl, indent);
      break;
    case npiVhForLoop:
    case npiVhWhileLoop:
    case npiVhForeverLoop:
      gen_vh_loop(fp, hdl, indent);
      break;
    case npiVhVarAssign:
      gen_vh_var_assign(fp, hdl, indent);
      break;
    case npiVhReport:
      gen_vh_report(fp, hdl, indent);
      break;
    case npiVhWait:
      gen_vh_wait(fp, hdl, indent);
      break;
    case npiVhIf:
      gen_vh_if(fp, hdl, indent);
      break;
    case npiVhCase:
      gen_vh_case(fp, hdl, indent);
      break;
    case npiVhReturn:
      gen_vh_return(fp, hdl, indent);
      break;
    case npiVhExit:
      gen_vh_exit(fp, hdl, indent);
      break;
    case npiVhNext:
      gen_vh_next(fp, hdl, indent);
      break;
    case npiVhNull:
      gen_vh_null(fp, hdl, indent);
      break;
    default:
      printf("ASSERTION TYPE: %s\n", npi_get_str(npiType, hdl));
      assert(0);
      return false;
  }

  // -- TODO stmt -------------------------------------------------------------
  if (isToDoStmt) {
    fprintf(fp, "TODO_STMT;\n");
    return false;
  }
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_block(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "block");
  // -- gen guard expr ---------------------------------------------------------
  npiHandle subHdl = npi_handle(npiVhGuardExpr, hdl);
  if (subHdl) {
    fprintf(fp, "%s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, "\n");
  // -- gen header -------------------------------------------------------------
  gen_vh_header(fp, hdl, indent, true/*isGenMap*/);
  // -- gen decl ---------------------------------------------------------------
  gen_vh_declarative_part(fp, hdl, indent);
  // -- gen stmt ---------------------------------------------------------------
  int i;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "begin\n");
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end block;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_generate(FILE* fp, npiHandle hdl, int indent)
{
  static string prevLabelName = "";
  string labelName = npi_get_str(npiVhLabelName, hdl);
  // -- only gen for the first generate element --------------------------------
  if (prevLabelName.compare(labelName)==0)
    return true;
  prevLabelName.assign(labelName);
  const char* fileName = npi_get_str(npiFile, hdl);
  npiTextHandle fileHdl = npi_text_file_by_name(fileName);
  assert(fileHdl);
  int lineNo = npi_get(npiLineNo, hdl);
  npiTextHandle generateTxtHdl = npi_text_line_by_number(fileHdl, lineNo);
  string textStr = npi_text_property_str(npiTextLineContent, generateTxtHdl);
  // -- gen condition ----------------------------------------------------------
  string name = npi_get_str(npiName, hdl);
  // if gen
  int bgnPos = -1;
  if (name.rfind(')')+1 != name.length()) {
    bgnPos = textStr.find(" if ");
    if (bgnPos == -1)
      bgnPos = textStr.find(":if ");
  }
  // for gen
  else {
    bgnPos = textStr.find(" for ");
    if (bgnPos == -1)
      bgnPos = textStr.find(":for ");
  }
  assert(bgnPos > 0);
  int endPos = textStr.rfind(" generate");
  if (endPos == -1)
    endPos = textStr.length()-1;
  string condStr = textStr.substr(bgnPos+1, endPos-bgnPos-1);
  // -- gen stmt ---------------------------------------------------------------
  int i;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "%s: %s generate\n", labelName.data(), condStr.data());
  int subAddIndent = labelName.length() + 2;
  string subAddIndentStr = "";
  for (i=0; i<subAddIndent; i++)
    subAddIndentStr.append(" ");
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  npiHandle stmt = NULL;
  while ((stmt = npi_scan(itr))) {
    fprintf(fp, "%s", subAddIndentStr.data());
    gen_vh_stmt(fp, stmt, indent+1);
    npi_release_handle(stmt);
  }
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end generate;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_var_assign(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  npiHandle hsHdl = npi_handle(npiVhLhs, hdl);
  fprintf(fp, "%s := ", get_vh_expr(hsHdl));
  npi_release_handle(hsHdl);
  hsHdl = npi_handle(npiVhRhs, hdl);
  fprintf(fp, "%s;\n", get_vh_expr(hsHdl));
  npi_release_handle(hsHdl);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_sig_assign(FILE* fp, npiHandle hdl, int indent)
{
  string nextIndentStr = "";
  int i = 0;

  // indent and label name
  const char* name = npi_get_str(npiVhLabelName, hdl);
  int labelLen = 0;
  int hdlType = npi_get(npiType, hdl);
  if (name) {
    fprintf(fp, "%s: ", name);
    if (hdlType == npiVhCondSigAssign)
      labelLen = strlen(name) + 2;
  }
  // TODO: postponed property (for concurrent only)
  for (i=0; i<indent; i++)
    nextIndentStr.append("  ");
  // select expr
  if (hdlType == npiVhSelectSigAssign) {
    npiHandle selHdl = npi_handle(npiVhSelectExpr, hdl);
    fprintf(fp, " with %s select\n", get_vh_expr(selHdl));
    npi_release_handle(selHdl);
    nextIndentStr.append("  "); // add indent 
    fprintf(fp, "%s", nextIndentStr.data());
  }
  // lhs
  npiHandle rlsHdl = npi_handle(npiVhLhs, hdl);
  const char* lhsStr = get_vh_expr(rlsHdl);
  fprintf(fp, "%s <= %s", lhsStr, npi_get(npiVhGuarded, hdl)==1?"guarded ":"");
  int lhsLen = strlen(lhsStr) + labelLen;
  for (i=0; i<lhsLen; i++)
    nextIndentStr.append(" ");
  nextIndentStr.append("    ");
  npi_release_handle(rlsHdl);
  // rhs waveform
  npiHandle itr = npi_iterate(npiVhWaveform, hdl);
  int wvCnt = 0;
  while ((rlsHdl = npi_scan(itr))) {
    if (wvCnt++ > 0) {
      switch (hdlType) {
        case npiVhCondSigAssign:
          fprintf(fp, " else\n");
          break;
        case npiVhSelectSigAssign:
          fprintf(fp, ",\n");
          break;
        default:
          assert(0);
          break;
      }
      fprintf(fp, "%s", nextIndentStr.data());
    }
    fprintf(fp, "%s", get_vh_waveform(rlsHdl));
    npi_release_handle(rlsHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_proc_call(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  // TODO: postponed property (for concurrent only)
  fprintf(fp, "%s(", npi_get_str(npiName, hdl));
  npiHandle mapElem = NULL;
  int loopCnt = 0;
  npiHandle itr = npi_iterate(npiVhMapElem, hdl);
  while ((mapElem = npi_scan(itr))) {
    if (loopCnt++ > 0)
      fprintf(fp, ", ");
    fprintf(fp, "%s", get_vh_map_elem(mapElem));
    npi_release_handle(mapElem);
  }
  fprintf(fp, ");\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_assert(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  // TODO: postponed property
  fprintf(fp, "assert");
  npiHandle exprHdl = npi_handle(npiVhCondExpr, hdl);
  if (exprHdl) {
    fprintf(fp, " %s", get_vh_expr(exprHdl));
    npi_release_handle(exprHdl);
  }
  fprintf(fp, "\n");
  // -- gen stmt ---------------------------------------------------------------
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  if (!itr) {
    // TODO: refine here after KDB fixes assert stmt bug
    int i = 0;
    for (i=0; i<=indent; i++)
      fprintf(fp, "  ");
    fprintf(fp, "report \"%s\"\n", KDB_TODO_MSG);
    for (i=0; i<=indent; i++)
      fprintf(fp, "  ");
    fprintf(fp, "severity NOTE;\n");
    return true;
  }
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_report(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);

  fprintf(fp, "report");
  // -- gen stmt ---------------------------------------------------------------
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  if (!itr) {
    // TODO: refine here after KDB fixes assert stmt bug
    int i = 0;
    for (i=0; i<=indent; i++)
      fprintf(fp, "  ");
    fprintf(fp, " \"%s\"\n", KDB_TODO_MSG);
    for (i=0; i<=indent; i++)
      fprintf(fp, "  ");
    fprintf(fp, "severity NOTE;\n");
    return true;
  }
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_wait(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);

  fprintf(fp, "wait");
  // sensitivities
  int loopCnt = 0;
  npiHandle subHdl = NULL;
  npiHandle itr = npi_iterate(npiVhSensitivities, hdl);
  while ((subHdl = npi_scan(itr))) {
    if (loopCnt++ > 0)
      fprintf(fp, ", ");
    else
      fprintf(fp, " on ");
    fprintf(fp, "%s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  // condition
  subHdl = npi_handle(npiVhCondExpr, hdl);
  if (subHdl) {
    fprintf(fp, " until %s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  // time
  subHdl = npi_handle(npiVhTimeExpr, hdl);
  if (subHdl) {
    fprintf(fp, " for %s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_loop(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  npiHandle subHdl = NULL;
  switch (npi_get(npiType, hdl)) {
    case npiVhForLoop: {
      fprintf(fp, "for ");
      subHdl = npi_handle(npiVhParam, hdl);
      assert(subHdl);
      if (subHdl) {
        fprintf(fp, "%s ", npi_get_str(npiName, subHdl));
        npi_release_handle(subHdl);
      }
      fprintf(fp, "in ");
      subHdl = npi_handle(npiVhConstraint, hdl);
      fprintf(fp, "%s ", get_vh_constraint(subHdl, EMPTY_CNSTR_TYPE/*cnstrType*/));
      npi_release_handle(subHdl);
      break;
    }
    case npiVhWhileLoop:
      fprintf(fp, "while ");
      subHdl = npi_handle(npiVhCondExpr, hdl);
      if (subHdl) {
        fprintf(fp, "%s ", get_vh_expr(subHdl));
        npi_release_handle(subHdl);
      }
      break;
    case npiVhForeverLoop:
      break;
    default:
      assert(0);
      break;
  }
  fprintf(fp, "loop\n");
  npiHandle itr = npi_iterate(npiVhStmt, hdl);
  while ((subHdl = npi_scan(itr))) {
    gen_vh_stmt(fp, subHdl, indent+1);
    npi_release_handle(subHdl);
  }
  int i;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end loop;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_if(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "if ");
  npiHandle itr = npi_iterate(npiVhBranch, hdl);
  assert(itr);
  npiHandle branchHdl = NULL;
  npiHandle subHdl = NULL;
  npiHandle subItr = NULL;
  int branchCnt = 0;
  int condCnt = 0;
  string indentStr = "";
  int i;
  for (i=0; i<indent; i++)
    indentStr.append("  ");
  // get branch
  while ((branchHdl = npi_scan(itr))) {
    if (branchCnt++ > 0)
      fprintf(fp, "%s", indentStr.data());
    // condition expr
    condCnt = 0;
    switch (npi_get(npiVhBranchType, branchHdl)) {
      case npiVhIfBranch:
        break;
      case npiVhElsifBranch:
        fprintf(fp, "elsif ");
        break;
      case npiVhSimpleBranch:
        fprintf(fp, "else\n");
        break;
      default:
        assert(0);
        break;
    }
    subItr = npi_iterate(npiVhCondExpr, branchHdl);
    while ((subHdl = npi_scan(subItr))) {
      assert(condCnt++ <= 0);
      fprintf(fp, "%s then\n", get_vh_expr(subHdl));
      npi_release_handle(subHdl);
    }
    // stmt
    subItr = npi_iterate(npiVhStmt, branchHdl);
    while ((subHdl = npi_scan(subItr))) {
      gen_vh_stmt(fp, subHdl, indent+1);
      npi_release_handle(subHdl);
    }
    npi_release_handle(branchHdl);
  }
  // end
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end if;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_case(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "case ");
  npiHandle subHdl = npi_handle(npiVhCaseExpr, hdl);
  if (subHdl) {
    fprintf(fp, "%s ", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, "is\n");
  int condCnt = 0;
  string indentStr = "";
  int i;
  for (i=0; i<=indent; i++)
    indentStr.append("  ");
  int stmtIndent = indent + 2;
  npiHandle branchHdl = NULL;
  npiHandle subItr = NULL;
  npiHandle itr = npi_iterate(npiVhBranch, hdl);
  assert(itr);
  while ((branchHdl = npi_scan(itr))) {
    fprintf(fp, "%s", indentStr.data());
    // condition expr
    switch (npi_get(npiVhBranchType, branchHdl)) {
      case npiVhWhenChoicesBranch:
        fprintf(fp, "when ");
        condCnt = 0;
        subItr = npi_iterate(npiVhCondExpr, branchHdl);
        while ((subHdl = npi_scan(subItr))) {
          if (condCnt++ > 0)
            fprintf(fp, " | ");
          if (npi_get(npiType, subHdl) == npiVhRange)
            fprintf(fp, "%s", get_vh_constraint(subHdl, EMPTY_CNSTR_TYPE/*cnstrType*/));
          else
            fprintf(fp, "%s", get_vh_expr(subHdl));
          npi_release_handle(subHdl);
        }
        break;
      case npiVhWhenOthersBranch:
        fprintf(fp, "when others");
        break;
      default:
        assert(0);
        break;
    }
    // stmt
    fprintf(fp, " =>\n");
    subItr = npi_iterate(npiVhStmt, branchHdl);
    while ((subHdl = npi_scan(subItr))) {
      gen_vh_stmt(fp, subHdl, stmtIndent);
      npi_release_handle(subHdl);
    }
    npi_release_handle(branchHdl);
  }
  // end
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "end case;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_return(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "return");
  npiHandle subHdl = npi_handle(npiVhReturnExpr, hdl);
  if (subHdl) {
    fprintf(fp, " %s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_exit(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "exit");
  name = npi_get_str(npiVhLoopLabelName, hdl);
  if (name)
    fprintf(fp, " %s", name);
  npiHandle subHdl = npi_handle(npiVhCondExpr, hdl);
  if (subHdl) {
    fprintf(fp, " when %s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_next(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "next");
  name = npi_get_str(npiVhLoopLabelName, hdl);
  if (name)
    fprintf(fp, " %s", name);
  npiHandle subHdl = npi_handle(npiVhCondExpr, hdl);
  if (subHdl) {
    fprintf(fp, " when %s", get_vh_expr(subHdl));
    npi_release_handle(subHdl);
  }
  fprintf(fp, ";\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_null(FILE* fp, npiHandle hdl, int indent)
{
  const char* name = npi_get_str(npiVhLabelName, hdl);
  if (name)
    fprintf(fp, "%s: ", name);
  fprintf(fp, "null;\n");
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::gen_vh_type(FILE* fp, npiHandle hdl, int indent)
{
  int hdlType = npi_get(npiType, hdl);
  int i = 0;
  for (i=0; i<indent; i++)
    fprintf(fp, "  ");
  fprintf(fp, "%stype ", (hdlType==npiVhSubTypeDecl)?"sub":"");
  npiHandle itr = NULL;
  npiHandle subHdl = NULL;
  npiHandle ooHdl = NULL;
  int subIndent = indent + 1;
  int subHdlCnt = 0;
  int itrSize = 0;
  switch (hdlType) {
    case npiVhEnumTypeDecl:
      fprintf(fp, "%s is (", npi_get_str(npiName, hdl));
      subHdlCnt = 0;
      itr = npi_iterate(npiVhEnumLiteral, hdl);
      while ((subHdl = npi_scan(itr))) {
        fprintf(fp, "%s%s", (subHdlCnt++ > 0)? ", ":"", npi_get_str(npiName, subHdl));
        npi_release_handle(subHdl);
      }
      fprintf(fp, ");\n");
      break;
    case npiVhPhysTypeDecl:
      fprintf(fp, "%s is ", npi_get_str(npiName, hdl));
      subHdl = npi_handle(npiVhConstraint, hdl);
      fprintf(fp, "%s\n", get_vh_constraint(subHdl, RANGE_CNSTR_TYPE/*cnstrType*/));
      npi_release_handle(subHdl);
      for (i=0; i<subIndent; i++)
        fprintf(fp, "  ");
      fprintf(fp, "units\n");
      itr = npi_iterate(npiVhPhysUnit, hdl);
      // [ work-around due to KDB bug
      if (!itr) {
        for (i=0; i<=subIndent; i++)
          fprintf(fp, "  ");
        fprintf(fp, "%s;\n", KDB_TODO_MSG);
      } // ]
      while ((subHdl = npi_scan(itr))) {
        for (i=0; i<=subIndent; i++)
          fprintf(fp, "  ");
        fprintf(fp, "%s", npi_get_str(npiName, subHdl));
        ooHdl = npi_handle(npiVhPhysLiteral, subHdl);
        if (ooHdl)
          fprintf(fp, " = %s", get_vh_expr(ooHdl));
        fprintf(fp, ";\n");
        npi_release_handle(ooHdl);
        npi_release_handle(subHdl);
      }
      for (i=0; i<subIndent; i++)
        fprintf(fp, "  ");
      fprintf(fp, "end units;\n");
      break;
    case npiVhIntTypeDecl:
    case npiVhFloatTypeDecl:
      fprintf(fp, "%s is ", npi_get_str(npiName, hdl));
      subHdl = npi_handle(npiVhConstraint, hdl);
      fprintf(fp, "%s;\n", get_vh_constraint(subHdl, RANGE_CNSTR_TYPE/*cnstrType*/));
      npi_release_handle(subHdl);
      break;
    case npiVhSubTypeDecl: {
      fprintf(fp, "%s is ", npi_get_str(npiName, hdl));
      ooHdl = npi_handle(npiVhType, hdl);
      fprintf(fp, "%s ", npi_get_str(npiName, ooHdl));
      bool isFromArray = (npi_get(npiType, ooHdl)==npiVhArrayTypeDecl)?true:false;
      npi_release_handle(ooHdl);
      itr = npi_iterate(npiVhConstraint, hdl);
      itrSize = npi_get(npiSize, itr);
      if (itrSize == 1) {
        subHdl = npi_scan(itr);
        fprintf(fp, "%s;\n", get_vh_constraint(subHdl, isFromArray?PRTHS_CNSTR_TYPE:RANGE_CNSTR_TYPE/*cnstrType*/));
        npi_release_handle(subHdl);
        break;
      }
      if (itrSize > 0)
        fprintf(fp, "(");
      subHdlCnt = 0;
      while ((subHdl = npi_scan(itr))) {
        if (subHdlCnt++ > 0)
          fprintf(fp, ", ");
        fprintf(fp, "%s", get_vh_constraint(subHdl, EMPTY_CNSTR_TYPE/*cnstrType*/));
        npi_release_handle(subHdl);
      }
      if (itrSize > 0)
        fprintf(fp, ")");
      fprintf(fp, ";\n");
      break;
    }
    case npiVhArrayTypeDecl:
      fprintf(fp, "%s is array", npi_get_str(npiName, hdl));
      subHdlCnt = 0;
      itr = npi_iterate(npiVhConstraint, hdl);
      itrSize = npi_get(npiSize, itr);
      if (itrSize > 1)
        fprintf(fp, "(");
      while ((subHdl = npi_scan(itr))) {
        if (subHdlCnt++ > 0)
          fprintf(fp, ", ");
        fprintf(fp, "%s", get_vh_constraint(subHdl, itrSize>1?EMPTY_CNSTR_TYPE:PRTHS_CNSTR_TYPE/*cnstrType*/));
        npi_release_handle(subHdl);
      }
      if (itrSize > 1)
        fprintf(fp, ")");
      ooHdl = npi_handle(npiVhElemType, hdl);
      assert(subHdlCnt == npi_get(npiVhNumDims, hdl) && ooHdl);
      fprintf(fp, " of %s;\n", npi_get_str(npiName, ooHdl));
      npi_release_handle(ooHdl);
      break;
    case npiVhRecordTypeDecl: {
      fprintf(fp, "%s is\n", npi_get_str(npiName, hdl));
      subHdlCnt = 0;
      for (i=0; i<subIndent; i++)
        fprintf(fp, "  ");
      fprintf(fp, "record\n");
      itr = npi_iterate(npiVhElemDecl, hdl);
      assert(npi_get(npiSize, itr) == npi_get(npiVhNumFields, hdl));
      while ((subHdl = npi_scan(itr))) {
        for (i=0; i<=subIndent; i++)
          fprintf(fp, "  ");
        fprintf(fp, "%s: ", npi_get_str(npiName, subHdl));
        // type
        npiHandle typeHdl = npi_handle(npiVhType, subHdl);
        if (typeHdl) {
          fprintf(fp, "%s", get_vh_type(typeHdl, RANGE_CNSTR_TYPE));
          npi_release_handle(typeHdl);
        }
        fprintf(fp, ";\n");
        npi_release_handle(subHdl);
      }
      for (i=0; i<subIndent; i++)
        fprintf(fp, "  ");
      fprintf(fp, "end record;\n");
      break;
    }
    default:
      assert(0);
      return false;
  }
  return true;
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_constraint(npiHandle hdl, CNSTR_TYPE_E cnstrType, bool isRecursive)
{
  static string cnstStr = "";
  if (!isRecursive)
    cnstStr.assign("");
  npiHandle lrRng = NULL;
  npiHandle tpHdl = NULL;
  npiHandle itr = NULL;
  int subHdlCnt = 0;
  if (!isRecursive && cnstrType == PRTHS_CNSTR_TYPE)
    cnstStr.append("(");
  switch (npi_get(npiType, hdl)) {
    case npiVhRange:
      lrRng = npi_handle(npiVhLeftRange, hdl);
      assert(lrRng);
      switch (cnstrType) {
        case RANGE_CNSTR_TYPE:
          cnstStr.append("range ");
          break;
        case PRTHS_CNSTR_TYPE:
        case EMPTY_CNSTR_TYPE:
        default:
          break;
      }
      cnstStr.append(get_vh_expr(lrRng));
      npi_release_handle(lrRng);
      if (npi_get(npiVhDirType, hdl) == npiVhDowntoDir)
        cnstStr.append(" downto ");
      else
        cnstStr.append(" to ");
      lrRng = npi_handle(npiVhRightRange, hdl);
      assert(lrRng);
      cnstStr.append(get_vh_expr(lrRng));
      npi_release_handle(lrRng);
      break;
    case npiVhSubTypeDecl:
      tpHdl = npi_handle(npiVhType, hdl);
      // [TODO] Review this segment after KDB fixes type/subtype bug
      if (!tpHdl)
        break;
      cnstStr.append(npi_get_str(npiName, tpHdl));
      cnstStr.append(" ");
      npi_release_handle(tpHdl);
      itr = npi_iterate(npiVhConstraint, hdl);
      if (!itr) {
        cnstStr.append("range <>");
        break;
      }
      if (npi_get(npiSize, itr) == 1) {
        lrRng = npi_scan(itr);
        // [TODO] review
        // recursive call, no need to append to str
        get_vh_constraint(lrRng, RANGE_CNSTR_TYPE, true/*isRecursive*/);
        npi_release_handle(lrRng);
        break;
      }
      subHdlCnt = 0;
      while ((lrRng = npi_scan(itr))) {
        if (subHdlCnt++ > 0)
          cnstStr.append(", ");
        // [TODO] review
        // recursive call, no need to append to str
        get_vh_constraint(lrRng, cnstrType, true/*isRecursive*/);
        npi_release_handle(lrRng);
      }
      break;
    case npiVhParamAttrName:
    case npiVhEnumTypeDecl:
    case npiVhPhysTypeDecl:
    case npiVhIntTypeDecl:
    case npiVhFloatTypeDecl:
    case npiVhArrayTypeDecl:
    case npiVhRecordTypeDecl:
      cnstStr.append(npi_get_str(npiName, hdl));
      break;
    case npiNIY:
      cnstStr.append(KDB_TODO_MSG);
      break;
    default:
      printf("ASSERTION TYPE: %s\n", npi_get_str(npiType, hdl));
      assert(0);
      break;
  }
  if (!isRecursive && cnstrType == PRTHS_CNSTR_TYPE)
    cnstStr.append(")");
  return cnstStr.data();
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_type(npiHandle hdl, CNSTR_TYPE_E cnstrType/*constraint type*/)
{
  static string typeStr = "";
  typeStr.assign("");
  const char* typeName = npi_get_str(npiName, hdl);
  // type
  if (typeName) {
    typeStr.append(typeName);
    npi_release_handle(hdl);
    return typeStr.data();
  }
  // type check (should be subtype)
  switch (npi_get(npiType, hdl)) {
    case npiVhSubTypeDecl:
      break;
    case npiNIY:
      typeStr.assign(KDB_TODO_MSG);
      return typeStr.data();
    default:
      assert(0);
      break;
  }
  npiHandle stHdl = hdl;
  npiHandle typeHdl = npi_handle(npiVhType, stHdl);
  typeStr.append(npi_get_str(npiName, typeHdl));
  npiHandle cnstHdl = NULL;
  npiHandle cnstItr = npi_iterate(npiVhConstraint, stHdl);
  while ((cnstHdl = npi_scan(cnstItr))) {
    typeStr.append(" ");
    typeStr.append(get_vh_constraint(cnstHdl, cnstrType/*cnstrType*/));
    npi_release_handle(cnstHdl);
  }
  npi_release_handle(stHdl);
  npi_release_handle(typeHdl);
  return typeStr.data();
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_expr(npiHandle hdl, bool isRecursive)
{
  if (!hdl)
    return NULL;

  npiValue val;
  const char* name = NULL;
  char* strBuf = get_str_buffer();
  static string exprStr = "";
  if (!isRecursive)
    exprStr.assign("");
  switch (npi_get(npiType, hdl)) {
    // name
    case npiVhSig:
    case npiVhVar:
    case npiVhConst:
    case npiVhGeneric:
    case npiPort:
    case npiVhParam:
    case npiVhIndexedName:
    case npiVhSliceName:
    //case npiVhSelectedName: // TODO
    case npiVhSimpAttrName:
    case npiVhParamAttrName:
    case npiVhUserAttrName:
    case npiVhConstParam:
    case npiVhVarParam:
    case npiVhSigParam:
    case npiVhIndexVar:
      name = npi_get_str(npiName, hdl);
      if (name) {
        sprintf(strBuf, "%s", name);
        exprStr.append(strBuf);
      }
      break;
    // literal
    case npiVhBitStrLiteral:
      switch (npi_get(npiConstType, hdl)) {
        case npiBinaryConst:
          val.format = npiBinStrVal;
          exprStr.append("b\"");
          break;
        case npiOctConst:
          val.format = npiOctStrVal;
          exprStr.append("o\"");
          break;
        case npiDecConst:
          val.format = npiDecStrVal;
          exprStr.append("d\"");
          break;
        case npiHexConst:
          val.format = npiHexStrVal;
          exprStr.append("x\"");
          break;
        case npiStringConst:
        default:
          val.format = npiStringVal;
          exprStr.append("\"");
          break;
      }
      npi_get_value(hdl, val/*IO*/);
      sprintf(strBuf, "%s\"", val.value.str);
      exprStr.append(strBuf);
      break;
    case npiVhRealLiteral:
      val.format = npiRealVal;
      npi_get_value(hdl, val/*IO*/);
      sprintf(strBuf, "%f", val.value.real);
      exprStr.append(strBuf);
      break;
    case npiVhIntLiteral:
      val.format = npiIntVal;
      npi_get_value(hdl, val/*IO*/);
      sprintf(strBuf, "%d", val.value.integer);
      exprStr.append(strBuf);
      break;
    case npiVhPhysLiteral: {
      npiHandle subLit = npi_handle(npiVhAbstractLiteral, hdl);
      if (!subLit) {
        assert(0);
        break;
      }
      // recursive call, no need to append to str
      get_vh_expr(subLit, isRecursive);
      npi_release_handle(subLit);
      subLit = npi_handle(npiVhPhysUnit, hdl);
      exprStr.append(" ");
      exprStr.append(npi_get_str(npiName, subLit));
      npi_release_handle(subLit);
      break;
    }
    case npiVhNullLiteral:
      assert(0); // check/remove this after null-literal is supported
    case npiVhStringLiteral:
      val.format = npiStringVal;
      if (npi_get_value(hdl, val/*IO*/))
        sprintf(strBuf, "\"%s\"", val.value.str);
      else
        sprintf(strBuf, "\"\"");
      exprStr.append(strBuf);
      break;
    case npiVhEnumLiteral:
    case npiVhCharLiteral:
      exprStr.append(npi_get_str(npiName, hdl));
      break;
    case npiVhFuncCall: {
      exprStr.append(npi_get_str(npiName, hdl));
      exprStr.append("(");
      npiHandle mapElem = NULL;
      int loopCnt = 0;
      npiHandle itr = npi_iterate(npiVhMapElem, hdl);
      while ((mapElem = npi_scan(itr))) {
        if (loopCnt++ > 0)
          exprStr.append(", ");
        // [ don't call get_vh_map_elem to prevent inevitable recursive issue
        npiHandle faHdl = NULL;
        faHdl = npi_handle(npiVhFormal, mapElem);
        get_vh_expr(faHdl, true/*isRecursive*/);
        exprStr.append(" => ");
        npi_release_handle(faHdl);
        faHdl = npi_handle(npiVhActual, mapElem);
        get_vh_expr(faHdl, true/*isRecursive*/);
        npi_release_handle(faHdl);
        npi_release_handle(mapElem); // ]
      }
      exprStr.append(")");
      break;
    }
    // operation
    case npiVhOperation: {
      const char* oprSymbol = get_vh_opr_symbol(hdl);
      npiHandle oprHdl = NULL;
      npiHandle itr = npi_iterate(npiVhOperand, hdl);
      assert(itr);
      int oprNum = npi_get(npiSize, itr);
      int oprCnt = 0;
      exprStr.append("(");
      while ((oprHdl = npi_scan(itr))) {
        if (++oprCnt == oprNum) {
          sprintf(strBuf, "%s%s ", oprNum>1?" ":"", oprSymbol);
          exprStr.append(strBuf);
        }
        // recursive call, no need to append to str
        get_vh_expr(oprHdl, true/*isRecursive*/);
        npi_release_handle(oprHdl);
      }
      exprStr.append(")");
      break;
    }
    // aggregate
    case npiVhAggregate: {
      npiHandle elemAssoc = NULL;
      npiHandle chItr = NULL;
      npiHandle subHdl = NULL;
      int cnt = 0;
      int chCnt = 0;
      exprStr.append("(");
      npiHandle itr = npi_iterate(npiVhElemAssoc, hdl);
      while ((elemAssoc = npi_scan(itr))) {
        if (cnt++ > 0)
          exprStr.append(", ");
        // named association
        if (npi_get(npiVhIsNamed, elemAssoc) == 1) {
          chCnt = 0;
          chItr = npi_iterate(npiVhChoices, elemAssoc);
          while ((subHdl = npi_scan(chItr))) {
            if (chCnt++ > 0)
              exprStr.append(" | ");
            switch (npi_get(npiType, subHdl)) {
              case npiVhConstraint:
                exprStr.append(get_vh_constraint(subHdl, EMPTY_CNSTR_TYPE/*cnstrType*/));
                break;
              case npiVhOthers:
                exprStr.append("others");
                break;
              default:
                get_vh_expr(subHdl, true/*isRecursive*/);
                break;
            }
            npi_release_handle(subHdl);
          }
          exprStr.append(" => ");
        }
        // expr
        subHdl = npi_handle(npiVhExpr, elemAssoc);
        assert(subHdl);
        get_vh_expr(subHdl, true/*isRecursive*/);
        npi_release_handle(subHdl);
        npi_release_handle(elemAssoc);
      }
      exprStr.append(")");
      break;
    }
    // type conversion
    case npiVhTypeConv: {
      npiHandle subHdl = npi_handle(npiVhType, hdl);
      exprStr.append(get_vh_type(subHdl));
      npi_release_handle(subHdl);
      exprStr.append("(");
      subHdl = npi_handle(npiVhExpr, hdl);
      get_vh_expr(subHdl, true/*isRecursive*/);
      exprStr.append(")");
      break;
    }
    case npiVhQualifiedExpr: {
      npiHandle subHdl = npi_handle(npiVhType, hdl);
      exprStr.append(get_vh_type(subHdl));
      npi_release_handle(subHdl);
      exprStr.append("'(");
      subHdl = npi_handle(npiVhExpr, hdl);
      get_vh_expr(subHdl, true/*isRecursive*/);
      exprStr.append(")");
      break;
    }
    case npiNIY:
      //exprStr.append("NIY_OBJ");
      // [ tmp workaround: avoid parsing error
      exprStr.append("999999999"); // ]
      break;
    // [ for map-element of language interface
    case npiParameter:
      exprStr.append(npi_get_str(npiName, hdl));
      break;
    // ]
    default:
      printf("ASSERTION ASSERT: TYPE: %s at %d in %s\n", npi_get_str(npiType, hdl), npi_get(npiLineNo, hdl), npi_get_str(npiFile, hdl));
      assert(0);
      exprStr.append("");
      break;
  }
  return exprStr.data();
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_opr_symbol(npiHandle hdl)
{
  if (!hdl)
    return NULL;

  switch(npi_get(npiVhOpType, hdl)) {
    case npiVhAndOp:
      return "and";
    case npiVhOrOp:
      return "or";
    case npiVhNandOp:
      return "nand";
    case npiVhNorOp:
      return "nor";
    case npiVhXorOp:
      return "xor";
    case npiVhXnorOp:
      return "xnor";
    case npiVhEqOp:
      return "=";
    case npiVhNeqOp:
      return "/=";
    case npiVhLtOp:
      return "<";
    case npiVhLeOp:
      return "<=";
    case npiVhGtOp:
      return ">";
    case npiVhGeOp:
      return ">=";
    case npiVhSllOp:
      return "sll";
    case npiVhSrlOp:
      return "srl";
    case npiVhSlaOp:
      return "sla";
    case npiVhSraOp:
      return "sra";
    case npiVhRolOp:
      return "rol";
    case npiVhRorOp:
      return "ror";
    case npiVhAddOp:
      return "+";
    case npiVhSubOp:
      return "-";
    case npiVhConcatOp:
      return "&";
    case npiVhPlusOp:
      return "+";
    case npiVhMinusOp:
      return "-";
    case npiVhMultOp:
      return "*";
    case npiVhDivOp:
      return "/";
    case npiVhModOp:
      return "mod";
    case npiVhRemOp:
      return "rem";
    case npiVhExpOp:
      return "**";
    case npiVhAbsOp:
      return "abs";
    case npiVhNotOp:
      return "not";
    default:
      assert(0);
      break;
  }
  return "";
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_map_elem(npiHandle hdl)
{
  assert(hdl);
  static string meStr = "";
  meStr.assign("");
  npiHandle faHdl = NULL;
  faHdl = npi_handle(npiVhFormal, hdl);
  meStr.append(get_vh_expr(faHdl));
  meStr.append(" => ");
  npi_release_handle(faHdl);
  faHdl = npi_handle(npiVhActual, hdl);
  meStr.append(get_vh_expr(faHdl));
  npi_release_handle(faHdl);
  return meStr.data();
}
//------------------------------------------------------------------------------
const char* npi_code_gen_t::get_vh_waveform(npiHandle hdl, bool isRecursive)
{
  static string wfStr = "";
  if (!isRecursive)
    wfStr.assign("");
  npiHandle exprHdl = NULL;
  npiHandle wfHdl = NULL;
  npiHandle itr = NULL;
  int cnt = 0;
  switch (npi_get(npiType, hdl)) {
    case npiVhWaveformElem:
      if (npi_get(npiVhUnaffected, hdl) == 1) {
        wfStr.append("unaffected");
        break;
      }
      exprHdl = npi_handle(npiVhValExpr, hdl);
      assert(exprHdl);
      wfStr.append(get_vh_expr(exprHdl));
      npi_release_handle(exprHdl);
      exprHdl = npi_handle(npiVhTimeExpr, hdl);
      if (exprHdl) {
        wfStr.append(" after ");
        wfStr.append(get_vh_expr(exprHdl));
        npi_release_handle(exprHdl);
      }
      break;
    case npiVhCondWaveform:
      wfHdl = npi_handle(npiVhWaveformElem, hdl);
      assert(wfHdl);
      get_vh_waveform(wfHdl, true/*isRecursive*/);
      npi_release_handle(wfHdl);
      exprHdl = npi_handle(npiVhCondExpr, hdl);
      if (exprHdl) {
        wfStr.append(" when ");
        wfStr.append(get_vh_expr(exprHdl));
        npi_release_handle(exprHdl);
      }
      break;
    case npiVhSelectWaveform:
      wfHdl = npi_handle(npiVhWaveformElem, hdl);
      assert(wfHdl);
      get_vh_waveform(wfHdl, true/*isRecursive*/);
      npi_release_handle(wfHdl);
      itr = npi_iterate(npiVhChoices, hdl);
      if (!itr) {
        wfStr.append(" when others");
        break;
      }
      cnt = 0;
      while ((exprHdl = npi_scan(itr))) {
        if (cnt++ > 0)
          wfStr.append(" | ");
        else
          wfStr.append(" when ");
        wfStr.append(get_vh_expr(exprHdl));
        npi_release_handle(exprHdl);
      }
      break;
    default:
      assert(0);
      break;
  }
  return wfStr.data();
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::extract_file_name(string &file)
{
  int lastDivPos = file.rfind('/');
  if (lastDivPos < 0)
    return false;
  file = file.substr(lastDivPos+1);
  return true;
}
//------------------------------------------------------------------------------
bool npi_code_gen_t::is_decl_type(int type)
{
  switch (type) {
    case npiVhSubpDecl:
    case npiVhFunction:
    case npiVhProcedure:
    case npiVhPackDecl:
    case npiVhPackBody:
    case npiVhEnumTypeDecl:
    case npiVhPhysTypeDecl:
    case npiVhIntTypeDecl:
    case npiVhFloatTypeDecl:
    case npiVhArrayTypeDecl:
    case npiVhRecordTypeDecl:
    case npiVhSubTypeDecl:
    case npiVhConst:
    case npiVhSig:
    case npiVhVar:
    case npiVhAttrDecl:
      return true;
    default:
      return false;
  }
}
//------------------------------------------------------------------------------

#endif   /* ----- #ifndef NPI_CODE_GEN_CPP  ----- */

