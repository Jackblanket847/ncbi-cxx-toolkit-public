/* 
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
 *  Please cite the authors in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Lewis Y. Geer
 *
 * File Description:
 *    code to do mass spec scoring
 *
 * ===========================================================================
 */

//
//
// 
// Please keep this file clear of NCBI toolkit dependent code as it
// is meant to be used externally from the toolkit.
// 
// NCBI dependent code can go into omssascore.hpp
// 
// 
// 


#ifndef MSSCORE__HPP
#define MSSCORE__HPP

#include <vector>
#include <map>

// ncbi toolkit specific define for dll export
#ifndef NCBI_XOMSSA_EXPORT
#define NCBI_XOMSSA_EXPORT
#endif

/**
 * enumeration of ion series
 * 
 * should overlap with EMSIonType in omssa object loaders
 *  
 */

enum EMSIonSeries {
    eMSIonTypeA,
    eMSIonTypeB,
    eMSIonTypeC,
    eMSIonTypeX,
    eMSIonTypeY,
    eMSIonTypeZ,
    eMSIonTypeUnknown,
    eMSIonTypeErr,
    eMSIonTypeMax
};

/** set of ion series specifiers */
typedef vector <EMSIonSeries> TIonSeriesSet;


/** 
 * minimal information about a peak
 */
class NCBI_XOMSSA_EXPORT CMSBasicPeak {
public:

    CMSBasicPeak(void);

    /** return the intensity of the peak */
    const TMSIntensity GetIntensity(void) const;

    /** set the intensity of the peak */
    TMSIntensity& SetIntensity(void);

    /** get the m/z value of the peak */
    const TMSMZ GetMZ(void) const;

    /** set the m/z value of the peak */
    TMSMZ& SetMZ(void);

private:

    /** intensity */
    TMSIntensity Intensity;

    /** m/z */
    TMSMZ MZ;
};

inline
CMSBasicPeak::CMSBasicPeak(void):
    Intensity(0),
    MZ(0)
{
}

inline
const TMSIntensity CMSBasicPeak::GetIntensity(void) const
{
    return Intensity;
}

inline
TMSIntensity& CMSBasicPeak::SetIntensity(void)
{
    return Intensity;
}

inline
const TMSMZ CMSBasicPeak::GetMZ(void) const
{
    return MZ;
}

inline
TMSMZ& CMSBasicPeak::SetMZ(void)
{
    return MZ;
}


/**
 * information about a matched peak.  
 * Intended for use in omssa hitlist
 */
class NCBI_XOMSSA_EXPORT CMSBasicMatchedPeak: public CMSBasicPeak {
public:

    CMSBasicMatchedPeak(void);

    /**
     * copy constructor from basic class
     */
    CMSBasicMatchedPeak(const CMSBasicPeak& BasicPeak);

    /** Get the ion type */
    const TMSIonSeries GetIonSeries(void) const;

    /** Set the ion type */
    TMSIonSeries& SetIonSeries(void);

    /** Get the ion charge */
    const TMSCharge GetCharge(void) const;

    /** Set the ion charge */
    TMSCharge& SetCharge(void);

    /** Get the ion series number */
    const TMSNumber GetNumber(void) const;

    /** Get the ion series number */
    TMSNumber& SetNumber(void);

private:
    TMSCharge Charge;
    TMSIonSeries Series;
    TMSNumber Number;
};

inline
CMSBasicMatchedPeak::CMSBasicMatchedPeak(void):
    Charge(-1),
    Series(eMSIonTypeUnknown),
    Number(-1)
{
}

inline
CMSBasicMatchedPeak::CMSBasicMatchedPeak(const CMSBasicPeak& BasicPeak):
    Charge(-1),
    Series(eMSIonTypeUnknown),
    Number(-1)
{
    SetMZ() = BasicPeak.GetMZ();
    SetIntensity() = BasicPeak.GetIntensity();
}

inline
const TMSIonSeries CMSBasicMatchedPeak::GetIonSeries(void) const
{
    return Series;
}

inline
TMSIonSeries& CMSBasicMatchedPeak::SetIonSeries(void)
{
    return Series;
}

inline
const TMSCharge CMSBasicMatchedPeak::GetCharge(void) const
{
    return Charge;
}

inline
TMSCharge& CMSBasicMatchedPeak::SetCharge(void)
{
    return Charge;
}

inline
const TMSNumber CMSBasicMatchedPeak::GetNumber(void) const
{
    return Number;
}

inline
TMSNumber& CMSBasicMatchedPeak::SetNumber(void)
{
    return Number;
}



/**
 * enumeration to denote the type of match
 */
enum EMSMatchType {
    eMSMatchTypeUnknown, /**< unknown if there is a match or not, i.e. default state */
    eMSMatchTypeNotTyped, /**< there is a match but the type is unknown */
    eMSMatchTypeIndependent, /**< the match is statistically independent */
    eMSMatchTypeSemiIndependent, /**< the match is independent to first order */
    eMSMatchTypeDependent, /**< the match is statistically dependent */
    eMSMatchTypeNoSearch, /**< the peak was not searched, e.g. b1 */
    eMSMatchTypeNoMatch   /**< peak was searched, but no match */
};

/**
 * holds match of a spectrum peak to a theoretical m/z value
 */
class NCBI_XOMSSA_EXPORT CMSMatchedPeak : public CMSBasicMatchedPeak {
public:
    CMSMatchedPeak(void);

    virtual ~CMSMatchedPeak() {}

    /** virtual assignment operator */
    virtual void Assign(CMSMatchedPeak *in);

    /** fill in using base class */
    void Assign(CMSBasicMatchedPeak *in);

    /** Get the experimental mass tolerance of the peak */
    const TMSMZ GetMassTolerance(void) const;

    /** Set the experimental mass tolerance of the peak */
    TMSMZ& SetMassTolerance(void);

    /** Get the exp ions per unit mass */
    const TMSExpIons GetExpIons() const;

    /** Set the exp ions per unit mass */
    TMSExpIons& SetExpIons();

    /** Get the type of match */
    const EMSMatchType GetMatchType(void) const;

    /** Set the type of match */
    EMSMatchType& SetMatchType(void);

private:
    TMSMZ MassTolerance;
    TMSExpIons ExpIons;
    EMSMatchType MatchType;
};


inline
const TMSMZ CMSMatchedPeak::GetMassTolerance(void) const
{
    return MassTolerance;
}

inline
TMSMZ& CMSMatchedPeak::SetMassTolerance(void)
{
    return MassTolerance;
}

inline
const TMSExpIons CMSMatchedPeak::GetExpIons(void) const
{
    return ExpIons;
}

inline
TMSExpIons& CMSMatchedPeak::SetExpIons(void)
{
    return ExpIons;
}

inline
const EMSMatchType CMSMatchedPeak::GetMatchType(void) const
{
    return MatchType;
}

inline
EMSMatchType& CMSMatchedPeak::SetMatchType(void)
{
    return MatchType;
}

/** container typedef for a set of matches */
typedef std::vector <CMSMatchedPeak *> TMatchedPeakSet;

/**
 * container for a set of matches
 */
class CMSMatchedPeakSet {
public:
    CMSMatchedPeakSet(void);
    virtual ~CMSMatchedPeakSet();

    /** 
     * Get the match info 
     */
    const TMatchedPeakSet& GetMatchedPeakSet(void) const;

    /** 
     * Set the match info
     */
    TMatchedPeakSet& SetMatchedPeakSet(void);

    /**
     *  initialize the MatchInfo array
     *  delete any existing array
     * 
     * @param size number of peaks
     */
    virtual void CreateMatchedPeakSet(int Size);

    /**
     * delete the MatchInfo array
     */
    void DeleteMatchedPeakSet(void);


private:
    /**
     * list of matched peak info
     */
    TMatchedPeakSet MatchedPeakSet;

};

inline
const TMatchedPeakSet& CMSMatchedPeakSet::GetMatchedPeakSet(void) const
{
    return MatchedPeakSet;
}

inline
TMatchedPeakSet& CMSMatchedPeakSet::SetMatchedPeakSet(void)
{
    return MatchedPeakSet;
}


/** map from an ion series to a set of matched peak sets */
typedef std::map <int, CMSMatchedPeakSet *> TIonSeriesMatchMap;

/** 
 * contains a map of charge and series to a set of matches
 */
class CMSMatchedPeakSetMap {
public:
    /** c'tor */
    CMSMatchedPeakSetMap(void);

    /** d'tor */
    ~CMSMatchedPeakSetMap();

    /**
     * create a new ion series
     * 
     * @param Charge charge of ion series
     * @param Series which ion series
     * 
     */
    CMSMatchedPeakSet * CreateSeries(
        TMSCharge Charge,
        TMSIonSeries Series,
        int Size);

    /** 
     * get a series for modification
     * 
     * @param Charge charge of ion series
     * @param Series which ion series
     */
    CMSMatchedPeakSet * SetSeries(
        TMSCharge Charge, 
        TMSIonSeries Series);

    /** Get the Map */
    const TIonSeriesMatchMap& GetMatchMap(void) const;

    /** Set the Map */
    TIonSeriesMatchMap& SetMatchMap(void);

private:
    /** the map itself */
    TIonSeriesMatchMap MatchMap;

    /** convert a charge and series to a map key */
    const int ChargeSeries2Key(TMSCharge Charge, TMSIonSeries Series) const;
};


inline
const TIonSeriesMatchMap& CMSMatchedPeakSetMap::GetMatchMap(void) const
{
    return MatchMap;
}

inline
TIonSeriesMatchMap& CMSMatchedPeakSetMap::SetMatchMap(void)
{
    return MatchMap;
}

/**
 * container for match between a spectrum and a mass ladder
 * 
 * base class for omssa CMSHit
 */
class NCBI_XOMSSA_EXPORT CMSSpectrumMatch {
public:

    /** c'tor */
    CMSSpectrumMatch(void);

    /** 
     * d'tor
     * 
     * deletes the HitInfo Array
     */
    virtual ~CMSSpectrumMatch();

    /** Get the experimental m/z of the spectrum */
    const TMSMZ GetExpMass(void) const;

    /** Set the experimental mass of the spectrum */
    TMSMZ& SetExpMass(void);

    /** return theoretical mass of the hit */
    const TMSMZ GetTheoreticalMass(void) const;

    /** set the theoretical mass of the hit */
    TMSMZ& SetTheoreticalMass(void);

    /** get the charge */
    const TMSCharge GetCharge(void) const;

    /** set the charge */
    TMSCharge& SetCharge();

    /** Get the sum of ranks */
    const int GetSum(void) const; 

    /** Set the sum of ranks */
    int& SetSum(void); 

    /** Get the number of matched peaks */
    const int GetM(void) const;

    /** Set the number of matched peaks */
    int& SetM(void);

    /** Get the number of experimental peaks */
    const int GetN(void) const;

    /** Set the number of experimental peaks */
    int& SetN(void);

    /** return the size of the HitInfo array */
    const int GetHits(void) const;

    /** set the size of the HitInfo array */
    int& SetHits(void); 

    /** 
     * Get the hit info at array position n
     * 
     * @param n array position
     */
    const CMSBasicMatchedPeak& GetHitInfo(int n) const;

    /** 
     * Set the hit info at array position n
     * 
     * note: there is an implicit assumption in the scoring code
     * that ions of the same species are grouped together
     * and that the ions within the species are in order
     * 
     * @param n array position
     */
    CMSBasicMatchedPeak& SetHitInfo(int n);

    /**
     *  initialize the HitInfo array
     *  delete any existing array
     *  size is determined by GetHits()
     * 
     */
    void CreateHitInfo(void);

    /**
     * find a peak within HitInfo
     * 
     * @param Number number of the peak, starting with 0
     * @param Charge charge of peak
     * @param Series the series to search
     * @return found peak (0 if not found)
     */
    CMSBasicMatchedPeak * Find(TMSNumber Number, TMSCharge Charge, TMSIonSeries Series);

    /** assignment operator */
//    CMSSpectrumMatch& operator= (CMSSpectrumMatch& in);

    /**
     * get map from ion series to CMSMatchedPeakSet *
     */
    const CMSMatchedPeakSetMap& GetIonSeriesMatchMap(void) const;

    /**
     * Set map from ion series to CMSMatchedPeakSet *
     */
    CMSMatchedPeakSetMap& SetIonSeriesMatchMap(void);

    /** 
     * copies hit array into match array
     * fills in missing peaks
     * does not fill in exp peak values.  This has to be done by the calling algorithm.
     * 
     * @param Charge charge of the ion series to fill
     * @param Series ion series
     * @param size length of the ion series
     * @param MinIntensity the minimum intensity of the peak to consider it as a match
     * @param MH the neutral mass of the precursor
     */
    void FillMatchedPeaks(
        TMSCharge Charge, 
        TMSIonSeries Series, 
        int Size, 
        TMSIntensity MinIntensity, 
        TMSMZ MH);

    /**
     * calculate the mean value of the poisson distribution for this match
     */
    const double CalcPoissonMean(void) const;

    /**
     * calulate the poisson distribution
     * 
     * @param Mean mean value of poisson
     * @param i counts
     * @return value of poisson at i
     */
    const double CalcPoisson(double Mean, int i) const;

    /**
     * calculates the poisson times the top n hit probability
     * 
     * @param Mean mean value of poisson
     * @param i counts
     * @param TopHitProb probability for top n hits
     * @return value of poisson times top hit prob at i
     */
    const double CalcPoissonTopHit(double Mean, int i, double TopHitProb) const;

    /**
     * calculate the p-value using poisson distribution
     * 
     * @param Mean mean value of poisson
     * @param Hits number of hits
     * @return p-value
     */
    const double CalcPvalue(double Mean, int Hits) const;

    /**
     * integrate CalcPoissonTopHit over all i
     * 
     * @param Mean mean value of poisson
     * @param TopHitProb probability for top n hits
     * @return integral
     */
    const double CalcNormalTopHit(double Mean, double TopHitProb) const;

    /**
     * calcualte the p-value using poisson distribution and the top hit prob
     * 
     * @param Mean mean value of poisson
     * @param Hits number of hits
     * @param Normal the integral of the distribution, used to normalize
     * @param TopHitProb the probability of a top n hit.
     * @return p-value
     */
    const double CalcPvalueTopHit(double Mean, int Hits, double Normal, double TopHitProb) const;

private:
    // disallow copy
    CMSSpectrumMatch(const CMSSpectrumMatch& in) {}

    /**
     * list of matched peaks
     * ideally, this would be an array of pointers to 
     * allow each element to be virtual, but efficiency requires that it
     * be an array. this assertion should be tested.
     */
    CMSBasicMatchedPeak* HitInfo;

    /** size of HitInfo array */
    int Hits;

    /** the experimental mass */
    TMSMZ ExpMass;

    /** theoretical mass */
    TMSMZ TheoreticalMass;

    /** the charge of the hit */
    TMSCharge Charge;

    /** Sum of Ranks */
    int Sum;

    /** Number of matched peaks */
    int M;

    /** Number of exp peaks */
    int N;

    /** map from ion series to matched peak sets */
    CMSMatchedPeakSetMap IonSeriesMatchMap;
};

inline
const TMSMZ CMSSpectrumMatch::GetExpMass(void) const
{
    return ExpMass;
}

inline
TMSMZ& CMSSpectrumMatch::SetExpMass(void)
{
    return ExpMass;
}

inline
const TMSMZ CMSSpectrumMatch::GetTheoreticalMass(void) const
{
    return TheoreticalMass;
}

inline
TMSMZ& CMSSpectrumMatch::SetTheoreticalMass(void)
{
    return TheoreticalMass;
}

inline
const TMSCharge CMSSpectrumMatch::GetCharge(void) const
{
    return Charge;
}

inline
TMSCharge& CMSSpectrumMatch::SetCharge()
{
    return Charge;
}

inline
const int CMSSpectrumMatch::GetSum(void) const
{
    return Sum;
}

inline
int& CMSSpectrumMatch::SetSum(void)
{
    return Sum;
}

inline
const int CMSSpectrumMatch::GetM(void) const
{
    return M;
}

inline
int& CMSSpectrumMatch::SetM(void)
{
    return M;
}

inline
const int CMSSpectrumMatch::GetN(void) const
{
    return N;
}

inline
int& CMSSpectrumMatch::SetN(void)
{
    return N;
}

inline 
const int CMSSpectrumMatch::GetHits(void) const
{
    return Hits;
}

inline 
int& CMSSpectrumMatch::SetHits(void) 
{ 
    return Hits;
}

inline
const CMSBasicMatchedPeak& CMSSpectrumMatch::GetHitInfo(int n) const
{
    return HitInfo[n];
}

inline
CMSBasicMatchedPeak& CMSSpectrumMatch::SetHitInfo(int n)
{
    return HitInfo[n];
}

inline
const CMSMatchedPeakSetMap& CMSSpectrumMatch::GetIonSeriesMatchMap(void) const
{
    return IonSeriesMatchMap;
}

inline
CMSMatchedPeakSetMap& CMSSpectrumMatch::SetIonSeriesMatchMap(void)
{
    return IonSeriesMatchMap;
}


#if 0
inline 
CMSSpectrumMatch& CMSSpectrumMatch::operator= (CMSSpectrumMatch& in) 
{ 
    // handle self assignment
    if(this == &in) return *this;

    SetExpMass() = in.GetExpMass();
    SetHits() = in.GetHits();
    SetCharge() = in.GetCharge();
    SetTheoreticalMass() = in.GetTheoreticalMass();
    SetSum() = in.GetSum();
    SetM() = in.GetM();
    SetN() = in.GetN();

    int i;
    if(in.HitInfo) {
        CreateHitInfo();
        for(i = 0; i < GetHits(); i++) 
            SetHitInfo(i) = in.GetHitInfo(i);
    }
    if(in.MatchInfo) {
         CreateMatchInfo();
         for(i = 0; i < GetHits(); i++) 
             SetMatchInfo(i) = in.GetMatchInfo(i);
     }
    return *this;
}
#endif



#endif

