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
 * File Description:
 *   This code is generated by application DATATOOL
 *   using specifications from the data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

// standard includes
#include <serial/serialimpl.hpp>

// generated includes
#include "Named_Item_Set.hpp"
#include "Item_Set.hpp"
#include "Name.hpp"

// generated classes

void CNamed_Item_Set_Base::ResetName(void)
{
    (*m_Name).Reset();
}

void CNamed_Item_Set_Base::SetName(CName& value)
{
    m_Name.Reset(&value);
}

void CNamed_Item_Set_Base::ResetDb(void)
{
    m_Db.erase();
    m_set_State[0] &= ~0xc;
}

void CNamed_Item_Set_Base::ResetItem_Set(void)
{
    (*m_Item_Set).Reset();
}

void CNamed_Item_Set_Base::SetItem_Set(CItem_Set& value)
{
    m_Item_Set.Reset(&value);
}

void CNamed_Item_Set_Base::Reset(void)
{
    ResetName();
    ResetDb();
    ResetItem_Set();
}

BEGIN_NAMED_BASE_CLASS_INFO("Named-Item-Set", CNamed_Item_Set)
{
    SET_CLASS_MODULE("NCBI-Env");
    ADD_NAMED_REF_MEMBER("name", m_Name, CName);
    ADD_NAMED_STD_MEMBER("db", m_Db)->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    ADD_NAMED_REF_MEMBER("item-Set", m_Item_Set, CItem_Set);
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CNamed_Item_Set_Base::CNamed_Item_Set_Base(void)
    : m_Name(new CName()), m_Item_Set(new CItem_Set())
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CNamed_Item_Set_Base::~CNamed_Item_Set_Base(void)
{
}



