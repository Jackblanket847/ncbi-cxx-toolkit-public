#ifndef OBJOSTRB__HPP
#define OBJOSTRB__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/06/07 19:30:18  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CDataWriter;

class CObjectOStreamBinary : public CObjectOStream
{
public:
    typedef unsigned char TByte;
    typedef map<string, TIndex> TStrings;

    CObjectOStreamBinary(CNcbiOstream& out);
    virtual ~CObjectOStreamBinary(void);

    virtual void WriteStd(const char& data);
    virtual void WriteStd(const unsigned char& data);
    virtual void WriteStd(const signed char& data);
    virtual void WriteStd(const short& data);
    virtual void WriteStd(const unsigned short& data);
    virtual void WriteStd(const int& data);
    virtual void WriteStd(const unsigned int& data);
    virtual void WriteStd(const long& data);
    virtual void WriteStd(const unsigned long& data);
    virtual void WriteStd(const float& data);
    virtual void WriteStd(const double& data);
    virtual void WriteStd(const string& data);
    virtual void WriteStd(char* const& data);
    virtual void WriteStd(const char* const& data);

    virtual void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo);
    virtual void WriteObject(TConstObjectPtr object,
                             const CClassInfoTmpl* typeInfo);
    virtual void WriteTypeInfo(TTypeInfo typeInfo);

    void WriteNull(void);
    void WriteByte(TByte byte);
    void WriteBytes(const char* bytes, size_t size);
    void WriteIndex(TIndex index);
    void WriteString(const string& str);
    bool WriteRegisteredClassInfo(TTypeInfo typeInfo);
    virtual void WriteClassInfo(TTypeInfo typeInfo);
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   TTypeInfo arg);
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   TTypeInfo arg1, TTypeInfo arg2);
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   const vector<TTypeInfo>& args);

    virtual void Begin(Block& block, unsigned count, bool tmpl);

private:
    TStrings m_Strings;

    CNcbiOstream& m_Output;
};

//#include <serial/objostrb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRB__HPP */
