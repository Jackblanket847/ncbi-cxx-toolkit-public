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
 */

/// @Math_.hpp
/// Data storage class.
///
/// This file was generated by application DATATOOL
/// using the following specifications:
/// 'samplesoap.dtd'.
///
/// ATTENTION:
///   Don't edit or commit this file into CVS as this file will
///   be overridden (by DATATOOL) without warning!

#ifndef MATH_BASE_HPP
#define MATH_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


// forward declarations
class COperand;


// generated classes

/////////////////////////////////////////////////////////////////////////////
class CMath_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CMath_Base(void);
    // destructor
    virtual ~CMath_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::list< ncbi::CRef< COperand > > TOperand;

    // getters
    // setters

    /// mandatory
    /// typedef std::list< ncbi::CRef< COperand > > TOperand
    bool IsSetOperand(void) const;
    bool CanGetOperand(void) const;
    void ResetOperand(void);
    const TOperand& GetOperand(void) const;
    TOperand& SetOperand(void);

    /// Reset the whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CMath_Base(const CMath_Base&);
    CMath_Base& operator=(const CMath_Base&);

    // data
    Uint4 m_set_State[1];
    TOperand m_Operand;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CMath_Base::IsSetOperand(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CMath_Base::CanGetOperand(void) const
{
    return true;
}

inline
const std::list< ncbi::CRef< COperand > >& CMath_Base::GetOperand(void) const
{
    return m_Operand;
}

inline
std::list< ncbi::CRef< COperand > >& CMath_Base::SetOperand(void)
{
    m_set_State[0] |= 0x1;
    return m_Operand;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // MATH_BASE_HPP
