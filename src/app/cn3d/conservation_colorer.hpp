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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to color alignment blocks by sequence conservation
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2000/10/16 14:25:20  thiessen
* working alignment fit coloring
*
* Revision 1.3  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.2  2000/10/04 17:40:45  thiessen
* rearrange STL #includes
*
* Revision 1.1  2000/09/20 22:24:57  thiessen
* working conservation coloring; split and center unaligned justification
*
* ===========================================================================
*/

#ifndef CN3D_CONSERVATION_COLORER__HPP
#define CN3D_CONSERVATION_COLORER__HPP

#include <corelib/ncbistl.hpp>

#include <map>
#include <vector>

#include <corelib/ncbistl.hpp>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class UngappedAlignedBlock;

class ConservationColorer
{
public:
    ConservationColorer(void);

    // add an aligned block to the profile
    void AddBlock(const UngappedAlignedBlock *block);

    // create colors - should be done only after all blocks have been added
    void CalculateConservationColors(void);

    static const Vector MinimumConservationColor, MaximumConservationColor;

private:
    typedef std::map < const UngappedAlignedBlock *, std::vector < int > > BlockMap;
    BlockMap blocks;

    int GetProfileIndex(const UngappedAlignedBlock *block, int blockColumn) const
        { return blocks.find(block)->second.at(blockColumn); }
    void GetProfileIndexAndResidue(const UngappedAlignedBlock *block, int blockColumn, int row,
        int *profileIndex, char *residue) const;

    int nColumns;

    std::vector < bool > identities;

    typedef std::vector < Vector > ColumnColors;
    ColumnColors varietyColors, weightedVarietyColors;

    typedef std::map < char, Vector > ResidueColors;
    typedef std::vector < ResidueColors > FitColors;
    FitColors fitColors;

public:

    void Clear(void)
    {
        blocks.clear();
        nColumns = 0;
    }

    // color accessors
    const Vector *GetIdentityColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(identities[GetProfileIndex(block, blockColumn)]
            ? MaximumConservationColor : MinimumConservationColor); }

    const Vector *GetVarietyColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(varietyColors[GetProfileIndex(block, blockColumn)]); }

    const Vector *GetWeightedVarietyColor(const UngappedAlignedBlock *block, int blockColumn) const
        { return &(weightedVarietyColors[GetProfileIndex(block, blockColumn)]); }

    const Vector *GetFitColor(const UngappedAlignedBlock *block, int blockColumn, int row) const
    { 
        int profileIndex;
        char residue;
        GetProfileIndexAndResidue(block, blockColumn, row, &profileIndex, &residue);
        return &(fitColors[profileIndex].find(residue)->second);
    }    
};

END_SCOPE(Cn3D)

#endif // CN3D_CONSERVATION_COLORER__HPP
