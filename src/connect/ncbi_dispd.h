#ifndef NCBI_SERVICEP_DISPD__H
#define NCBI_SERVICEP_DISPD__H

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of NCBI network dispatcher (DISPD).
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2000/10/05 22:36:21  lavr
 * Additional parameters in call to DISPD mapper
 *
 * Revision 6.2  2000/05/22 16:53:13  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:39:49  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_connutil.h>
#include "ncbi_servicep.h"

#ifdef __cplusplus
extern "C" {
#endif


const SSERV_VTable* SERV_DISPD_Open(SERV_ITER iter, SConnNetInfo *info);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICEP_DISPD__H */

