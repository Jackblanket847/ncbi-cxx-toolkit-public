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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'omssa.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/omssa/MSSearchSettings.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CMSSearchSettings::~CMSSearchSettings(void)
{
}


///
/// Validate Search Settings
/// returns 0 if OK, 1 if not
/// Error contains explanation
///
int CMSSearchSettings::Validate(std::list<std::string> & Error) const
{
    int retval(0);


    if (CanGetPrecursorsearchtype()) {
        if (GetPrecursorsearchtype() < 0 || GetPrecursorsearchtype() >= eMSSearchType_max ) {
            Error.push_back("Invalid precursor search type");
            retval = 1;
        }
    }
    else {
        Error.push_back("Precursor search type missing");
        retval = 1;
    }

    if (CanGetProductsearchtype()) {
        if (GetProductsearchtype() < 0 || GetProductsearchtype() >= eMSSearchType_max ) {
            Error.push_back("Invalid Product search type");
            retval = 1;
        }
    }
    else {
        Error.push_back("Product search type missing");
        retval = 1;
    }


    if (CanGetIonstosearch()) {
        TIonstosearch::const_iterator i;
        for (i = GetIonstosearch().begin(); i != GetIonstosearch().end(); i++)
            if ( *i < 0 || *i >= eMSIonType_max ) {
                Error.push_back("Unknown search ion");
                retval = 1;
            }
    }
    else {
        Error.push_back("Ion search type missing");
        retval = 1;
    }




    if (CanGetPeptol()) {
        if ( GetPeptol() < 0) {
            Error.push_back("Precursor mass tolerance less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Precursor mass tolerance missing");
        retval = 1;
    }


    if (CanGetMsmstol()) {
        if (GetMsmstol() < 0) {
            Error.push_back("Product mass tolerance less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Product mass tolerance missing");
        retval = 1;
    }

    if (CanGetExactmass()) {
        if (GetExactmass() < 0) {
            Error.push_back("Exact Mass threshold less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Exact Mass threshold  missing");
        retval = 1;
    }


    if (CanGetZdep()) {
        if (GetZdep() < eMSZdependence_independent ||
            GetZdep() >= eMSZdependence_max) {
            Error.push_back("Precursor mass tolerance charge dependence setting invalid");
            retval = 1;
        }
    }
    else {
        Error.push_back("Precursor mass tolerance charge dependence missing");
        retval = 1;
    }


    if (CanGetCutlo()) {
        if (GetCutlo() < 0 || GetCutlo() > 1) {
            Error.push_back("Low cut value out of range");
            retval = 1;
        }
    }
    else {
        Error.push_back("Low cut value missing");
        retval = 1;
    }


    if (CanGetCuthi()) {
        if (GetCuthi() < 0 || GetCuthi() > 1) {
            Error.push_back("Hi cut value out of range");
            retval = 1;
        }
    }
    else {
        Error.push_back("Hi cut value missing");
        retval = 1;
    }


    if (CanGetCuthi() && CanGetCutlo()) {
        if (GetCutlo() > GetCuthi()) {
            Error.push_back("Lo cut exceeds Hi cut value");
            retval = 1;
        }
        else if (CanGetCutinc() && GetCutinc() <= 0) {
            Error.push_back("Cut increment too small");
            retval = 1;
        }
    }

    if (CanGetCutinc()) {
        if (GetCutinc() < 0) {
            Error.push_back("Cut increment less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Cut increment missing");
        retval = 1;
    }


    if (CanGetSinglewin()) {
        if (GetSinglewin() < 0) {
            Error.push_back("Single win size less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Single win size missing");
        retval = 1;
    }


    if (CanGetDoublewin()) {
        if (GetDoublewin() < 0) {
            Error.push_back("Double win size less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Double win size missing");
        retval = 1;
    }


    if (CanGetSinglenum()) {
        if (GetSinglenum() < 0) {
            Error.push_back("Single num less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Single num missing");
        retval = 1;
    }


    if (CanGetDoublenum()) {
        if (GetDoublenum() < 0) {
            Error.push_back("Double num less than 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("Double num missing");
        retval = 1;
    }


    if (CanGetFixed()) {
        TFixed::const_iterator i;
        for (i = GetFixed().begin(); i != GetFixed().end(); i++)
            if ( *i < 0 ) {
                Error.push_back("Unknown fixed mod");
                retval = 1;
            }
    }


    if (CanGetVariable()) {
        TVariable::const_iterator i;
        for (i = GetVariable().begin(); i != GetVariable().end(); i++)
            if ( *i < 0 ) {
                Error.push_back("Unknown variable mod");
                retval = 1;
            }
    }


    if (CanGetEnzyme()) {
        if (GetEnzyme() < 0) {
            Error.push_back("Unknown enzyme");
            retval = 1;
        }
    }
    else {
        Error.push_back("Enzyme missing");
        retval = 1;
    }


    if (CanGetMissedcleave()) {
        if (GetMissedcleave() < 0) {
            Error.push_back("Invalid missed cleavage value");
            retval = 1;
        }
    }
    else {
        Error.push_back("Missed cleavage value missing");
        retval = 1;
    }


    if (CanGetHitlistlen()) {
        if (GetHitlistlen() < 1) {
            Error.push_back("Invalid hit list length");
            retval = 1;
        }
    }
    else {
        Error.push_back("Hit list length missing");
        retval = 1;
    }


    if (CanGetDb()) {
        if (GetDb().empty()) {
            Error.push_back("Empty BLAST library name");
            retval = 1;
        }
    }
    else {
        Error.push_back("BLAST library name missing");
        retval = 1;
    }


    if (CanGetTophitnum()) {
        if (GetTophitnum() < 1) {
            Error.push_back("Less than one top hit needed");
            retval = 1;
        }
    }
    else {
        Error.push_back("Missing top hit value");
        retval = 1;
    }


    if (CanGetMinhit()) {
        if (GetMinhit() < 1) {
            Error.push_back("Less than one match need for hit");
            retval = 1;
        }
    }
    else {
        Error.push_back("Missing minimum number of matches");
        retval = 1;
    }


    if (CanGetMinspectra()) {
        if (GetMinspectra() < 2) {
            Error.push_back("Less than two peaks required in input spectra");
            retval = 1;
        }
    }
    else {
        Error.push_back("Missing minimum peaks value for valid spectra");
        retval = 1;
    }


    if (CanGetScale()) {
        if (GetScale() < 1) {
            Error.push_back("m/z scaling value less than one");
            retval = 1;
        }
    }
    else {
        Error.push_back("missing m/z scaling value");
        retval = 1;
    }

    // optional arguments

    if (IsSetTaxids()) {
        TTaxids::const_iterator i;
        for (i = GetTaxids().begin(); i != GetTaxids().end(); i++)
            if ( *i < 0 ) {
                Error.push_back("taxid < 0");
                retval = 1;
            }
    }


    if(IsSetIterativesettings()){
        if(!GetIterativesettings().CanGetResearchthresh() ||
           !GetIterativesettings().CanGetSubsetthresh() ||
           !GetIterativesettings().CanGetReplacethresh()) {
            Error.push_back("unknown iterative search setting");
            retval = 1;
        }
        else {
            if(GetIterativesettings().GetResearchthresh() < 0.0) {
                Error.push_back("threshold to iteratively search a spectrum again < 0");
                retval = 1;
            }
            if(GetIterativesettings().GetSubsetthresh() < 0.0) {
                 Error.push_back("threshold to include a sequence in the iterative search < 0");
                 retval = 1;
            }
            if(GetIterativesettings().GetReplacethresh() < 0.0) {
                Error.push_back("threshold to replace a hit < 0");
                retval = 1;
            }
        }
    }


    if (IsSetChargehandling()) {
        if (!GetChargehandling().CanGetMaxcharge() ||
            !GetChargehandling().CanGetMincharge() ||
            !GetChargehandling().CanGetCalccharge() ||
            !GetChargehandling().CanGetCalcplusone() ||
            !GetChargehandling().CanGetConsidermult() ||
            !GetChargehandling().CanGetPlusone() ||
            !GetChargehandling().CanGetMaxproductcharge()) {
            Error.push_back("charge handling option is missing");
            retval = 1;
        }
        else {
            if (GetChargehandling().GetMaxcharge() < 1 ||
                GetChargehandling().GetMincharge() < 1) {
                Error.push_back("invalid min/max charge value");
                retval = 1;
            }
            if (GetChargehandling().GetConsidermult() < 0) {
                Error.push_back("consider multiply charge product ions value out of range");
                retval = 1;
            }
            if (GetChargehandling().GetCalccharge() < 0 ||
                GetChargehandling().GetCalccharge() > 2) {
                Error.push_back("invalid calc charge setting");
                retval = 1;
            }
            if (GetChargehandling().GetCalcplusone() < 0 ||
                GetChargehandling().GetCalcplusone() > 1) {
                Error.push_back("invalid 1+ charge calc setting");
                retval = 1;
            }
            if (GetChargehandling().GetPlusone() < 0 ||
                GetChargehandling().GetPlusone() > 1) {
                Error.push_back("invalid 1+ charge determination threshold");
                retval = 1;
            }
            if (GetChargehandling().GetMaxproductcharge() < 0 ||
                  GetChargehandling().GetMaxproductcharge() > 20) {
                  Error.push_back("invalid maximum product ion charge");
                  retval = 1;
            }
        }
    }

    if (CanGetMaxmods()) {
        if (GetMaxmods() < 0) {
            Error.push_back("maximum number of modifications is negative");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get maximum number of modifications");
        retval = 1;
    }

    if (CanGetPseudocount()) {
        if (GetPseudocount() < 0) {
            Error.push_back("pseudocount value less than zero");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get pseudocount");
        retval = 1;
    }

    if (CanGetSearchb1()) {
        if (GetSearchb1() != 1 && GetSearchb1() != 0) {
            Error.push_back("invalid setting for first forward ion search");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get setting for first forward ion search");
        retval = 1;
    }

    if (CanGetSearchctermproduct()) {
        if (GetSearchctermproduct() != 1 && GetSearchctermproduct() != 0) {
            Error.push_back("invalid setting for c-term ion search");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get setting for c-term ion search");
        retval = 1;
    }

    if (CanGetMaxproductions()) {
        if (GetMaxproductions() < 0) {
            Error.push_back("maximum number of product ions < 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get maximum number of product ions");
        retval = 1;
    }

    if (CanGetMinnoenzyme()) {
        if (GetMinnoenzyme() < 1) {
            Error.push_back("minimum size of peptide in noenzyme search < 1");
            retval = 1;
        }
        if (CanGetMaxnoenzyme()) {
            if (GetMaxnoenzyme() < GetMinnoenzyme() && GetMaxnoenzyme() != 0) {
                Error.push_back("maximum size of peptide in noenzyme search exceeds minimum peptide size");
                retval = 1;
            }
        }
        else {
            Error.push_back("unable to get maximum size of peptide in noenzyme search");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get minimum size of peptide in noenzyme search");
        retval = 1;
    }


    if (CanGetExactmass()) {
        if (GetExactmass() < 0) {
            Error.push_back("exact mass threshold set to < 0");
            retval = 1;
        }
    }
    else {
        Error.push_back("unable to get exact mass threshold");
        retval = 1;
    }

    if(CanGetPrecursorcull()) {
        if (GetPrecursorcull() < 0 || GetPrecursorcull() > 1) {
             Error.push_back("precursor cull setting < 0 or > 1");
             retval = 1;
         }
     }
     else {
         Error.push_back("unable to precursor cull setting");
         retval = 1;
     }

     if(CanGetNocorrelationscore()) {
        if (GetNocorrelationscore() != 0 && GetNocorrelationscore() != 1) {
             Error.push_back("unknown correlation score setting");
             retval = 1;
         }
     }
     else {
         Error.push_back("unable to get correlation score setting");
         retval = 1;
     }

     if(CanGetProbfollowingion()) {
        if (GetProbfollowingion() < 0.0 || GetProbfollowingion() > 1.0) {
             Error.push_back("probability of consecutive ions is < 0 or > 1");
             retval = 1;
         }
     }
     else {
         Error.push_back("unable to get probability of consecutive ions");
         retval = 1;
     }

     if(CanGetAutomassadjust()) {
         if (GetAutomassadjust() < 0.0 || GetAutomassadjust() > 1.0) {
              Error.push_back("automatic mass tolerance adjustment fraction is < 0 or > 1");
              retval = 1;
          }
      }
      else {
          Error.push_back("unable to get automatic mass tolerance adjustment fraction ");
          retval = 1;
      }

    return retval;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 64, chars: 1885, CRC32: ffa5fdc2 */
