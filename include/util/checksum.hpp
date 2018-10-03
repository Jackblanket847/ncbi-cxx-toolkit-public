#ifndef CHECKSUM__HPP
#define CHECKSUM__HPP

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
 * Author:  Eugene Vasilchenko
 *
 */

/// @file checksum.hpp
/// Checksum (CRC32 or MD5) calculation class.

#include <corelib/ncbistd.hpp>
#include <corelib/reader_writer.hpp>
#include <util/md5.hpp>


/** @addtogroup Checksum
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CChecksum -- Checksum calculator
///
/// This class is used to compute control sums (CRC32, Adler32, MD5).
/// Please note that this class uses static tables. This presents a potential
/// race condition in MT programs. To avoid races call static InitTables method
/// before first concurrent use of CChecksum.
///
class NCBI_XUTIL_EXPORT CChecksum
{
public:
    /// Method used to compute control sum.
    enum EMethod {
        eNone,             ///< No checksum in file.
        eCRC32,            ///< 32-bit Cyclic Redundancy Check.
                           ///< Most significant bit first, no inversions.
        eCRC32ZIP,         ///< Exact zip CRC32. Same polynomial as eCRC32.
                           ///< Least significant bit are processed first,
                           ///< extra inversions at the beginning and the end.
        eCRC32INSD,        ///< Inverted CRC32ZIP. Hash function used in
                           ///< the ID system to verify sequence uniqueness.
        eMD5,              ///< Message Digest version 5.
        eAdler32,          ///< A bit faster than CRC32ZIP, not recommended
                           ///< for small data sizes.
        eCRC32CKSUM,       ///< CRC32 implemented by cksum utility.
                           ///< Same as eCRC32, but with data length included
                           ///< in CRC, and extra inversion at the end.
        eCRC32C,           ///< CRC32C (Castagnoli). Better polynomial used in some
                           ///< places (iSCSI). This method has hardware support in new
                           ///< Intel processors. Least significant bits are processed first,
                           ///< extra inversions at the beginning and the end.

        eCityHash32,       ///< CityHash 32-bit. Can be used with Calculate() only.
        eCityHash64,       ///< CityHash 64-bit. Can be used with Calculate() only.

        // ATTENTION! 
        // FarmHash methods may change from time to time, may differ on different platforms,
        // may differ depending on NDEBUG. So, shouldn't be used for storing. 
        
        eFarmHash32,       ///< FarmHash 32-bit. Can be used with Calculate() only.
        eFarmHash64,       ///< FarmHash 64-bit. Can be used with Calculate() only.
        eDefault = eCRC32
    };
    enum {
        kMinimumChecksumLength = 20
    };

    /// Default constructor.
    CChecksum(EMethod method = eDefault);
    /// Copy constructor.
    CChecksum(const CChecksum& checksum);
    /// Destructor.
    ~CChecksum();
    /// Assignment operator.
    CChecksum& operator=(const CChecksum& checksum);

    /// Get current method used to compute control sum.
    EMethod GetMethod(void) const;

    /// Check that method is specified (not equal eNone).
    bool Valid(void) const;

    /// Return size of checksum in bytes.
    size_t GetChecksumSize(void) const;

    /// Return size of checksum in bits (32, 64).
    size_t GetChecksumBits(void) const {
        return GetChecksumSize()*8;
    };

    /// Return calculated checksum (legacy 32-bit version).
    // @sa GetChecksum32, GetChecksum64
    Uint4 GetChecksum(void) const {
        return GetChecksum32();
    }

    /// Return calculated checksum.
    /// Only valid in 32-bit modes, like: CRC32*, Adler32, CityHash32, FarmHash32.
    Uint4 GetChecksum32(void) const;

    /// Return calculated checksum.
    /// Only valid in 64-bit modes, like: CityHash64, FarmHash64.
    Uint8 GetChecksum64(void) const;

    /// Return string with checksum in hexadecimal form.
    string GetHexSum(void) const;

    /// Return calculated MD5 digest.
    /// @attention Only valid in MD5 mode!
    void GetMD5Digest(unsigned char digest[16]) const;
    void GetMD5Digest(string& str) const;

    /// Reset the object to prepare it to the next checksum computation.
    void Reset(EMethod method = eNone/**<keep current method*/);

    /// One pass method to calculate checksum/hash.
    /// Similar to AddChars(), but with additional checks, it doesn't allow
    /// to update calculated checksum anymore. Cannot be combined with any Add*() methods.
    /// @note
    ///   Should be used with eCityHash32, eCitiHash64, eFarmHash32 and eFarmHash64.
    /// @sa AddChars
    void Calculate(const CTempString str);
    void Calculate(const char* str, size_t len);

    // Methods used for file/stream operations
    //
    // ATTENTION!
    //
    // The following Add*() methods are not implemented for methods
    // eCityHash32, eCitiHash64, eFarmHash32 and eFarmHash64!
    // Use Calculate().

    /// Update current control sum with data provided.
    void AddChars(const char* str, size_t len);
    void AddLine(const char* line, size_t len);
    void AddLine(const string& line);
    void NextLine(void);

    /// Compute checksum for the file, add it to this checksum.
    /// On any error an exception will be thrown, and the checksum
    /// will not change.
    void AddFile(const string& file_path);

    /// Compute checksum for the stream, add it to this checksum.
    /// On any error an exception will be thrown, and the checksum
    /// will not change.
    /// @param is
    ///   Input stream to read data from.
    ///   Please use ios_base::binary flag for the input stream
    ///   if you want to count all symbols there, including end of lines.
    void AddStream(CNcbiIstream& is);

    /// Check for checksum line.
    bool ValidChecksumLine(const char* line, size_t len) const;
    bool ValidChecksumLine(const string& line) const;

    /// Write checksum calculation results into output stream
    CNcbiOstream& WriteChecksum(CNcbiOstream& out) const;
    CNcbiOstream& WriteChecksumData(CNcbiOstream& out) const;
    CNcbiOstream& WriteHexSum(CNcbiOstream& out) const;

public:
    /// Initialize static tables used in checksum calculation.
    /// There is no need to call this method since it's done internally.
    static void InitTables(void);

    /// Print C++ code for CRC32 tables for direct inclusion into library.
    /// It also eliminates the need to initialize CRC32 tables.
    static void PrintTables(CNcbiOstream& out);

private:
    size_t   m_LineCount;  ///< Number of lines
    size_t   m_CharCount;  ///< Number of chars
    EMethod  m_Method;     ///< Current method

    /// Checksum computation results
    union {
        Uint4 CRC32;  ///< Used to store 32-bit checksums
        Uint8 CRC64;  ///< Used to store 64-bit checksums
        CMD5* MD5;    ///< Used for MD5 calculation
    } m_Checksum;

    /// Check for checksum line.
    bool ValidChecksumLineLong(const char* line, size_t len) const;
    /// Update current control sum with data provided.
    void x_Update(const char* str, size_t len);
    /// Cleanup (used in destructor and assignment operator).
    void x_Free(void);
};


/// Write checksum calculation results into output stream
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum);

/// Compute checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
CChecksum  ComputeFileChecksum(const string& path, CChecksum::EMethod method);

/// Computes checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
CChecksum& ComputeFileChecksum(const string& path, CChecksum& checksum);

/// Compute CRC32 checksum for the given file.
/// @deprecated
///   Please use CChecksum::AddFile() method instead.
NCBI_DEPRECATED NCBI_XUTIL_EXPORT
Uint4      ComputeFileCRC32(const string& path);



/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumException --
///
/// Define exceptions generated for CChecksum methods.
///
/// CChecksumException inherits its basic functionality from CCoreException
/// and defines additional error codes for CChecksum methods.

class NCBI_XNCBI_EXPORT CChecksumException : public CCoreException
{
public:
    /// Error types that can be generated.
    enum EErrCode {
        eStreamIO,
        eFileIO
    };

    /// Translate from an error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CChecksumException, CCoreException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CChecksumStreamWriter --
///
/// Stream class to compute checksum for written data.
///
class NCBI_XUTIL_EXPORT CChecksumStreamWriter : public IWriter
{
public:
    /// Construct object to compute checksum for written data.
    CChecksumStreamWriter(CChecksum::EMethod method);
    virtual ~CChecksumStreamWriter(void);

    /// Virtual methods from IWriter
    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0);
    virtual ERW_Result Flush(void);

    /// Return checksum
    const CChecksum& GetChecksum(void) const;

private:
    CChecksum m_Checksum;  ///< Checksum calculator
};


/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

inline
CChecksum::EMethod CChecksum::GetMethod(void) const
{
    return m_Method;
}

inline
bool CChecksum::Valid(void) const
{
    return GetMethod() != eNone;
}

inline
void CChecksum::Calculate(const CTempString str)
{
    _ASSERT(!m_CharCount);
    x_Update(str.data(), str.size());
    m_CharCount = str.size();
}

inline
void CChecksum::Calculate(const char* str, size_t count)
{
    _ASSERT(!m_CharCount);
    x_Update(str, count);
    m_CharCount = count;
}

inline
void CChecksum::AddChars(const char* str, size_t count)
{
    x_Update(str, count);
    m_CharCount += count;
}

inline
void CChecksum::AddLine(const char* line, size_t len)
{
    AddChars(line, len);
    NextLine();
}

inline
void CChecksum::AddLine(const string& line)
{
    AddLine(line.data(), line.size());
}

inline
bool CChecksum::ValidChecksumLine(const char* line, size_t len) const
{
    return len > kMinimumChecksumLength &&
        line[0] == '/' && line[1] == '*' &&  // first four letters of checksum
        line[2] == ' ' && line[3] == 'O' &&  // see sx_Start in checksum.cpp
        ValidChecksumLineLong(line, len);    // complete check
}

inline
bool CChecksum::ValidChecksumLine(const string& line) const
{
    return ValidChecksumLine(line.data(), line.size());
}

inline
void CChecksum::GetMD5Digest(unsigned char digest[16]) const
{
    _ASSERT(GetMethod() == eMD5);
    m_Checksum.MD5->Finalize(digest);
}

inline
void CChecksum::GetMD5Digest(string& str) const
{
    unsigned char buf[16];
    m_Checksum.MD5->Finalize(buf);
    str.clear();
    str.insert(str.end(), (const char*)buf, (const char*)buf + 16);
}

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CChecksum& checksum)
{
    return checksum.WriteChecksum(out);
}


inline
const CChecksum& CChecksumStreamWriter::GetChecksum(void) const
{
    return m_Checksum;
}


END_NCBI_SCOPE

#endif  /* CHECKSUM__HPP */
