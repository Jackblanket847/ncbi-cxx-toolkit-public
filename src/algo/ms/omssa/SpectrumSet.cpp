/* $Id$
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
 * Author:  Lewis Y. Geer
 *
 * File Description:
 *   Contains code for reading in spectrum data sets.
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'omssa.asn'.
 */

// standard includes
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <util/regexp.hpp>
#include <objects/omssa/MSSpectrum.hpp>


// generated includes
#include "SpectrumSet.hpp"

// added includes
#include "msms.hpp"

// generated classes
BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


/////////////////////////////////////////////////////////////////////////////
//
//  CSpectrumSet::
//


// load multiple dta's
int CSpectrumSet::LoadMultDTA(std::istream& DTA)
{   
    CRef <CMSSpectrum> MySpectrum;
    int iIndex(-1); // the spectrum index
    string Line;
    double dummy;
    try{
	do {
	    do {
		getline(DTA, Line);
	    } while(NStr::Compare(Line, 0, 4, "<dta") != 0 && DTA && !DTA.eof());
	    if(!DTA || DTA.eof()) break;
    
	    MySpectrum = new CMSSpectrum;
	    CRegexp RxpGetNum("\\sid\\s*=\\s*(\"(\\S+)\"|(\\S+)\b)");
	    string Match;
	    if((Match = RxpGetNum.GetMatch(Line.c_str(), 0, 2)) != "" ||
	       (Match = RxpGetNum.GetMatch(Line.c_str(), 0, 3)) != "") {
		MySpectrum->SetNumber(NStr::StringToInt(Match));
	    }
	    else {
		MySpectrum->SetNumber(iIndex);
		iIndex--;
	    }

	    CRegexp RxpGetName("\\sname\\s*=\\s*(\"(\\S+)\"|(\\S+)\b)");
	    if((Match = RxpGetName.GetMatch(Line.c_str(), 0, 2)) != "" ||
	       (Match = RxpGetName.GetMatch(Line.c_str(), 0, 3)) != "") {
		MySpectrum->SetName(Match);
	    }
    
	    MySpectrum->SetScale(MSSCALE);
	    DTA >> dummy;
	    MySpectrum->SetPrecursormz(static_cast <int> (dummy*MSSCALE));
	    DTA >> dummy;
	    MySpectrum->SetCharge().push_back(static_cast <int> (dummy));
	    getline(DTA, Line);
	    getline(DTA, Line);

	    while(NStr::Compare(Line, 0, 5, "</dta") != 0) {
		dummy = 0;
		CNcbiIstrstream istr(Line.c_str());
		istr >> dummy;
		if(dummy == 0) break;
		MySpectrum->SetMz().push_back(static_cast <int> 
					      (dummy*MSSCALE));
		// attenuate the really big peaks
		istr >> dummy;
		if(dummy > kMax_UInt) dummy = kMax_UInt/MSSCALE;
		MySpectrum->SetAbundance().push_back(static_cast <int>
						     (dummy*MSSCALE));
		getline(DTA, Line);
	    } 

	    Set().push_back(MySpectrum);
	} while(DTA && !DTA.eof());

    } catch (exception& e) {
	ERR_POST(Error << e.what());
	return 1;
    }

    return 0;
}


// load in a single dta file

int CSpectrumSet::LoadDTA(std::istream& DTA)
{   
    CRef <CMSSpectrum> MySpectrum(new CMSSpectrum);
    MySpectrum->SetNumber(1);
    MySpectrum->SetScale(MSSCALE);

    double dummy;
    DTA >> dummy;
    MySpectrum->SetPrecursormz(static_cast <int> (dummy*MSSCALE));
    DTA >> dummy;
    MySpectrum->SetCharge().push_back(static_cast <int> (dummy));
    
    while(DTA) {
	dummy = 0;
	DTA >> dummy;
	if(dummy == 0) break;
	MySpectrum->SetMz().push_back(static_cast <int> (dummy*MSSCALE));
	// attenuate the really big peaks
	DTA >> dummy;
	if(dummy > kMax_UInt) dummy = kMax_UInt/MSSCALE;
	MySpectrum->SetAbundance().push_back(static_cast <int> (dummy*MSSCALE));
    } 

    Set().push_back(MySpectrum);
    return 0;
}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2004/03/16 20:18:54  gorelenk
 * Changed includes of private headers.
 *
 * Revision 1.4  2003/10/22 15:03:32  lewisg
 * limits and string compare changed for gcc 2.95 compatibility
 *
 * Revision 1.3  2003/10/21 21:20:57  lewisg
 * use strstream instead of sstream for gcc 2.95
 *
 * Revision 1.2  2003/10/21 21:12:16  lewisg
 * reorder headers
 *
 * Revision 1.1  2003/10/20 21:32:13  lewisg
 * ommsa toolkit version
 *
 *
 * ===========================================================================
 */
/* Original file checksum: lines: 56, chars: 1753, CRC32: bdc55e21 */
