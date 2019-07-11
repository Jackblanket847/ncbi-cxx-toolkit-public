#ifndef SOURCE_QUAL_READER_HPP
#define SOURCE_QUAL_READER_HPP

#include <corelib/ncbistl.hpp>
#include <objtools/readers/mod_reader.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
class CSeq_entry;
class ILineErrorListener;
class CBioseq;
};
class CMemoryFileMap;


USING_SCOPE(objects);

class CMemorySrcFileMap;

void g_ApplyMods(
    unique_ptr<CMemorySrcFileMap>& namedSrcFileMap,
    const string& namedSrcFile,
    const string& defaultSrcFile,
    const string& commandLineStr,
    bool allowAcc,
    bool isVerbose,
    ILineErrorListener* pEC,
    CSeq_entry& entry);


class CMemorySrcFileMap
{
public:
    using TModList = CModHandler::TModList;
    using TLineMap = map<string, CTempString>;

    bool GetMods(const CBioseq& bioseq, TModList& mods);
    void MapFile(const string& fileName, bool allowAcc);
    bool Empty(void) const;
    const TLineMap& GetLineMap(void) const;
    void ReportUnusedIds(ILineErrorListener* pEC);
private:
    bool m_FileMapped = false;
    unique_ptr<CMemoryFileMap> m_pFileMap;
    vector<CTempString> m_ColumnNames;
    TLineMap m_LineMap;
};


END_NCBI_SCOPE


#endif
