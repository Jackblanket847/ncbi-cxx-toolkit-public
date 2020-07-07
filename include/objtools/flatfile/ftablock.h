/* ftablock.h
 *
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
 * File Name:  ftablock.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef  _BLOCK_
#define  _BLOCK_

#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Linkage_evidence.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seq/Delta_seq.hpp>

#include <objtools/flatfile/valnode.h>
#include <objtools/flatfile/fta_parser.h>

typedef std::list<ncbi::CRef<ncbi::objects::CSeq_feat> > TSeqFeatList;
typedef std::list<std::string> TAccessionList;
typedef std::list<ncbi::CRef<ncbi::objects::CSeq_id> > TSeqIdList;
typedef std::list<ncbi::CRef<ncbi::objects::COrgMod> > TOrgModList;
typedef std::vector<ncbi::CRef<ncbi::objects::CGb_qual> > TGbQualVector;
typedef std::list<ncbi::CRef<ncbi::objects::CSeqdesc> > TSeqdescList;
typedef std::vector<ncbi::CRef<ncbi::objects::CUser_object> > TUserObjVector;
typedef std::list<ncbi::CRef<ncbi::objects::CPub> > TPubList;
typedef std::list<ncbi::CRef<ncbi::objects::CSeq_loc> > TSeqLocList;
typedef std::list<ncbi::CRef<ncbi::objects::CDelta_seq> > TDeltaList;


#define ERR_ZERO             0,0

#define ParFlat_ENTRYNODE    500

#define FTA_OUTPUT_BIOSEQSET 0
#define FTA_OUTPUT_SEQSUBMIT 1

#define FTA_RELEASE_MODE     0
#define FTA_HTGS_MODE        1
#define FTA_HTGSCON_MODE     2

typedef struct file_buf {
    const char* start;
    const char* current;
} FileBuf, PNTR FileBufPtr;

typedef struct info_bioseq {
    TSeqIdList ids;                       /* for this Bioseq */
    CharPtr  locus;
    CharPtr  acnum;

    info_bioseq() :
        locus(NULL),
        acnum(NULL)
    {}

} InfoBioseq, PNTR InfoBioseqPtr;

typedef struct protein_block {
    ncbi::objects::CSeq_entry* biosep; /* for the toppest level of the BioseqSet */

    bool           segset;      /* TRUE if a BioseqSet SeqEntry */

    TEntryList     entries;     /* a ProtRef SeqEntry list, link to above
                                   biosep */

    TSeqFeatList   feats;       /* a CodeRegionPtr list to link the BioseqSet
                                   with class = nuc-prot */
    ncbi::objects::CGenetic_code::C_E gcode;         /* for this Bioseq */
    InfoBioseqPtr  ibp;
    Uint1          genome;
    Int4           orig_gcode;

    protein_block() :
        biosep(nullptr),
        segset(false),
        ibp(NULL),
        genome(0),
        orig_gcode(0)
    {}

} ProtBlk, PNTR ProtBlkPtr;

typedef struct _locus_cont {
    Int4 bases;
    Int4 bp;
    Int4 strand;
    Int4 molecule;
    Int4 topology;
    Int4 div;
    Int4 date;
} LocusCont, PNTR LocusContPtr;

typedef struct _gap_feats {
    Int4    from;
    Int4    to;
    Int4    estimated_length;
    bool    leftNs;
    bool    rightNs;
    bool    assembly_gap;
    CharPtr gap_type;
    Int4    asn_gap_type;

    ncbi::objects::CLinkage_evidence::TLinkage_evidence asn_linkage_evidence;

    struct _gap_feats PNTR next;

    _gap_feats();

} GapFeats, PNTR GapFeatsPtr;

typedef struct token_block {
    CharPtr                 str;        /* the token string */
    struct token_block PNTR next;       /* points to next token */
} TokenBlk, PNTR TokenBlkPtr;

typedef struct token_statistics_block {
    TokenBlkPtr list;                   /* a pointer points to the first
                                           token */
    Int2        num;                    /* total number of token in the
                                           chain list */
} TokenStatBlk, PNTR TokenStatBlkPtr;

typedef struct _XmlIndex {
    Int4                  tag;
    Int4                  order;
    size_t                start;        /* Offset from the beginning of the
                                           record, not file! */
    size_t                end;          /* Offset from the beginning of the
                                           record, not file! */
    Int4                  start_line;
    Int4                  end_line;
    Int2                  type;         /* Used for references */
    struct _XmlIndex PNTR subtags;
    struct _XmlIndex PNTR next;
} XmlIndex, PNTR XmlIndexPtr;

typedef std::list<std::string> TKeywordList;

typedef struct indexblk_struct {
    Char               acnum[200];      /* accession num */
    Int2               vernum;          /* version num */
    size_t             offset;          /* byte-offset of in the flatfile at
                                           which the entry starts */
    Char               locusname[200];  /* locus name */
    Char               division[4];     /* division code */
    size_t             bases;           /* basepair length of the entry */
    Uint2              segnum;          /* the number of the entry w/i a
                                           segment set */
    Uint2              segtotal;        /* total number of members in
                                           segmented set to which this
                                           entry belongs */
    Char               blocusname[200]; /* base locus name s.t. w/o tailing
                                           number */
    size_t             linenum;         /* line number at which the entry
                                           starts */
    Uint1              drop;            /* 1 if the accession should be
                                           dropped, otherwise 0 */
    size_t             len;             /* total length (or sizes in bytes)
                                           of the entry */

    ncbi::CRef<ncbi::objects::CDate_std> date; /* the record's entry-date or last
                                                  update's date */

    ncbi::CRef<ncbi::objects::CPatent_seq_id> psip; /* patent reference */

    bool               EST;             /* special EST entries */
    bool               STS;             /* special STS entries */
    bool               GSS;             /* special Genome servey entries */
    bool               HTC;             /* high throughput cDNA */
    Int2               htg;             /* special HTG [0,1,2,3] entries */
    bool               is_contig;       /* TRUE if entry has CONTIG line,
                                           otherwise FALSE */
    bool               is_mga;          /* TRUE if entry has MGA line,
                                           otherwise FALSE */
    bool               origin;          /* TRUE if sequence is present */
    bool               is_pat;          /* TRUE if accession prefix is
                                           patented and matches source.
                                           FALSE - otherwise. */
    bool               is_wgs;
    bool               is_tpa;
    bool               is_tsa;
    bool               is_tls;
    bool               is_tpa_wgs_con;  /* TRUE if "is_contig", "is_wgs" and
                                           "is_tpa" are TRUE */
    bool               tsa_allowed;
    LocusCont          lc;
    CharPtr            moltype;         /* the value of /mol_type qual */
    GapFeatsPtr        gaps;
    TokenBlkPtr        secaccs;
    XmlIndexPtr        xip;
    bool               embl_new_ID;
    bool               env_sample_qual; /* TRUE if at least one source
                                           feature has /environmental_sample
                                           qualifier */
    bool               is_prot;
    CharPtr            organism;        /* The value of /organism qualifier */
    Int4               taxid;           /* The value gotten from source feature
                                           /db_xref qualifier if any */
    bool               no_gc_warning;   /* If TRUE then suppress
                                           ERR_SERVER_GcFromSuppliedLineage
                                           WARNING message */
    size_t             qsoffset;
    size_t             qslength;
    Int4               wgs_and_gi;      /* 01 - has GI, 02 - WGS contig,
                                           03 - both above */
    bool               got_plastid;     /* Set to TRUE if there is at least
                                           one /organelle qual beginning
                                           with "plastid" */
    Char               wgssec[100];     /* Reserved buffer for WGS master or
                                           project accession as secondary */
    Int4               gc_genomic;      /* Genomic Genetic code from OrgRef */
    Int4               gc_mito;         /* Mitochondrial Genetic code */
    TKeywordList       keywords;        /* All keywords from a flat record */
    bool               assembly;        /* TRUE for TPA:assembly in
                                           KEYWORDS line */
    bool               specialist_db;   /* TRUE for TPA:specialist_db in
                                           KEYWORDS line */
    bool               inferential;     /* TRUE for TPA:inferential in
                                           KEYWORDS line */
    bool               experimental;    /* TRUE for TPA:experimental in
                                           KEYWORDS line */
    CharPtr            submitter_seqid;
    struct parser_vals *ppp;

    indexblk_struct();

} Indexblk, PNTR IndexblkPtr;

typedef struct _fta_operon {
    const Char*             featname;   /* Do not free! Just a pointer. */
    const Char*             operon;     /* Do not free! Just a pointer. */

    ncbi::CConstRef<ncbi::objects::CSeq_loc> location;   /* Do not free! Just a pointer. */

    CharPtr                 strloc;     /* String value of location. */
    bool                    operon_feat;
    bool                    ret;
    struct _fta_operon PNTR next;

    _fta_operon() :
        featname(nullptr),
        operon(nullptr),
        strloc(nullptr),
        operon_feat(false),
        ret(false),
        next(nullptr)
    {}

} FTAOperon, PNTR FTAOperonPtr;

typedef struct data_block {
    Int2                   type;        /* which keyword block or node type */
    Pointer                data;        /* any pointer type points to
                                           information block */
    CharPtr                offset;      /* points to beginning of the entry
                                           in the memory */
    size_t                 len;         /* lenght of data in bytes */
    CharPtr                qscore;      /* points to quality score buffer */
    Uint1                  drop;        /* 1 if drop this data block */
    struct data_block PNTR next;
} DataBlk, PNTR DataBlkPtr;

typedef struct entry_block {
    DataBlkPtr              chain;      /* a header points to key-word
                                           block information */
    ncbi::CRef<ncbi::objects::CSeq_entry> seq_entry; /* points to sequence entry */

    struct entry_block PNTR next;

    entry_block() :
        chain(NULL),
        next(NULL)
    {}

} EntryBlk, PNTR EntryBlkPtr;

typedef struct keyword_block {
    const char *str;
    Int2       len;
} KwordBlk, PNTR KwordBlkPtr;

/**************************************************************************/

void FreeDatablk PROTO((DataBlkPtr dbp));
void FreeEntry PROTO((DataBlkPtr entry));
void FreeIndexblk PROTO((IndexblkPtr ibp));
void GapFeatsFree PROTO((GapFeatsPtr gfp));
void XMLIndexFree PROTO((XmlIndexPtr xip));

void FreeEntryBlk PROTO((EntryBlkPtr entry));
EntryBlkPtr CreateEntryBlk PROTO(());

#endif
