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
*      Classes to hold sets of atomic data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/07/01 15:43:50  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.2  2000/06/29 19:17:47  thiessen
* improved atom map
*
* Revision 1.1  2000/06/29 14:35:05  thiessen
* new atom_set files
*
* ===========================================================================
*/

#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Atom_id.hpp>
#include <objects/mmdb2/Model_space_points.hpp>
#include <objects/mmdb2/Atomic_temperature_factors.hpp>
#include <objects/mmdb2/Atomic_occupancies.hpp>
#include <objects/mmdb2/Alternate_conformation_ids.hpp>
#include <objects/mmdb2/Alternate_conformation_id.hpp>
#include <objects/mmdb2/Anisotro_temperatu_factors.hpp>
#include <objects/mmdb2/Isotropi_temperatu_factors.hpp>
#include <objects/mmdb2/Conformation_ensemble.hpp>

#include "cn3d/atom_set.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

AtomSet::AtomSet(StructureBase *parent, const CAtomic_coordinates& coords) :
    StructureBase(parent)
{
    int nAtoms = coords.GetNumber_of_points();
    TESTMSG("model has " << nAtoms << " atomic coords");

    bool haveTemp = coords.IsSetTemperature_factors(),
         haveOccup = coords.IsSetOccupancies(),
         haveAlt = coords.IsSetAlternate_conf_ids();

    // sanity check
    if (coords.GetAtoms().GetMolecule_ids().size()!=nAtoms ||
        coords.GetAtoms().GetResidue_ids().size()!=nAtoms || 
        coords.GetAtoms().GetAtom_ids().size()!=nAtoms ||
        coords.GetSites().GetX().size()!=nAtoms || 
        coords.GetSites().GetY().size()!=nAtoms || 
        coords.GetSites().GetZ().size()!=nAtoms ||
        (haveTemp && 
            ((coords.GetTemperature_factors().IsIsotropic() &&
              coords.GetTemperature_factors().GetIsotropic().GetB().size()!=nAtoms) ||
             (coords.GetTemperature_factors().IsAnisotropic() &&
              (coords.GetTemperature_factors().GetAnisotropic().GetB_11().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_12().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_13().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_22().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_23().size()!=nAtoms ||
               coords.GetTemperature_factors().GetAnisotropic().GetB_33().size()!=nAtoms)))) ||
        (haveOccup && coords.GetOccupancies().GetO().size()!=nAtoms) ||
        (haveAlt && coords.GetAlternate_conf_ids().Get().size()!=nAtoms))
        ERR_POST(Fatal << "AtomSet: confused by list length mismatch");

    // atom-pntr iterators
    CAtom_pntrs::TMolecule_ids::const_iterator 
        i_mID = coords.GetAtoms().GetMolecule_ids().begin();
    CAtom_pntrs::TResidue_ids::const_iterator 
        i_rID = coords.GetAtoms().GetResidue_ids().begin();
    CAtom_pntrs::TAtom_ids::const_iterator 
        i_aID = coords.GetAtoms().GetAtom_ids().begin();

    // atom site iterators
    double siteScale = static_cast<double>(coords.GetSites().GetScale_factor());
    CModel_space_points::TX::const_iterator i_X = coords.GetSites().GetX().begin();
    CModel_space_points::TY::const_iterator i_Y = coords.GetSites().GetY().begin();
    CModel_space_points::TZ::const_iterator i_Z = coords.GetSites().GetZ().begin();

    // occupancy iterator
    CAtomic_occupancies::TO::const_iterator *i_occup = NULL;
    double occupScale = 0.0;
    if (haveOccup) {
        occupScale = static_cast<double>(coords.GetOccupancies().GetScale_factor());
        i_occup = &(coords.GetOccupancies().GetO().begin());
    }    

    // altConfID iterator
    CAlternate_conformation_ids::Tdata::const_iterator *i_alt = NULL;
    if (haveAlt) i_alt = &(coords.GetAlternate_conf_ids().Get().begin());

    // temperature iterators
    double tempScale = 0.0;
    CIsotropic_temperature_factors::TB::const_iterator *i_tempI = NULL;
    CAnisotropic_temperature_factors::TB_11::const_iterator *i_tempA11 = NULL;
    CAnisotropic_temperature_factors::TB_12::const_iterator *i_tempA12 = NULL;
    CAnisotropic_temperature_factors::TB_13::const_iterator *i_tempA13 = NULL;
    CAnisotropic_temperature_factors::TB_22::const_iterator *i_tempA22 = NULL;
    CAnisotropic_temperature_factors::TB_23::const_iterator *i_tempA23 = NULL;
    CAnisotropic_temperature_factors::TB_33::const_iterator *i_tempA33 = NULL;
    if (haveTemp) {
        if (coords.GetTemperature_factors().IsIsotropic()) {
            tempScale = static_cast<double>
                (coords.GetTemperature_factors().GetIsotropic().GetScale_factor());
            i_tempI = &(coords.GetTemperature_factors().GetIsotropic().GetB().begin());
        } else {
            tempScale = static_cast<double>
                (coords.GetTemperature_factors().GetAnisotropic().GetScale_factor());
            i_tempA11 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_11().begin());
            i_tempA12 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_12().begin());
            i_tempA13 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_13().begin());
            i_tempA22 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_22().begin());
            i_tempA23 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_23().begin());
            i_tempA33 = &(coords.GetTemperature_factors().GetAnisotropic().GetB_33().begin());
        }
    }

    // actually do the work of unpacking serial atom data into Atom objects
    for (int i=0; i<nAtoms; i++) {
        Atom *atom = new Atom(this);

        atom->site.x = (static_cast<double>(*(i_X++)))/siteScale;
        atom->site.y = (static_cast<double>(*(i_Y++)))/siteScale;
        atom->site.z = (static_cast<double>(*(i_Z++)))/siteScale;
        if (haveOccup)
            atom->occupancy = (static_cast<double>(*((*i_occup)++)))/occupScale;
        if (haveAlt)
            atom->altConfID = (*((*i_alt)++)).GetObject().Get()[0];
        if (haveTemp) {
            if (coords.GetTemperature_factors().IsIsotropic()) {
                atom->averageTemperature =
                    (static_cast<double>(*((*i_tempI)++)))/tempScale;
            } else {
                atom->averageTemperature = (
                    (static_cast<double>(*((*i_tempA11)++))) +
                    (static_cast<double>(*((*i_tempA12)++))) +
                    (static_cast<double>(*((*i_tempA13)++))) +
                    (static_cast<double>(*((*i_tempA22)++))) +
                    (static_cast<double>(*((*i_tempA23)++))) +
                    (static_cast<double>(*((*i_tempA33)++)))) / (tempScale * 6.0);
            }
        }

        // store pointer in map - key must be unique
        const AtomPntrKey& key = MakeKey(
            (*(i_mID++)).GetObject().Get(),
            (*(i_rID++)).GetObject().Get(),
            (*(i_aID++)).GetObject().Get(),
            atom->altConfID);
        if (atomMap.find(key) != atomMap.end())
            ERR_POST(Fatal << "confused by repeated atom pntr");
        atomMap[key] = atom;

        if (i==0) TESTMSG("first atom: x " << atom->site.x << 
                ", y " << atom->site.y << 
                ", z " << atom->site.z <<
                ", occup " << atom->occupancy <<
                ", altConfId '" << atom->altConfID << "'" <<
                ", temp " << atom->averageTemperature);
    }

    // get alternate conformer ensembles; store as string of altID characters
    if (haveAlt && coords.IsSetConf_ensembles()) {
        CAtomic_coordinates::TConf_ensembles::const_iterator i_ensemble, 
            e_ensemble = coords.GetConf_ensembles().end();
        for (i_ensemble=coords.GetConf_ensembles().begin();
             i_ensemble!=e_ensemble; i_ensemble++) {
            const CConformation_ensemble& ensemble = (*i_ensemble).GetObject();
            string *ensembleStr = new string();
            CConformation_ensemble::TAlt_conf_ids::const_iterator i_altIDs,
                e_altIDs = ensemble.GetAlt_conf_ids().end();
            for (i_altIDs=ensemble.GetAlt_conf_ids().begin();
                 i_altIDs!=e_altIDs; i_altIDs++) {
                (*ensembleStr) += ((*i_altIDs).GetObject().Get())[0];
            }
            ensembles.push_back(ensembleStr);
            TESTMSG("alt conf ensemble '" << (*ensembleStr) << "'");
        }
    }
}

AtomSet::~AtomSet(void)
{
    TESTMSG("deleting AtomSet");
}

void AtomSet::Draw(void) const
{
    TESTMSG("drawing AtomSet");
}

const Atom* AtomSet::GetAtomPntr(int mID, int rID, int aID, char altID) const
{
    AtomMap::const_iterator atom = atomMap.find(MakeKey(mID, rID, aID, altID));
    if (atom != atomMap.end())
        return (*atom).second;
    ERR_POST(Warning << "can't find atom (" << mID << ',' 
                     << rID << ',' << aID << ',' << altID << ')');
    return NULL;
}

Atom::Atom(StructureBase *parent) : 
	StructureBase(parent),
    averageTemperature(ATOM_NO_TEMPERATURE),
    occupancy(ATOM_NO_OCCUPANCY),
    altConfID(ATOM_NO_ALTCONFID)
{
}

Atom::~Atom(void)
{
    TESTMSG("deleting Atom");
}

void Atom::Draw(void) const
{
    TESTMSG("drawing Atom");
}

END_SCOPE(Cn3D)

