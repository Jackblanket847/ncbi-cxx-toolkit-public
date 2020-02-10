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
*
* Author:  Sergiy Gotvyanskyy, NCBI
* File Description:
*   High level reader for source qualifiers 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include "src_quals.hpp"
#include "visitors.hpp"
#include <sstream>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


static void sPostError(
        ILineErrorListener* pEC,
        const string& message,
        const CTempString& seqId,
        size_t lineNum=0)
{
    _ASSERT(pEC);

    AutoPtr<CLineErrorEx> pErr(
            CLineErrorEx::Create(
                ILineError::eProblem_GeneralParsingError,
                eDiag_Error,
                0, 0, // code and subcode
                seqId,
                lineNum, // lineNumber,
                message));

    pEC->PutError(*pErr);

}

static void sReportMissingMods(
        ILineErrorListener* pEC,
        const string& fileName,
        const CBioseq& bioseq)
{

    string seqId = bioseq.GetId().front()->AsFastaString();
    string message = 
        fileName + 
        " doesn't contain qualifiers for sequence id " +
        seqId + 
       "."; 

    sPostError(pEC, message, seqId);
}


static void sReportMultipleMatches(
        ILineErrorListener* pEC,
        const string& fileName,
        size_t lineNum,
        const CBioseq& bioseq)
{
    string seqId = bioseq.GetId().front()->AsFastaString();
    ostringstream message;
    message 
        << "Multiple potential matches for line " 
        << lineNum 
        << " of " 
        << fileName
        << ". Unable to match sequence id " 
        << seqId 
        << " to a previously matched entry.";

    sPostError(pEC, message.str(), seqId);
}


static void s_PostProcessID(string& id)
{
    if (id.empty()) {
        return;
    }

    if (id.back() == '|') {
        id.pop_back();
    }
    NStr::ToLower(id);
}


static void sReportDuplicateIds(
    ILineErrorListener* pEC,
    const string& fileName,
    size_t currentLine,
    size_t previousLine,
    const CTempString& seqId)
{

    ostringstream message;
    message
        <<  "Sequence id " 
        <<  seqId 
        << " on line "  
        << currentLine
        << " of " << fileName 
        << " duplicates id on line "
        << previousLine 
        << ". Skipping line "
        << currentLine 
        << ".";

    sPostError(pEC, message.str(), seqId, currentLine);
}


static void sReportUnusedMods(
    ILineErrorListener* pEC,
    const string& fileName,
    size_t lineNum,
    const CTempString& seqId)
{
    _ASSERT(pEC);

    string message =
        fileName +
        " contains qualifiers for sequence id " +
        seqId +
        ", but no sequence with that id was found.";

    AutoPtr<CLineErrorEx> pErr(
        CLineErrorEx::Create(
            ILineError::eProblem_GeneralParsingError,
            eDiag_Error,
            0, 0, // code and subcode
            seqId,
            lineNum, // lineNumber,
            message));

    pEC->PutError(*pErr);
}


bool CMemorySrcFileMap::Empty(void) const 
{
    return m_LineMap.empty();
}


bool CMemorySrcFileMap::Mapped(void) const 
{
    return m_FileMapped;
}


bool CMemorySrcFileMap::GetMods(const CBioseq& bioseq, TModList& mods, bool isVerbose) 
{
    mods.clear();
    if (!m_FileMapped) {
        return false;
    }

    list<string> id_strings;
    for (const auto& pId : bioseq.GetId()) {
        string id;
        pId->GetLabel(&id, nullptr, CSeq_id::eFasta);
        s_PostProcessID(id);
        id_strings.push_back(id);
        CTempString type, content;
        NStr::SplitInTwo(id, "|", type, content);
        id_strings.push_back(content);
        if (pId->IsGeneral()) {
            CTempString db, tag;
            NStr::SplitInTwo(content, "|", db, tag);
            id_strings.push_back(tag);
        }
        else {
            auto pTextSeqId = pId->GetTextseq_Id();
            if (pTextSeqId && pTextSeqId->IsSetVersion()) {
                size_t pointPos = id.rfind('.');
                if (pointPos != string::npos) {
                    CTempString versionlessId(id, 0, pointPos);
                    NStr::SplitInTwo(versionlessId, "|", type, content);
                    id_strings.push_back(versionlessId);
                    id_strings.push_back(content);
                }
            }
        }
    }


    for (const auto& id : id_strings) {
        auto it = m_LineMap.find(id);
        if (it != m_LineMap.end()) {
            x_ProcessLine(it->second.line, mods);
            auto lineNum = it->second.lineNum;
            m_ProcessedIdsToLineNum.emplace(id, lineNum);
            for (const auto& pEquivIt : it->second.equiv) {
                m_LineMap.erase(pEquivIt->val);
            }
            m_LineMap.erase(it);
            return true;
        }
    }

    for (const auto& id : id_strings) {
        auto it = m_ProcessedIdsToLineNum.find(id);
        if (it != end(m_ProcessedIdsToLineNum)) {
            sReportMultipleMatches(m_pEC, m_pFileMap->GetFileName(), it->second, bioseq);
            return false;
        }
    }

    if (isVerbose) {
        sReportMissingMods(m_pEC, m_pFileMap->GetFileName(), bioseq);
    }
    return false;
}


void CMemorySrcFileMap::ReportUnusedIds()
{
    if (!Empty()) {
        map<size_t, CTempString> unusedLines;
        for (const auto& entry : m_LineMap) { 
            unusedLines.emplace(entry.second.lineNum, entry.second.line);    
        }

        for (const auto& entry : unusedLines) {
            CTempString seqId, remainder;
            NStr::SplitInTwo(entry.second, "\t", seqId, remainder);
            sReportUnusedMods(m_pEC, 
                    m_pFileMap->GetFileName(), 
                    entry.first, 
                    NStr::TruncateSpaces_Unsafe(seqId));
        }
    }
}

void CMemorySrcFileMap::x_ProcessLine(const CTempString& line, TModList& mods)
{
    vector<CTempString> tokens;
    NStr::Split(line, "\t", tokens, 0);
    for (size_t i=1; i < tokens.size() && i < m_ColumnNames.size(); ++i) {
        auto value=NStr::TruncateSpaces_Unsafe(tokens[i]);
        if (!NStr::IsBlank(value)) {
            mods.emplace_back(m_ColumnNames[i], value);
        }
    }
}


void CMemorySrcFileMap::x_RegisterLine(size_t lineNum, CTempString line, bool allowAcc)
{
    CTempString idString, remainder;
    NStr::SplitInTwo(line, "\t", idString, remainder);
    NStr::TruncateSpacesInPlace(idString);

    if (!idString.empty()) {
        auto parseFlags =
            allowAcc ?
            CSeq_id::fParse_AnyRaw | CSeq_id::fParse_ValidLocal :
            CSeq_id::fParse_AnyLocal;

        list<CRef<CSeq_id>> ids;
        try {
            CSeq_id::ParseIDs(ids, idString, parseFlags);
        }
        catch (const CSeqIdException& e) {
            sPostError(m_pEC,
                    "In " + m_pFileMap->GetFileName() +
                    ". Unable to parse " + idString + ".",
                    "",
                    lineNum);
            return;
        }


        set<string> idSet;
        if (ids.size() == 1 &&
            ids.front()->IsLocal() &&
            !NStr::StartsWith(idString, "lcl|", NStr::eNocase)) {
            string idKey = idString;
            s_PostProcessID(idKey);
            idSet.emplace(idKey);
        }
        else {
            for (const auto& pSeqId : ids) {
                string idKey;
                pSeqId->GetLabel(&idKey, nullptr, CSeq_id::eFasta);
                s_PostProcessID(idKey);
                idSet.emplace(idKey);
            }
        }


        list<TLineMap::iterator> iterators;
        for (auto idKey : idSet) {
            auto rval = m_LineMap.emplace(idKey, SLineInfo{lineNum, line});
            if (!rval.second) {
                CTempString seqId, remainder; // revisit this
                NStr::SplitInTwo(line, "\t", seqId, remainder);
                sReportDuplicateIds(m_pEC, 
                    m_pFileMap->GetFileName(),
                    lineNum,
                    rval.first->second.lineNum,
                    NStr::TruncateSpaces_Unsafe(seqId));
                for (auto it : iterators) {
                    m_LineMap.erase(it);
                }
                break;
            }
            iterators.push_back(rval.first);
        }

        if (iterators.size()>1) {
            for (auto current_it : iterators) {
                for(auto other_it : iterators) {
                    if (other_it != current_it) {
                        current_it->second.equiv.emplace_back(new SIter(other_it));
                    }
                }
            }
        }
    }
}



void CMemorySrcFileMap::MapFile(const string& fileName, bool allowAcc)
{
    if (m_FileMapped || 
        m_pFileMap ||
        !m_LineMap.empty()) {
        return;
    }

    m_pFileMap.reset(new CMemoryFileMap(fileName));

    size_t fileSize = m_pFileMap->GetFileSize();
    const char* ptr = (const char*)m_pFileMap->Map(0, fileSize);
    const char* end = ptr + fileSize;

    size_t lineNum = 0;
    while (ptr < end)
    {
        // search for next non empty line
        if (*ptr == '\r' || *ptr == '\n') {
            ++ptr;
            continue;
        }

        const char* start = ptr;
        // search for end of line
        const char* endline = (const char*)memchr(ptr, '\n', end - ptr);
        if (endline == nullptr) endline = end;

        ptr = endline + 1;
        endline--;

        while (start < endline && *endline == '\r')
            endline--;

        // compose line control structure
        if (start < endline)
        {
            ++lineNum;
            CTempString line(start, endline-start+1);
            if (m_ColumnNames.empty())
                NStr::Split(line, "\t", m_ColumnNames, 0);
            else // parse regular line
                x_RegisterLine(lineNum, line, allowAcc);
        }
    }

    if (m_ColumnNames.empty()) {
        NCBI_THROW(CArgException, eConstraint,
        "source modifiers file header line is not valid");
    }

    m_FileMapped = true;
}






static void s_AppendMods(
    const CModHandler::TModList& mods,
    string& title)
{
    for (const auto& mod : mods) {

        title.append(" ["
                + mod.GetName()
                + "="
                + mod.GetValue()
                + "]");
    }
}


static void sReportError(
        ILineErrorListener* pEC,
        EDiagSev severity,
        int subcode,
        const string& seqId,
        const string& message,
        ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    _ASSERT(pEC);

    AutoPtr<CLineErrorEx> pErr(
            CLineErrorEx::Create(
                problemType,
                severity,
                EReaderCode::eReader_Mods,
                subcode,
                seqId,
                0, // lineNumber,
                message));

    pEC->PutError(*pErr);
}



static void s_PreprocessNoteMods(CModHandler::TModList& mods)
{
    for (auto& mod : mods) {
        if (CModHandler::GetCanonicalName(mod.GetName()) == "note"){
            string new_value = mod.GetValue();
            NStr::ReplaceInPlace(new_value, "<", "[");
            NStr::ReplaceInPlace(new_value, ">", "]");
            mod.SetValue(new_value);
        }
    }
}


class CApplyMods
{
public:
    using TModList = CModHandler::TModList;
    using TMods = CModHandler::TMods;

    CApplyMods(const TMods& commandLineMods, 
               const string& m_CommandLineRemainder,
               CMemorySrcFileMap* pNamedSrcFileMap,
               CMemorySrcFileMap* pDefaultSrcFileMap,
               ILineErrorListener* pMessageListener,
               bool readModsFromTitle,
               bool isVerbose);

    void operator()(CBioseq& bioseq);

private:
    TMods m_CommandLineMods;
    const string m_CommandLineRemainder;

    CMemorySrcFileMap* m_pNamedSrcFileMap=nullptr;
    CMemorySrcFileMap* m_pDefaultSrcFileMap=nullptr;
    ILineErrorListener* m_pMessageListener=nullptr;

    bool m_ReadModsFromTitle = false;
    bool m_IsVerbose=false; // can set this in CMemorySrcFileMap
};


CApplyMods::CApplyMods(
        const TMods& commandLineMods,
        const string& commandLineRemainder,
        CMemorySrcFileMap* pNamedSrcFileMap,
        CMemorySrcFileMap* pDefaultSrcFileMap,
        ILineErrorListener* pMessageListener,
        bool readModsFromTitle,
        bool isVerbose) : 
    m_CommandLineMods(commandLineMods), 
    m_CommandLineRemainder(commandLineRemainder),
    m_pNamedSrcFileMap(pNamedSrcFileMap),
    m_pDefaultSrcFileMap(pDefaultSrcFileMap),
    m_pMessageListener(pMessageListener),
    m_ReadModsFromTitle(readModsFromTitle),
    m_IsVerbose(isVerbose)
{}



static void s_GetModsFromFileMap(
        CMemorySrcFileMap& fileMap,
        const CBioseq& bioseq,
        bool isVerbose,
        CModHandler::FReportError fReportError,
        CModHandler& mod_handler,
        string& remainder)
{
    CApplyMods::TModList mods;
    if (!fileMap.GetMods(bioseq, mods, isVerbose)) {
        return;
    }
        
    s_PreprocessNoteMods(mods); // RW-928
    CApplyMods::TModList rejectedMods;

    mod_handler.AddMods(mods,
            CModHandler::ePreserve,
            rejectedMods,
            fReportError);
    s_AppendMods(rejectedMods, remainder);
}

    
void CApplyMods::operator()(CBioseq& bioseq)
{
    CModHandler mod_handler;
    mod_handler.SetMods(m_CommandLineMods);
    string remainder = m_CommandLineRemainder;
    TModList rejectedMods;

    string seqId = bioseq.GetId().front()->AsFastaString();
    auto fReportError = 
        [&](const CModData& /* mod */, const string& msg, EDiagSev /* sev */,
            EModSubcode subcode) {
            return sReportError(m_pMessageListener, eDiag_Warning, subcode, seqId, msg);
        };
 
    if (m_pNamedSrcFileMap && m_pNamedSrcFileMap->Mapped()) {
        s_GetModsFromFileMap(
                *m_pNamedSrcFileMap,
                bioseq,
                m_IsVerbose,
                fReportError,
                mod_handler,
                remainder);
    }

    if (m_pDefaultSrcFileMap && m_pDefaultSrcFileMap->Mapped()) {
        s_GetModsFromFileMap(
                *m_pDefaultSrcFileMap,
                bioseq,
                m_IsVerbose,
                fReportError,
                mod_handler,
                remainder);
    }

    CRef<CSeqdesc> pTitleDesc;
    CSeq_descr::Tdata* pDescriptors = nullptr;
    if ((bioseq.IsSetDescr() &&
        bioseq.GetDescr().IsSet()) ||
        !NStr::IsBlank(remainder)) {
        pDescriptors = &(bioseq.SetDescr().Set());
    }

    CSeq_descr::Tdata::iterator title_it;
    if (pDescriptors) {
        title_it = 
            find_if(pDescriptors->begin(), pDescriptors->end(),
                    [](CRef<CSeqdesc> pDesc) { return pDesc->IsTitle(); });
        if (title_it != pDescriptors->end()) {
            pTitleDesc = *title_it;
            if (m_ReadModsFromTitle) { 
                auto& title = (*title_it)->SetTitle();
                string titleRemainder;
                TModList mods;
                CTitleParser::Apply(title, mods, titleRemainder);
                title.clear(); 
                mod_handler.AddMods(mods, 
                    CModHandler::ePreserve, 
                    rejectedMods, 
                    fReportError);
                s_AppendMods(rejectedMods, titleRemainder);
                remainder = titleRemainder +  remainder;
            }
        }
    }


    CModAdder::Apply(mod_handler, bioseq, rejectedMods, fReportError);
    s_AppendMods(rejectedMods, remainder);


    NStr::TruncateSpacesInPlace(remainder);
    if (!remainder.empty()) {
        if (!pTitleDesc) {
            pTitleDesc = Ref(new CSeqdesc());
            pDescriptors->push_back(pTitleDesc);
            pTitleDesc->SetTitle() = remainder;
        }
        else {
            string current_title = 
                NStr::TruncateSpaces(
                    pTitleDesc->GetTitle(), 
                    NStr::eTrunc_End);
            pTitleDesc->SetTitle() = current_title.empty() ?
                remainder : 
                current_title + " " + remainder;
        }
    }
    else // remainder.empty() 
    if (pDescriptors) {
        if (title_it != pDescriptors->end() &&
            (*title_it)->GetTitle().empty()) {
            pDescriptors->erase(title_it);
        }
        
        if (pDescriptors->empty()) {
            bioseq.ResetDescr();
        }
    }
}


void g_ApplyMods(
    unique_ptr<CMemorySrcFileMap>& pNamedSrcFileMap,
    const string& namedSrcFile,
    const string& defaultSrcFile,
    const string& commandLineStr,
    bool readModsFromTitle,
    bool allowAcc,
    bool isVerbose,
    ILineErrorListener* pEC,
    CSeq_entry& entry)
{
    using TModList = CModHandler::TModList;
    using TMods = CModHandler::TMods;
    
    string commandLineRemainder;
    TMods commandLineMods;

    if (!NStr::IsBlank(commandLineStr)) {
        TModList mods;
        CTitleParser::Apply(commandLineStr, mods, commandLineRemainder);
        s_PreprocessNoteMods(mods); // RW-928

        auto fReportCommandLineError = 
            [&](const CModData& /* mod */, const string& msg, EDiagSev /* sev */,
                EModSubcode subcode) {
                return sReportError(pEC, eDiag_Warning, subcode, "", msg);
            };

        TModList rejectedMods;
        CModHandler mod_handler;
        mod_handler.AddMods(mods,
            CModHandler::ePreserve,
            rejectedMods, 
            fReportCommandLineError);
        s_AppendMods(rejectedMods, commandLineRemainder);
        commandLineMods = mod_handler.GetMods();
    }


    if (!NStr::IsBlank(namedSrcFile) && CFile(namedSrcFile).Exists()) {
        if (!pNamedSrcFileMap)
            pNamedSrcFileMap.reset(new CMemorySrcFileMap(pEC));
        pNamedSrcFileMap->MapFile(namedSrcFile, allowAcc);
    }

    unique_ptr<CMemorySrcFileMap> pDefaultSrcFileMap; 
    if (!NStr::IsBlank(defaultSrcFile) && CFile(defaultSrcFile).Exists()) {
        pDefaultSrcFileMap.reset(new CMemorySrcFileMap(pEC));
        pDefaultSrcFileMap->MapFile(defaultSrcFile, allowAcc);
    }

    CApplyMods applyMods(commandLineMods, 
                         commandLineRemainder,
                         pNamedSrcFileMap.get(),
                         pDefaultSrcFileMap.get(),
                         pEC,
                         readModsFromTitle,
                         isVerbose);

    VisitAllBioseqs(entry, applyMods);

    if (isVerbose && pDefaultSrcFileMap) {
        pDefaultSrcFileMap->ReportUnusedIds();
    }
}

END_NCBI_SCOPE

