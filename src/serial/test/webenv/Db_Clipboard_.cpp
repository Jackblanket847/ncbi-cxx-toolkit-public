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
#include "Db_Clipboard.hpp"
#include "Item_Set.hpp"

// generated classes

void CDb_Clipboard_Base::ResetName(void)
{
    m_Name.erase();
    m_set_State[0] &= ~0x3;
}

void CDb_Clipboard_Base::ResetItems(void)
{
    (*m_Items).Reset();
}

void CDb_Clipboard_Base::SetItems(CItem_Set& value)
{
    m_Items.Reset(&value);
}

void CDb_Clipboard_Base::Reset(void)
{
    ResetName();
    ResetCount();
    ResetItems();
}

BEGIN_NAMED_BASE_CLASS_INFO("Db-Clipboard", CDb_Clipboard)
{
    SET_CLASS_MODULE("NCBI-Env");
    ADD_NAMED_STD_MEMBER("name", m_Name)->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    ADD_NAMED_STD_MEMBER("count", m_Count)->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    ADD_NAMED_REF_MEMBER("items", m_Items, CItem_Set);
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CDb_Clipboard_Base::CDb_Clipboard_Base(void)
    : m_Count(0), m_Items(new CItem_Set())
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CDb_Clipboard_Base::~CDb_Clipboard_Base(void)
{
}



