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
 * Authors:  Josh Cherry
 *
 * File Description:  Find occurrences of a regular expression in a sequence
 *
 */


#include <util/regexp.hpp>
#include "find_pattern.hpp"

BEGIN_NCBI_SCOPE


// Find all occurences of a pattern (regular expression) in a sequence.
void CFindPattern::Find(const string& seq, const string& pattern,
                       vector<TSeqPos>& starts, vector<TSeqPos>& ends) {

    // do a case-insensitive r.e. search for "all" (non-overlapping) occurences
    // note that it's somewhat ambiguous what this means

    // want to ignore case, and to allow white space (and comments) in pattern
    CRegexp re(pattern, PCRE_CASELESS | PCRE_EXTENDED);

    starts.clear();
    ends.clear();
    unsigned int offset = 0;
    while (!re.GetMatch(seq, offset).empty()) {  
        // (empty string means no match)
        const int *res = re.GetResults(0);
        starts.push_back(res[0]);
        ends.push_back(res[1] - 1);
        offset = res[1];
    }
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/12/15 20:16:09  jcherry
 * Changed CFindPattern::Find to take a string rather than a CSeqVector
 *
 * Revision 1.5  2003/12/15 19:51:07  jcherry
 * CRegexp::GetMatch now takes a string&, not a char*
 *
 * Revision 1.4  2003/07/08 22:08:00  jcherry
 * Clear 'starts' and 'ends' before using them!
 *
 * Revision 1.3  2003/07/03 19:14:12  jcherry
 * Initial version
 *
 * ===========================================================================
 */
