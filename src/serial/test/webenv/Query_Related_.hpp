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
 *   using specifications from the ASN data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

#ifndef QUERY_RELATED_BASE_HPP
#define QUERY_RELATED_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// forward declarations
class CItem_Set;
class CQuery_Command;


// generated classes

class CQuery_Related_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CQuery_Related_Base(void);
    // destructor
    virtual ~CQuery_Related_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    class C_Items : public ncbi::CSerialObject
    {
        typedef ncbi::CSerialObject Tparent;
    public:
        // constructor
        C_Items(void);
        // destructor
        ~C_Items(void);
    
        // type info
        DECLARE_INTERNAL_TYPE_INFO();
    
        // choice state enum
        enum E_Choice {
            e_not_set = 0,
            e_Items,
            e_ItemCount
        };
    
        // reset selection to none
        void Reset(void);
    
        // choice state
        E_Choice Which(void) const;
        // throw exception if current selection is not as requested
        void CheckSelected(E_Choice index) const;
        // throw exception about wrong selection
        void ThrowInvalidSelection(E_Choice index) const;
        // return selection name (for diagnostic purposes)
        static std::string SelectionName(E_Choice index);
    
        // select requested variant if needed
        void Select(E_Choice index, ncbi::EResetVariant reset = ncbi::eDoResetVariant);
    
        // variants' types
        typedef CItem_Set TItems;
        typedef int TItemCount;
    
        // variants' getters
        // variants' setters
        bool IsItems(void) const;
        const CItem_Set& GetItems(void) const;
    
        CItem_Set& GetItems(void);
        CItem_Set& SetItems(void);
        void SetItems(const CItem_Set& value);
    
        bool IsItemCount(void) const;
        const int& GetItemCount(void) const;
    
        int& GetItemCount(void);
        int& SetItemCount(void);
        void SetItemCount(const int& value);
    
    
    private:
        // copy constructor and assignment operator
        C_Items(const C_Items& );
        C_Items& operator=(const C_Items& );
    
        // choice state
        E_Choice m_choice;
        // helper methods
        void DoSelect(E_Choice index);
    
        static const char* const sm_SelectionNames[];
        // variants' data
        union {
            TItemCount m_ItemCount;
            NCBI_NS_NCBI::CSerialObject *m_object;
        };
    };
    // members' types
    typedef CQuery_Command TBase;
    typedef std::string TRelation;
    typedef std::string TDb;
    typedef C_Items TItems;

    // members' getters
    // members' setters
    void ResetBase(void);
    const CQuery_Command& GetBase(void) const;
    void SetBase(CQuery_Command& value);
    CQuery_Command& SetBase(void);

    void ResetRelation(void);
    const std::string& GetRelation(void) const;
    void SetRelation(const std::string& value);
    std::string& SetRelation(void);

    void ResetDb(void);
    const std::string& GetDb(void) const;
    void SetDb(const std::string& value);
    std::string& SetDb(void);

    void ResetItems(void);
    const C_Items& GetItems(void) const;
    void SetItems(C_Items& value);
    C_Items& SetItems(void);

    // reset whole object
    virtual void Reset(void);

    bool VerifyAssigned(size_t ) const {return true;}
    void SetAssigned(size_t ) {}

private:
    // Prohibit copy constructor and assignment operator
    CQuery_Related_Base(const CQuery_Related_Base&);
    CQuery_Related_Base& operator=(const CQuery_Related_Base&);

    // members' data
    ncbi::CRef< TBase > m_Base;
    TRelation m_Relation;
    TDb m_Db;
    ncbi::CRef< TItems > m_Items;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
CQuery_Related_Base::C_Items::E_Choice CQuery_Related_Base::C_Items::Which(void) const
{
    return m_choice;
}

inline
void CQuery_Related_Base::C_Items::CheckSelected(E_Choice index) const
{
    if ( m_choice != index )
        ThrowInvalidSelection(index);
}

inline
void CQuery_Related_Base::C_Items::Select(E_Choice index, NCBI_NS_NCBI::EResetVariant reset)
{
    if ( reset == NCBI_NS_NCBI::eDoResetVariant || m_choice != index ) {
        if ( m_choice != e_not_set )
            Reset();
        DoSelect(index);
    }
}

inline
bool CQuery_Related_Base::C_Items::IsItems(void) const
{
    return m_choice == e_Items;
}

inline
bool CQuery_Related_Base::C_Items::IsItemCount(void) const
{
    return m_choice == e_ItemCount;
}

inline
const CQuery_Related_Base::C_Items::TItemCount& CQuery_Related_Base::C_Items::GetItemCount(void) const
{
    CheckSelected(e_ItemCount);
    return m_ItemCount;
}

inline
CQuery_Related_Base::C_Items::TItemCount& CQuery_Related_Base::C_Items::GetItemCount(void)
{
    CheckSelected(e_ItemCount);
    return m_ItemCount;
}

inline
CQuery_Related_Base::C_Items::TItemCount& CQuery_Related_Base::C_Items::SetItemCount(void)
{
    Select(e_ItemCount, NCBI_NS_NCBI::eDoNotResetVariant);
    return m_ItemCount;
}

inline
void CQuery_Related_Base::C_Items::SetItemCount(const TItemCount& value)
{
    Select(e_ItemCount, NCBI_NS_NCBI::eDoNotResetVariant);
    m_ItemCount = value;
}

inline
CQuery_Related_Base::TBase& CQuery_Related_Base::SetBase(void)
{
    return (*m_Base);
}

inline
const CQuery_Related_Base::TRelation& CQuery_Related_Base::GetRelation(void) const
{
    return m_Relation;
}

inline
void CQuery_Related_Base::SetRelation(const TRelation& value)
{
    m_Relation = value;
}

inline
CQuery_Related_Base::TRelation& CQuery_Related_Base::SetRelation(void)
{
    return m_Relation;
}

inline
const CQuery_Related_Base::TDb& CQuery_Related_Base::GetDb(void) const
{
    return m_Db;
}

inline
void CQuery_Related_Base::SetDb(const TDb& value)
{
    m_Db = value;
}

inline
CQuery_Related_Base::TDb& CQuery_Related_Base::SetDb(void)
{
    return m_Db;
}

inline
CQuery_Related_Base::TItems& CQuery_Related_Base::SetItems(void)
{
    return (*m_Items);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // QUERY_RELATED_BASE_HPP
