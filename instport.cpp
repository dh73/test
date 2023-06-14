/* ==========================================================================
 *       Filename:  npi_inst_port_conn.cpp
 *    Description:  
 *        Version:  1.0
 *        Created:  10/24/11 16:21:54 CST
 *       Revision:  none
 *        Company:  Springsoft
 * ========================================================================== */

#ifndef  NPI_INST_PORT_CONN_CPP
#define  NPI_INST_PORT_CONN_CPP

#include "npi_inst_port_conn.h"

//------------------------------------------------------------------------------
// construct / destruct
//------------------------------------------------------------------------------
npi_inst_port_conn_t::npi_inst_port_conn_t(const char* instHierName)
  : _instHdl(NULL)
{
  if (instHierName != NULL)
    _instHdl = npi_util_get_instance_hdl_by_name(instHierName, NULL);
  // instance handle can be module / package / interface / program
  if (_instHdl != NULL)
  {
    switch (npi_get(npiType, _instHdl)) {
      case npiModule:
      case npiVhRoot:
      case npiVhComponent:
        break;
      default:
        npi_release_handle(_instHdl);
        _instHdl = NULL;
        break;
    }
  }
}
//------------------------------------------------------------------------------
npi_inst_port_conn_t::~npi_inst_port_conn_t()
{
  if (_instHdl)
    npi_release_handle(_instHdl);
}
//------------------------------------------------------------------------------
// public member functions
//------------------------------------------------------------------------------
int npi_inst_port_conn_t::get_conn_sig (hdl2hdlVecPairVec_t& portSigPairVec/*O*/, NPI_INT32 type) 
{
  portSigPairVec.clear();
  if (type != npiHighConn && type != npiLowConn)
    return 0;

  npiHandle itr = NULL, portHdl = NULL;
  if (_instHdl == NULL || (itr = npi_iterate(npiPort, _instHdl)) == NULL)
    return 0;
  
  npi_util_collect_hdl_expr_t collectHdlExpr(npi_util_compare_sig_hdl_by_full_name_type);
  hdlVec_t hdlVec;
  npiHandle exprHdl = NULL;
  npiHandle mapElemItr = NULL;
  npiHandle mapElemHdl = NULL;
  int vhHlMethod = (type == npiHighConn)? npiVhActual:npiVhFormal;
  string tmpStr = "";

  // get the language status
  int scpLangType = npi_get(npiLangType, _instHdl);
  int parMethod = (scpLangType == npiVhdlLang)? npiVhScope:npiScope;
  npiHandle parScpHdl = npi_handle(parMethod, _instHdl);
  if (parScpHdl && npi_get(npiType, parScpHdl) == npiLangInterface) {
    npiHandle rlsHdl = parScpHdl;
    parScpHdl = npi_handle(npiActual, rlsHdl);
    npi_release_handle(rlsHdl);
  }
  int parScpLangType = parScpHdl? npi_get(npiLangType, parScpHdl):npiUndefined;
  enum {
    SV_PORT_AT_TOP,
    SV_PORT_UNDER_SV,
    SV_PORT_UNDER_VH,
    VH_PORT_AT_ROOT,
    VH_PORT_UNDER_VH,
    VH_PORT_UNDER_SV,
    UNDEFINED_PORT_STATUS
  };
  int portStatus = UNDEFINED_PORT_STATUS;
  switch (parScpLangType) {
    case npiUndefined:
      if (scpLangType == npiVhdlLang)
        portStatus = VH_PORT_AT_ROOT;
      else
        portStatus = SV_PORT_AT_TOP;
      break;
    case npiVhdlLang:
      if (scpLangType == npiVhdlLang)
        portStatus = VH_PORT_UNDER_VH;
      else
        portStatus = SV_PORT_UNDER_VH;
      break;
    case npiSvLang:
      if (scpLangType == npiVhdlLang)
        portStatus = VH_PORT_UNDER_SV;
      else
        portStatus = SV_PORT_UNDER_SV;
      break;
    default:
      // unkown error case
      if (parScpHdl)
        npi_release_handle(parScpHdl);
      return 0;
  }

  while ((portHdl = npi_scan(itr)))
  {
    hdlVec.clear();
    switch (portStatus) {
      // [ port's inst under SV
      case SV_PORT_AT_TOP:
      case SV_PORT_UNDER_SV:
      case VH_PORT_UNDER_SV:
        exprHdl = npi_handle(type, portHdl);
        // if input expr hdl is included in hdlVec, do not release it
        if (exprHdl) {
          if (!collectHdlExpr.collect_sig_hdl(exprHdl, hdlVec/*O*/))
            npi_release_handle(exprHdl);
        }
        break; // ]
      // [ port's inst under VHDL
      case VH_PORT_AT_ROOT:
      case VH_PORT_UNDER_VH:
      case SV_PORT_UNDER_VH: 
        mapElemItr = npi_iterate(npiVhMapElem, portHdl);
        while ((mapElemHdl = npi_scan(mapElemItr))) {
          if (portStatus == SV_PORT_UNDER_VH && vhHlMethod == npiVhFormal) {
            // spec limitation for getting lowConn for SV inst under VH component
            tmpStr = npi_get_str(npiFullName, _instHdl);
            tmpStr += ".";
            tmpStr += npi_get_str(npiName, portHdl);
            exprHdl = npi_handle_by_name(tmpStr.c_str(), NULL);
          }
          else
            exprHdl = npi_handle(vhHlMethod, mapElemHdl);
          if (exprHdl) {
            // if input expr hdl is included in hdlVec, do not release it
            if (!collectHdlExpr.collect_sig_hdl(exprHdl, hdlVec/*O*/))
              npi_release_handle(exprHdl);
          }
          npi_release_handle(mapElemHdl);
        }
        break; // ]
      default:
        break;
    }
    portSigPairVec.push_back(make_pair(portHdl, hdlVec));
  }
  if (parScpHdl)
    npi_release_handle(parScpHdl);
  return portSigPairVec.size();
}
//------------------------------------------------------------------------------

#endif   /* ----- #ifndef NPI_INST_PORT_CONN_CPP  ----- */

