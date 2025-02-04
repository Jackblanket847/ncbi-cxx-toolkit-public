#ifndef NETSTORAGE_EXCEPTION__HPP
#define NETSTORAGE_EXCEPTION__HPP

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
 * Authors:  Denis Vakatov
 *
 * File Description: Network Storage middleman server exception
 *
 */


#include <corelib/ncbiexpt.hpp>
#include <connect/services/netstorage.hpp>


BEGIN_NCBI_SCOPE


class CNetStorageServerException : public CException,
                                   public CNetStorageServerError
{
    public:
        typedef CNetStorageServerError::EErrCode EErrCode;

        virtual const char *  GetErrCodeString(void) const override;
        unsigned int ErrCodeToHTTPStatusCode(void) const;
        NCBI_EXCEPTION_DEFAULT(CNetStorageServerException, CException);
};


END_NCBI_SCOPE


#endif /* NETSTORAGE_EXCEPTION__HPP */

