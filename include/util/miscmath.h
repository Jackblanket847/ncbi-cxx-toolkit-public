#ifndef UTIL___MISCMATH__HPP
#define UTIL___MISCMATH__HPP

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
 * Author:  Aaron Ucko
 *
 */

/** @file miscmath.hpp
 * Miscellaneous math functions that might not be part of the
 * system's standard libraries. */

#include <corelib/mswin_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Miscellaneous
 *
 * @{
 */

/** The error function of x: the integral from 0 to x of e(-t*t) dt,
 *  scaled by 2/sqrt(pi) to fall within the range (-1,1). */
NCBI_XUTIL_EXPORT
double NCBI_Erf(double x);

/** The complementary error function of x: 1 - erf(x), but calculated
 *  more accurately for large x (where erf(x) approaches unity). */
NCBI_XUTIL_EXPORT
double NCBI_ErfC(double x);

/* @} */

#ifdef __cplusplus
}
#endif



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/12/20 22:18:18  vakatov
 * Moved from <util/math/> to <util/>
 *
 * Revision 1.2  2005/08/15 19:10:02  ucko
 * Take care to ensure proper linkage in all cases.
 *
 * Revision 1.1  2005/08/12 14:58:33  ucko
 * Ensure that implementations of erf and erfc are always available.
 *
 * ===========================================================================
 */

#endif  /* UTIL___MISCMATH__HPP */
