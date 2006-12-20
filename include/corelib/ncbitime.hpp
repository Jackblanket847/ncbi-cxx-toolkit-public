#ifndef CORELIB__NCBITIME__HPP
#define CORELIB__NCBITIME__HPP

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
 * Authors:  Anton Butanayev, Denis Vakatov, Vladimir Ivanov
 *
 * DayOfWeek():  Used code has been posted on comp.lang.c on March 10th, 1993
 *               by Tomohiko Sakamoto (sakamoto@sm.sony.co.jp).
 *
 */

/// @file ncbitime.hpp
/// Defines:
///   CTime      - standard Date/Time class to represent an absolute time.
///   CTimeSpan  - class to represents a relative time span.
///   CStopWatch - stop watch class to measure elasped time.
///
/// NOTE about CTime:
///   - Do not use Local time and time_t and its dependent functions with
///     dates outside range January 1, 1970 to January 18, 2038.
///     Also avoid to use GMT -> Local time conversion functions.
///
///   - Do not use DataBase conversion functions with dates
///     less than January 1, 1900.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbitype.h>
#include <time.h>


BEGIN_NCBI_SCOPE

/** @addtogroup Time
 *
 * @{
 */

// Forward declarations
class CTimeSpan;
class CFastLocalTime;


/// Number of nanoseconds in one second.
///
/// Interval for it is from 0 to 999,999,999.
const long kNanoSecondsPerSecond = 1000000000;

/// Number of microseconds in one second.
///
/// Interval for it is from 0 to 999,999.
const long kMicroSecondsPerSecond = 1000000;

/// Number milliseconds in one second.
///
/// Interval for it is from 0 to 999.
const long kMilliSecondsPerSecond = 1000;


// Time formats in databases (always contain local time only!)

/// Number of seconds.
typedef Int8 TSeconds;

/// Database format for time where day and time is unsigned 16 bit.
typedef struct {
    Uint2 days;   ///< Days from 1/1/1900
    Uint2 time;   ///< Minutes from begin of current day
} TDBTimeU, *TDBTimeUPtr;

/// Database format for time where day and time is signed 32 bit.
typedef struct {
    Int4  days;   ///< days from 1/1/1900
    Int4  time;   ///< x/300 seconds from begin of current day
} TDBTimeI, *TDBTimeIPtr;


/////////////////////////////////////////////////////////////////////////////
///
/// CTime --
///
/// Defines a standard Date/Time class.
///
/// Can be used to span time (to represent elapsed time). Can operate with
/// local and GMT (aka UTC) time. The time is kept in class in the format
/// in which it was originally given.
///
/// Throw exception of type CTimeException on errors.
///
/// NOTE: Do not use local time with time span and dates < "1/1/1900"
/// (use GMT time only!!!).

class NCBI_XNCBI_EXPORT CTime
{
public:
    /// Which initial value to use for time.
    enum EInitMode {
        eCurrent,     ///< Use current time
        eEmpty        ///< Use "empty" time
    };

    /// Which initial value to use for timezone.
    enum ETimeZone {
        eLocal,       ///< Use local time
        eGmt          ///< Use GMT (Greenwich Mean Time)
    };

    /// Current timezone. Used in AsString() method.
    enum {
        eCurrentTimeZone = -1
    };

    /// Which format use to get name of month or week of day.
    enum ENameFormat {
        eFull,        ///< Use full name.
        eAbbr         ///< Use abbreviated name.
    };

    /// Month names.
    enum EMonth {
        eJanuary = 1,
        eFebruary,
        eMarch,
        eApril,
        eMay,
        eJune,
        eJuly,
        eAugust,
        eSeptember,
        eOctober,
        eNovember,
        eDecember
    };

    /// Day of week names.
    enum EDayOfWeek {
        eSunday = 0,
        eMonday,
        eTuesday,
        eWednesday,
        eThursday,
        eFriday,
        eSaturday
    };

    /// What time zone precision to use for adjusting daylight saving time.
    ///
    /// Controls when (if ever) to adjust for the daylight saving time
    /// (only if the time is represented in local timezone format).
    ///
    /// NOTE: if diff between previous time value and the time
    /// after manipulation is greater than this range, then try apply
    /// daylight saving conversion on the result time value.
    enum ETimeZonePrecision {
        eNone,    ///< Daylight saving not to affect time manipulations.
        eMinute,  ///< Check condition - new minute.
        eHour,    ///< Check condition - new hour.
        eDay,     ///< Check condition - new day.
        eMonth,   ///< Check condition - new month.
        eTZPrecisionDefault = eNone
    };

    /// Whether to adjust for daylight saving time.
    enum EDaylight {
        eIgnoreDaylight,   ///< Ignore daylight saving time.
        eAdjustDaylight,   ///< Adjust for daylight saving time.
        eDaylightDefault = eAdjustDaylight
    };

    /// Constructor.
    ///
    /// @param mode
    ///   Whether to build time object with current time or empty
    ///   time (default).
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(EInitMode          mode = eEmpty,
          ETimeZone          tz   = eLocal,
          ETimeZonePrecision tzp  = eTZPrecisionDefault);

    /// Explicit conversion constructor for time_t representation of time.
    ///
    /// Construct time object from GMT time_t value.
    /// The constructed object will be in the eGMT format.
    ///
    /// @param t
    ///   Time in the GMT time_t format.
    /// @param tzp
    ///   What time zone precision to use.
    explicit CTime(time_t t, ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Constructor.
    ///
    /// Construct time given the year, month, day, hour, minute, second,
    /// nanosecond parts of a time value.
    ///
    /// @param year
    ///   Year part of time.
    /// @param month
    ///   Month part of time. Note month starts from 1.
    /// @param day
    ///   Day part of time. Note day starts from 1.
    /// @param hour
    ///   Hour part of time.
    /// @param minute
    ///   Minute part of time.
    /// @param second
    ///   Second part of time.
    /// @param nanosecond
    ///   Nanosecond part of time.
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(int year, int month, int day,
          int hour = 0, int minute = 0, int second = 0, long nanosecond = 0,
          ETimeZone tz = eLocal,
          ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Constructor.
    ///
    /// Construct date as N-th day of the year.
    ///
    /// @param year
    ///   Year part of date.
    /// @param yearDayNumber
    ///   N-th day of the year.
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    CTime(int year, int yearDayNumber,
          ETimeZone tz = eLocal,
          ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Explicit conversion constructor for string representation of time.
    ///
    /// Construct time object from string representation of time.
    ///
    /// @param str
    ///   String representation of time in format "fmt".
    /// @param fmt
    ///   Format in which "str" is presented. Default value of kEmptyStr,
    ///   implies the "M/D/Y h:m:s" format.
    /// @param tz
    ///   Whether to use local time (default) or GMT.
    /// @param tzp
    ///   What time zone precision to use.
    explicit CTime(const string& str, const string& fmt = kEmptyStr,
                   ETimeZone tz = eLocal,
                   ETimeZonePrecision tzp = eTZPrecisionDefault);

    /// Copy constructor.
    CTime(const CTime& t);

    /// Assignment operator.
    CTime& operator= (const CTime& t);

    /// Assignment operator.
    ///
    /// If current format contains 'Z', then TimeZone will be set to:
    /// - eGMT if "str" has word "GMT" in the appropriate position;
    /// - eLocal otherwise.
    ///
    /// If current format does not contain 'Z', TimeZone will not be changed.
    CTime& operator= (const string& str);

    /// Set time using time_t time value.
    ///
    /// @param t
    ///   Time to set in time object. This is always in GMT time format, and
    ///   nanoseconds will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeT(const time_t& t);

    /// Get time (time_t format).
    ///
    /// The function return the number of seconds elapsed since midnight
    /// (00:00:00), January 1, 1970. Do not use this function if year is
    /// less 1970.
    /// @return
    ///   Time in time_t format.
    time_t GetTimeT(void) const;

    /// Set time using database format time, TDBTimeU.
    ///
    /// Object's time format will always change to eLocal after call.
    ///
    /// @param t
    ///   Time to set in time object in TDBTimeU format.
    ///   This is always in local time format, and seconds, and nanoseconds
    ///   will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeDBU(const TDBTimeU& t);

    /// Set time using database format time, TDBTimeI.
    ///
    /// Object's time format will always change to eLocal after call.
    ///
    /// @param t
    ///   Time to set in time object in TDBTimeI format.
    ///   This is always in local time format, and seconds, and nanoseconds
    ///   will be truncated.
    /// @return
    ///   Time object that is set.
    CTime& SetTimeDBI(const TDBTimeI& t);

    /// Get time in database format time, TDBTimeU.
    ///
    /// @return
    ///   Time value in database format, TDBTimeU.
    TDBTimeU GetTimeDBU(void) const;

    /// Get time in database format time, TDBTimeI.
    ///
    /// @return
    ///   Time value in database format TDBTimeI.
    TDBTimeI GetTimeDBI(void) const;

    /// Make the time current in the presently active time zone.
    CTime& SetCurrent(void);

    /// Make the time "empty",
    CTime& Clear(void);

    /// Truncate the time to days (strip H,M,S and Nanoseconds.
    CTime& Truncate(void);

    /// Set the current time format.
    ///
    /// The default format is: "M/D/Y h:m:s".
    /// @param fmt
    ///   String of letters describing the time format. The letters having
    ///   the following meanings:
    ///   - Y = year with century
    ///   - y = year without century           (00-99)
    ///   - M = month as decimal number        (01-12)
    ///   - B = full month name                (January-December)
    ///   - b = abbeviated month name          (Jan-Dec)
    ///   - D = day as decimal number          (01-31)
    ///   - d = day as decimal number (w/o 0)  (1-31)
    ///   - H = hour in 12-hour format         (00-12)
    ///   - h = hour in 24-hour format         (00-23)
    ///   - m = minute as decimal number       (00-59)
    ///   - s = second as decimal number       (00-59)
    ///   - l = milliseconds as decimal number (000-999)
    ///   - r = microseconds as decimal number (000000-999999)
    ///   - S = nanosecond as decimal number   (000000000-999999999)
    ///   - P = am/pm                          (AM/PM)
    ///   - p = am/pm                          (am/pm)
    ///   - Z = timezone format                (GMT or none)
    ///   - z = timezone shift                 (GMT[+/-HHMM])
    ///   - W = full day of week name          (Sunday-Saturday)
    ///   - w = abbreviated day of week name   (Sun-Sat)
    /// @sa
    ///   GetFormat
    static void SetFormat(const string& fmt);

    /// Get the current time format.
    ///
    /// The default format is: "M/D/Y h:m:s".
    /// @return
    ///   A string of letters describing the time format.
    /// @sa
    ///   SetFormat
    static string GetFormat(void);

    /// Get numerical value of the month by name.
    ///
    /// @param month
    ///   Full or abbreviated month name.
    /// @return
    ///   Numerical value of a given month (1..12).
    /// @sa
    ///   MonthNumToName, Month
    static int MonthNameToNum(const string& month);

    /// Get name of the month by numerical value.
    ///
    /// @param month
    ///   Full or abbreviated month name.
    /// @param format
    ///   Format for returned value (full or abbreviated).
    /// @return
    ///   Name of the month.
    /// @sa
    ///   MonthNameToNum, Month
    static string MonthNumToName(int month, ENameFormat format = eFull);

    /// Get numerical value of the day of week by name.
    ///
    /// @param day
    ///   Full or abbreviated day of week name.
    /// @return
    ///   Numerical value of a given day of week (0..6).
    /// @sa
    ///   DayOfWeekNumToName, DayOfWeek
    static int DayOfWeekNameToNum(const string& day);

    /// Get name of the day of week by numerical value.
    ///
    /// @param day
    ///   Full or abbreviated day of week name.
    /// @param format
    ///   Format for returned value (full or abbreviated).
    /// @return
    ///   Name of the day of week.
    /// @sa
    ///   DayOfWeekNameToNum, DayOfWeek
    static string DayOfWeekNumToName(int day, ENameFormat format = eFull);

    /// Transform time to string.
    ///
    /// @param fmt
    ///   Format specifier used to convert time span to string.
    ///   If "fmt" is not defined, then GetFormat() will be used.
    /// @param out_tz
    ///   Output timezone. This is a difference in seconds between GMT time
    ///   and local time for some place (for example, for EST5 timezone
    ///   its value is 18000). This parameter works only with local time.
    ///   If the time object have GMT time that it is ignored.
    ///   Before transformation to string the time will be converted to output
    ///   timezone. Timezone can be printed as string 'GMT[+|-]HHMM' using
    ///   format symbol 'z'. By default current timezone is used.
    /// @sa
    ///   GetFormat, SetFormat
    string AsString(const string& fmt = kEmptyStr,
                    TSeconds out_tz   = eCurrentTimeZone) const;


    /// Return time as string using the format returned by GetFormat().
    operator string(void) const;

    //
    // Get various components of time.
    //

    /// Get year.
    ///
    /// Year = 1900 ..
    /// AsString() format symbols "Y", "y".
    int Year(void) const;

    /// Get month.
    ///
    /// Month number = 1..12.
    /// AsString() format symbols "M", "B", "b".
    int Month(void) const;

    /// Get day.
    ///
    /// Day of the month = 1..31
    /// AsString() format symbol "D".
    int Day(void) const;

    /// Get hour.
    ///
    /// Hours since midnight = 0..23.
    /// AsString() format symbol "h".
    int Hour(void) const;

    /// Get minute.
    ///
    /// Minutes after the hour = 0..59
    /// AsString() format symbol "m".
    int Minute(void) const;

    /// Get second.
    ///
    /// Seconds after the minute = 0..59
    /// AsString() format symbol "s".
    int Second(void) const;

    /// Get nano-seconds.
    ///
    /// Nano-seconds after the second = 0..999999999
    /// AsString() format symbol "S".
    long NanoSecond(void) const;

    //
    // Set various components of time.
    //

    /// Set year.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Feb 29 2000".SetYear(2001) => "Feb 28 2001".
    /// Because 2001 is not leap year.
    /// @param year
    ///   Year to set.
    /// @sa
    ///   Year
    void SetYear(int year);

    /// Set month.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Dec 31 2000".SetMonth(2) => "Feb 29 2000".
    /// Therefore e.g. calling SetMonth(1) again that result will be "Jan 28".
    /// @param month
    ///   Month number to set. Month number = 1..12.
    /// @sa
    ///   Month
    void SetMonth(int month);

    /// Set day.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Feb 01 2000".SetDay(31) => "Feb 29 2000".
    /// @param day
    ///   Day to set. Day of the month = 1..31.
    /// @sa
    ///   Day
    void SetDay(int day);

    /// Set hour.
    ///
    /// @param hour
    ///   Hours since midnight = 0..23.
    /// @sa
    ///   Hour
    void SetHour(int hour);

    /// Set minute.
    ///
    /// @param minute
    ///   Minutes after the hour = 0..59.
    /// @sa
    ///   Minute
    void SetMinute(int minute);

    /// Set second.
    ///
    /// @param second
    ///   Seconds after the minute = 0..59.
    /// @sa
    ///   Second
    void SetSecond(int second);

    /// Set nano-seconds.
    ///
    /// @param nanosecond
    ///   Nano-seconds after the second = 0..999999999.
    /// @sa
    ///   NanoSecond
    void SetNanoSecond(long nanosecond);

    /// Get year's day number.
    ///
    /// Year day number = 1..366
    int YearDayNumber(void) const;

    /// Get week number in the year.
    ///
    /// Calculate the week number in a year of a given date.
    /// The week can start on any day accordingly given parameter.
    /// First week always start with 1st January.
    /// @param week_start
    ///   What day of week is first.
    ///   Default is to use Sunday as first day of week. For Monday-based
    ///   weeks use eMonday as parameter value.
    /// @return
    ///   Week number = 1..54.
    int YearWeekNumber(EDayOfWeek first_day_of_week = eSunday) const;

    /// Get week number in current month.
    ///
    /// @return
    ///   Week number in current month = 1..6.
    /// @sa
    ///   YearWeekNumber
    int MonthWeekNumber(EDayOfWeek first_day_of_week = eSunday) const;

    /// Get day of week.
    ///
    /// Days since Sunday = 0..6
    /// AsString() format symbols "W", "w".
    int DayOfWeek(void) const;

    /// Get number of days in the current month.
    ///
    /// Number of days = 1..31
    int DaysInMonth(void) const;

    /// Add specified years and adjust for daylight saving time.
    ///
    /// It is an exact equivalent of calling AddMonth(years * 12).
    /// @sa
    ///   AddMonth
    CTime& AddYear(int years = 1, EDaylight adl = eDaylightDefault);

    /// Add specified months and adjust for daylight saving time.
    ///
    /// Beware that this operation is inherently inconsistent.
    /// In case of different number of days in the months, the day number
    /// can change, e.g.:
    ///  - "Dec 31 2000".AddMonth(2) => "Feb 28 2001" ("Feb 29" if leap year).
    /// Therefore e.g. calling AddMonth(1) 12 times for e.g. "Jul 31" will
    /// result in "Jul 28" (or "Jul 29") of the next year.
    /// @param months
    ///   Months to add. Default is 1 month.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight savings time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddMonth(int months = 1, EDaylight adl = eDaylightDefault);

    /// Add specified days and adjust for daylight saving time.
    ///
    /// @param days
    ///   Days to add. Default is 1 day.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddDay(int days = 1, EDaylight adl = eDaylightDefault);

    /// Add specified hours and adjust for daylight saving time.
    ///
    /// @param hours
    ///   Hours to add. Default is 1 hour.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddHour(int hours = 1, EDaylight adl = eDaylightDefault);

    /// Add specified minutes and adjust for daylight saving time.
    ///
    /// @param minutes
    ///   Minutes to add. Default is 1 minute.
    ///   If negative, it will result in a "subtraction" operation.
    /// @param adl
    ///   Whether to adjust for daylight saving time. Default is to adjust
    ///   for daylight saving time. This parameter is for eLocal time zone
    ///   and where the time zone precision is not eNone.
    CTime& AddMinute(int minutes = 1, EDaylight adl = eDaylightDefault);

    /// Add specified seconds.
    ///
    /// @param seconds
    ///   Seconds to add. Default is 1 second.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddSecond(TSeconds seconds = 1, EDaylight adl = eDaylightDefault);

    /// Add specified nanoseconds.
    ///
    /// @param nanoseconds
    ///   Nanoseconds to add. Default is 1 nanosecond.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddNanoSecond(long nanoseconds = 1);

    /// Add specified time span.
    ///
    /// @param timespan
    ///   Object of CTimeSpan class to add.
    ///   If negative, it will result in a "subtraction" operation.
    CTime& AddTimeSpan(const CTimeSpan& timespan);

    //
    // Add/subtract days
    //

    /// Operator to add days.
    CTime& operator+= (int days);

    /// Operator to subtract days.
    CTime& operator-= (int days);

    /// Operator to increment days.
    CTime& operator++ (void);

    /// Operator to decrement days.
    CTime& operator-- (void);

    /// Operator to increment days.
    CTime  operator++ (int);

    /// Operator to decrement days.
    CTime  operator-- (int);

    /// Operator to increment days.
    CTime operator+ (int days);

    /// Operator to decrement days.
    CTime operator- (int days);

    //
    // Add/subtract time span
    //

    // Operator to add time span.
    CTime& operator+= (const CTimeSpan& ts);

    /// Operator to subtract time span.
    CTime& operator-= (const CTimeSpan& ts);

    // Operator to add time span.
    CTime operator+ (const CTimeSpan& ts);

    /// Operator to subtract time span.
    CTime operator- (const CTimeSpan& ts);

    /// Operator to subtract times.
    CTimeSpan operator- (const CTime& t);

    //
    // Time comparison ('>' means "later", '<' means "earlier")
    //

    /// Operator to test equality of time.
    bool operator== (const CTime& t) const;

    /// Operator to test in-equality of time.
    bool operator!= (const CTime& t) const;

    /// Operator to test if time is later.
    bool operator>  (const CTime& t) const;

    /// Operator to test if time is earlier.
    bool operator<  (const CTime& t) const;

    /// Operator to test if time is later or equal.
    bool operator>= (const CTime& t) const;

    /// Operator to test if time is earlier or equal.
    bool operator<= (const CTime& t) const;

    //
    // Time difference
    //

    /// Difference in days from specified time.
    double DiffDay(const CTime& t) const;

    /// Difference in hours from specified time.
    double DiffHour(const CTime& t) const;

    /// Difference in minutes from specified time.
    double DiffMinute(const CTime& t) const;

    /// Difference in seconds from specified time.
    TSeconds DiffSecond(const CTime& t) const;

    /// Difference in nanoseconds from specified time.
    double DiffNanoSecond(const CTime& t) const;

    /// Difference in nanoseconds from specified time.
    CTimeSpan DiffTimeSpan(const CTime& t) const;

    //
    // Checks
    //

    /// Is time empty?
    bool IsEmpty     (void) const;

    /// Is time in a leap year?
    bool IsLeap      (void) const;

    /// Is time valid?
    bool IsValid     (void) const;

    /// Is time local time?
    bool IsLocalTime (void) const;

    /// Is time GMT time?
    bool IsGmtTime   (void) const;

    //
    // Timezone functions
    //

    /// Get time zone format.
    ETimeZone GetTimeZoneFormat(void) const;

    /// Set time zone format.
    ETimeZone SetTimeZoneFormat(ETimeZone val);

    /// Get time zone precision.
    ETimeZonePrecision GetTimeZonePrecision(void) const;

    /// Set time zone precision.
    ETimeZonePrecision SetTimeZonePrecision(ETimeZonePrecision val);

    /// Get difference between local timezone and GMT in seconds.
    TSeconds TimeZoneDiff(void) const;

    /// Get current time as local time.
    CTime GetLocalTime(void) const;

    /// Get current time as GMT time.
    CTime GetGmtTime(void) const;

    /// Convert current time into specified time zone time.
    CTime& ToTime(ETimeZone val);

    /// Convert current time into local time.
    CTime& ToLocalTime(void);

    /// Convert current time into GMT time.
    CTime& ToGmtTime(void);

private:
    /// Helper method to set time value from string "str" using format "fmt".
    void x_Init(const string& str, const string& fmt);

    /// Helper method to check if time format "fmt" is valid.
    static void x_VerifyFormat(const string& fmt);

    /// Helper method to set time from 'time_t' -- If "t" not specified,
    /// then set to current time.
    CTime& x_SetTime(const time_t* t = 0);

    /// Version of x_SetTime() with MT-safe locks
    CTime& x_SetTimeMTSafe(const time_t* t = 0);

    /// Helper method to adjust day number to correct value after day
    /// manipulations.
    void x_AdjustDay(void);

    /// Helper method to adjust the time to correct timezone (across the
    /// barrier of winter & summer times) using "from" as a reference point.
    ///
    /// This does the adjustment only if the time object:
    /// - contains local time (not GMT), and
    /// - has TimeZonePrecision != CTime::eNone, and
    /// - differs from "from" in the TimeZonePrecision (or larger) part.
    CTime& x_AdjustTime(const CTime& from, bool shift_time = true);

    /// Helper method to forcibly adjust timezone using "from" as a
    /// reference point.
    CTime& x_AdjustTimeImmediately(const CTime& from, bool shift_time = true);

    /// Helper method to check if there is a need adjust time in timezone.
    bool x_NeedAdjustTime(void) const;

    /// Helper method to add hour with/without shift time.
    /// Parameter "shift_time" access or denied use time shift in
    /// process adjust hours.
    CTime& x_AddHour(int hours = 1, EDaylight daylight = eDaylightDefault,
                     bool shift_time = true);

private:
    typedef struct {
        // Time
        unsigned int  year        : 12;  // 4 digits
        unsigned char month       :  4;  // 0..12
        unsigned char day         :  5;  // 0..31
        unsigned char hour        :  5;  // 0..23
        unsigned char min         :  6;  // 0..59
        unsigned char sec         :  6;  // 0..61
        // Difference between GMT and local time in seconds,
        // as stored during the last call to x_AdjustTime***().
        Int4          adjTimeDiff : 18;
        // Timezone and precision
        ETimeZone     tz          :  2;  // Time format local/GMT
        ETimeZonePrecision tzprec :  4;  // Time zone precission
        unsigned                  :  0;  // Force alignment
        Int4          nanosec;
    } TData;
    TData m_Data;  ///< Packed members

    // Friend left-hand operators
    NCBI_XNCBI_EXPORT
    friend CTime operator+ (int days, const CTime& t);
    NCBI_XNCBI_EXPORT
    friend CTime operator+ (const CTimeSpan& ts, const CTime& t);

    // Friend class
    friend class CFastLocalTime;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeSpan
///
/// Defines a class to represents a relative time span.
/// Time span can be both positive and negative.
///
/// Throw exception of type CTimeException on errors.

class NCBI_XNCBI_EXPORT CTimeSpan
{
public:
    /// Default constructor.
    CTimeSpan(void);

    /// Constructor.
    ///
    /// Construct time span given the number of days, hours, minutes, seconds,
    /// nanoseconds parts of a time span value.
    /// @param days
    ///   Day part of time. Note day starts from 1.
    /// @param hours
    ///   Hour part of time.
    /// @param minutes
    ///   Minute part of time.
    /// @param seconds
    ///   Second part of time.
    /// @param nanoseconds
    ///   Nanosecond part of time.
    CTimeSpan(long days, long hours, long minutes, long seconds,
              long nanoseconds = 0);

    /// Constructor.
    ///
    /// Construct time span given the number of seconds and nanoseconds.
    /// @param seconds
    ///   Second part of time.
    /// @param nanoseconds
    ///   Nanosecond part of time.
    explicit CTimeSpan(long seconds, long nanoseconds = 0);

    /// Constructor.
    ///
    /// Construct time span from number of seconds.
    /// Please, use this constructor as rarely as possible, because after
    /// doing some arithmetical operations and conversion with it,
    /// the time span can differ at some nanoseconds from expected value.
    /// @param seconds
    ///   Second part of time. The fractional part is used to compute
    ///   nanoseconds.
    explicit CTimeSpan(double seconds);

    /// Explicit conversion constructor for string representation of time span.
    ///
    /// Construct time span object from string representation of time.
    ///
    /// @param str
    ///   String representation of time span in format "fmt".
    /// @param fmt
    ///   Format in which "str" is presented. Default value of kEmptyStr,
    ///   implies the "-S.n" format.
    explicit CTimeSpan(const string& str, const string& fmt = kEmptyStr);

    /// Copy constructor.
    CTimeSpan(const CTimeSpan& t);

    /// Assignment operator.
    CTimeSpan& operator= (const CTimeSpan& t);

    /// Assignment operator.
    CTimeSpan& operator= (const string& str);

    /// Make the time span "empty",
    CTimeSpan& Clear(void);

    /// Get sign of time span.
    ESign GetSign(void) const;

    /// Set the current time span format.
    ///
    /// The default format is: "-S.n".
    /// @param fmt
    ///   String of letters describing the time span format.
    ///   The letters having the following meanings:
    ///   - - = add minus for negative time spans
    ///   - d = days
    ///   - h = hours (-23 - 23)
    ///   - H = total number of hour
    ///   - m = minutes (-59 - 59)
    ///   - M = total number of minutes
    ///   - s = seconds (-59 - 59)
    ///   - S = total number of seconds
    ///   - n = nanosecond (-999999999 - 999999999)
    ///   - N = total number of nanoseconds
    /// @sa
    ///   GetFormat
    static void SetFormat(const string& fmt);

    /// Get the current time span format.
    ///
    /// The default format is: "-S.n".
    /// @return
    ///   A string of letters describing the time span format.
    /// @sa
    ///   SetFormat
    static string GetFormat(void);

    /// Transform time span to string.
    ///
    /// @param fmt
    ///   Format specifier used to convert time span to string.
    ///   If "fmt" is not defined, then GetFormat() will be used.
    /// @return
    ///   A string representation of time span in specified format.
    /// @sa
    ///   GetFormat, SetFormat
    string AsString(const string& fmt = kEmptyStr) const;

    /// Return span time as string using the format returned by GetFormat().
    operator string(void) const;


    /// Precision for span "smart" string. Used in AsSmartString() method.
    enum ESmartStringPrecision {
        // Named precision levels
        eSSP_Year,               ///< Round to years
        eSSP_Month,              ///< Round to months
        eSSP_Day,                ///< Round to days
        eSSP_Hour,               ///< Round to hours
        eSSP_Minute,             ///< Round to minutes
        eSSP_Second,             ///< Round to seconds
        eSSP_Millisecond,        ///< Round to milliseconds
        eSSP_Microsecond,        ///< Round to microseconds
        eSSP_Nanosecond,         ///< Do not round at all (accurate time span)

        // Float precision levels (1-7)
        eSSP_Precision1,
        eSSP_Precision2,
        eSSP_Precision3,
        eSSP_Precision4,
        eSSP_Precision5,
        eSSP_Precision6,
        eSSP_Precision7,

        eSSP_Default = eSSP_Day  ///< Default precision level
    };

    /// Which format use to zero time span output.
    enum ESmartStringZeroMode {
        eSSZ_SkipZero,           ///< Skip zero valued
        eSSZ_NoSkipZero,         ///< Print zero valued
        eSSZ_Default = eSSZ_SkipZero
    };

    /// Transform time span to "smart" string.
    ///
    /// @param precision
    ///   Enum value describing how many parts of time span should be
    ///   returned. Values from eSSP_Year to eSSP_Nanosecond apparently
    ///   describe part of time span which will be last in output string.
    ///   Floating precision levels eSSP_PrecisionN say that maximum 'N'
    ///   parts of time span will be put to output string.
    ///   The parts counting begin from first non-zero value.
    /// @param rounding
    ///   Rounding mode. By default time span will be truncated at last value
    //    specified by precision. If mode is eRound, that last significant
    //    part of time span will be arifmetically rounded on base .
    //    For example, if precison is eSSP_Day and number of hours in time
    //    span is 20, that number of days will be increased on 1.
    /// @param zero_mode
    ///   Mode to print or skip zero parts of time span which should be
    ///   printed but have 0 value. Trailing and leading zeros will be
    ///   never printed.
    /// @return
    ///   A string representation of time span.
    /// @sa
    ///   AsString, ESmartStringPrecision, ERound, ESmartStringZeroMode
    string AsSmartString(ESmartStringPrecision precision = eSSP_Default,
                         ERound                rounding  = eTrunc,
                         ESmartStringZeroMode  zero_mode = eSSZ_Default)
                         const;

    //
    // Get various components of time span.
    //

    /// Get number of complete days.
    long GetCompleteDays(void) const;

    /// Get number of complete hours.
    long GetCompleteHours(void) const;

    /// Get number of complete minutes.
    long GetCompleteMinutes(void) const;

    /// Get number of complete seconds.
    long GetCompleteSeconds(void) const;

    /// Get number of nanoseconds.
    long GetNanoSecondsAfterSecond(void) const;

    /// Return span time as number of seconds.
    ///
    /// @return
    ///   Return representative of time span as type double.
    ///   The fractional part represents nanoseconds part of time span.
    ///   The double representation of the time span is aproximate.
    double GetAsDouble(void) const;

    /// Return TRUE is an object keep zero time span.
    bool IsEmpty(void);

    //
    // Arithmetic
    //

    // Operator to add time span.
    CTimeSpan& operator+= (const CTimeSpan& t);

    // Operator to add time span.
    CTimeSpan operator+ (const CTimeSpan& t);

    /// Operator to subtract time span.
    CTimeSpan& operator-= (const CTimeSpan& t);

    /// Operator to subtract time span.
    CTimeSpan operator- (const CTimeSpan& t);

    /// Unary operator "-" (minus) to change time span sign.
    const CTimeSpan operator- (void) const;

    /// Invert time span. Changes time span sign.
    void Invert(void);

    //
    // Comparison
    //

    /// Operator to test equality of time span.
    bool operator== (const CTimeSpan& t) const;

    /// Operator to test in-equality of time span.
    bool operator!= (const CTimeSpan& t) const;

    /// Operator to test if time span is greater.
    bool operator>  (const CTimeSpan& t) const;

    /// Operator to test if time is less.
    bool operator<  (const CTimeSpan& t) const;

    /// Operator to test if time span is greater or equal.
    bool operator>= (const CTimeSpan& t) const;

    /// Operator to test if time span is less or equal.
    bool operator<= (const CTimeSpan& t) const;

private:
    /// Get hour.
    /// Hours since midnight = -23..23
    int x_Hour(void) const;

    /// Get minute.
    /// Minutes after the hour = -59..59
    int x_Minute(void) const;

    /// Get second.
    /// Seconds after the minute = -59..59
    int x_Second(void) const;

    /// Helper method to set time value from string "str" using format "fmt".
    void x_Init(const string& str, const string& fmt);

    /// Helper method to normalize stored time value.
    void x_Normalize(void);

    /// Helper method to check if time format "fmt" is valid.
    static void x_VerifyFormat(const string& fmt);

private:
    long  m_Sec;      ///< Seconds part of the time span
    long  m_NanoSec;  ///< Nanoseconds after the second
};



/////////////////////////////////////////////////////////////////////////////
///
/// CFastLocalTime --
///
/// Define a class for quick and dirty getting a local time.
///
/// Getting local time may trigger request to a time server daemon,
/// thus potentially causing a relatively significant delay,
/// so we 'll need a caching local timer.

class NCBI_XNCBI_EXPORT CFastLocalTime
{
public:
    /// Constructor.
    /// It should not try to get local time from OS more often than once
    /// an hour. Default:  check once, 5 seconds after each hour.
    CFastLocalTime(unsigned int sec_after_hour = 5);

    /// Get local time
    CTime GetLocalTime(void);

    /// Get difference in seconds between UTC and current local time
    /// (daylight information included)
    int GetLocalTimezone(void);

    /// Do unscheduled check
    void Tuneup(void);

private:
    /// Internal version of Tuneup()
    void x_Tuneup(time_t timer);

private:
    unsigned int m_SecAfterHour;  ///< Time interval in seconds after hour
                              ///< in which we should avoid to do Tuneup().
    CTime   m_LocalTime;      ///< Current local time
    CTime   m_TunedTime;      ///< Last tuned time (changed by Tuneup())

    time_t  m_LastTuneupTime; ///< Last Tuneup() time
    time_t  m_LastSysTime;    ///< Last system time
    int     m_Timezone;       ///< Cached timezone adjustment for local time
    int     m_Daylight;       ///< Cached system daylight information
    bool    m_IsTuneup;       ///< Tuneup() in progress (MT)
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStopWatch --
///
/// Define a stop watch class to measure elasped time.

class NCBI_XNCBI_EXPORT CStopWatch
{
public:
    /// Defines how to create new timer.
    enum EStart {
        eStart,   ///< Stat timer immediately after creating.
        eStop     ///< Do not start timer, just create it.
    };

    /// Constructor.
    /// NB. By default ctor doesn't start timer, it merely creates it.
    CStopWatch(EStart state = eStop);

    /// Constructor.
    /// Start timer if argument is true.
    /// @deprecated Use CStopWatch(EStat) constuctor instead.
    NCBI_DEPRECATED_CTOR(CStopWatch(bool start));

    /// Start the timer.
    void Start(void);

    /// Return time elapsed since first Start() or last Restart() call
    /// (in seconds).
    /// Result is undefined if Start() or Restart() wasn't previously called.
    double Elapsed(void) const;

    /// Suspend the timer.
    /// Next Start() call continue to count time accured before.
    void Stop(void);

    /// Return time elapsed since first Start() or last Restart() call
    /// (in seconds). Start new timer after that.
    /// Result is undefined if Start() or Restart() wasn't previously called.
    double Restart(void);

    /// Set the current stopwatch time format.
    ///
    /// The default format is: "-S.n".
    /// @sa
    ///   CTimeSpan::GetFormat, GetFormat
    static void SetFormat(const string& fmt);

    /// Get the current stopwatch time format.
    ///
    /// The default format is: "-S.n".
    /// @return
    ///   A string of letters describing the time span format.
    ///   The letters having the same means that for CTimeSpan.
    /// @sa
    ///   CTimeSpan::GetFormat, SetFormat
    static string GetFormat(void);

    /// Transform stopwatch time to string.
    ///
    /// According to used OS, the double representation can provide much
    /// finer grained time control. The string representation is limited
    /// by nanoseconds.
    /// @param fmt
    ///   If "fmt" is not defined, then GetFormat() will be used.
    ///   Format specifier used to convert value returned by Elapsed()
    ///   to string.
    /// @sa
    ///   CTimeSpan::AsString, Elapsed, GetFormat, SetFormat
    string AsString(const string& fmt = kEmptyStr) const;

    /// Return stopwatch time as string using the format returned
    /// by GetFormat().
    operator string(void) const;

    /// Transform elapsed time to "smart" string.
    ///
    /// For more details see CTimeSpan::AsSmartString().
    /// @param precision
    ///   Enum value describing how many parts of time span should be
    ///   returned.
    /// @param rounding
    ///   Rounding mode.
    /// @param zero_mode
    ///   Mode to print or skip zero parts of time span.
    /// @return
    ///   A string representation of elapsed time span.
    /// @sa
    ///   CTimeSpan::AsSmartString, AsString, Elapsed
    string AsSmartString(
        CTimeSpan::ESmartStringPrecision precision = CTimeSpan::eSSP_Nanosecond,
        ERound                           rounding  = eTrunc,
        CTimeSpan::ESmartStringZeroMode  zero_mode = CTimeSpan::eSSZ_Default)
        const;

protected:
    /// Get current time mark.
    static double GetTimeMark();

private:
    double m_Start;  ///< Start time value.
    double m_Total;  ///< Accumulated elapsed time.
    EStart m_State;  ///< Stopwatch state (started/stopped)

};



/////////////////////////////////////////////////////////////////////////////
///
/// CTimeException --
///
/// Define exceptions generated by CTime.
///
/// CTimeException inherits its basic functionality from CCoreException
/// and defines additional error codes.

class NCBI_XNCBI_EXPORT CTimeException : public CCoreException
{
public:
    /// Error types that CTime can generate.
    enum EErrCode {
        eInvalid,       ///< Invalid time value
        eArgument,      ///< Bad function argument
        eFormat         ///< Incorrect format
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CTimeException, CCoreException);
};


/* @} */



//=============================================================================
//
//  Extern
//
//=============================================================================


// Add/subtract days (see CTime::operator +/-)
NCBI_XNCBI_EXPORT
extern CTime operator + (int days, const CTime& t);

NCBI_XNCBI_EXPORT
extern int   operator- (const CTime& t1, const CTime& t2);

/// Quick and dirty getter of local time.
/// Use global object of CFastLocalTime class to obtain time.
/// See CFastLocalTime for details.
NCBI_XNCBI_EXPORT
extern CTime GetFastLocalTime(void);

NCBI_XNCBI_EXPORT
extern void TuneupFastLocalTime(void);


//=============================================================================
//
//  Inline
//
//=============================================================================

// Add (subtract if negative) to the time (see CTime::AddXXX)

inline
CTime AddYear(const CTime& t, int  years = 1)
{
    CTime tmp(t);
    return tmp.AddYear(years);
}

inline
CTime AddMonth(const CTime& t, int  months = 1)
{
    CTime  tmp(t);
    return tmp.AddMonth(months);
}

inline
CTime AddDay(const CTime& t, int  days = 1)
{
    CTime  tmp(t);
    return tmp.AddDay(days);
}

inline
CTime AddHour(const CTime& t, int  hours = 1)
{
    CTime  tmp(t);
    return tmp.AddHour(hours);
}

inline
CTime AddMinute(const CTime& t, int  minutes = 1)
{
    CTime  tmp(t);
    return tmp.AddMinute(minutes);
}

inline
CTime AddSecond(const CTime& t, long seconds = 1)
{
    CTime  tmp(t);
    return tmp.AddSecond(seconds);
}

inline
CTime AddNanoSecond (const CTime& t, long nanoseconds = 1)
{
    CTime  tmp(t);
    return tmp.AddNanoSecond(nanoseconds);
}

// Add/subtract CTimeSpan (see CTime::operator +/-)
inline
CTime operator+ (const CTimeSpan& ts, const CTime& t)
{
    CTime tmp(t);
    tmp.AddTimeSpan(ts);
    return tmp;
}

// Get current time (in local or GMT format)
inline
CTime CurrentTime(
    CTime::ETimeZone          tz  = CTime::eLocal,
    CTime::ETimeZonePrecision tzp = CTime::eTZPrecisionDefault
    )
{
    return CTime(CTime::eCurrent, tz, tzp);
}

// Truncate the time to days (see CTime::Truncate)
inline
CTime Truncate(const CTime& t)
{
    CTime  tmp(t);
    return tmp.Truncate();
}

/// Dumps the current stopwatch time to an output stream.
/// The time will be printed out using format specified
/// by CStopWatch::GetFormat().
inline
ostream& operator<< (ostream& os, const CStopWatch& sw)
{
    return os << sw.AsString();
}

/// Dumps the current CTime time to an output stream.
/// The time will be printed out using format specified
/// by CStopWatch::GetFormat().
inline
ostream& operator<< (ostream& os, const CTime& time)
{
    return os << static_cast<string>(time);
}


//=============================================================================
//
//  Inline class methods
//
//=============================================================================

//
//  CTime
//

inline
int CTime::Year(void) const { return m_Data.year; }

inline
int CTime::Month(void) const { return m_Data.month; }

inline
int CTime::Day(void) const { return m_Data.day; }

inline
int CTime::Hour(void) const { return m_Data.hour; }

inline
int CTime::Minute(void) const { return m_Data.min; }

inline
int CTime::Second(void) const { return m_Data.sec; }

inline
long CTime::NanoSecond(void) const { return (long)m_Data.nanosec; }

inline
CTime& CTime::AddYear(int years, EDaylight adl)
{
    return AddMonth(years * 12, adl);
}

inline
CTime& CTime::SetTimeT(const time_t& t) { return x_SetTimeMTSafe(&t); }

inline
CTime& CTime::SetCurrent(void) { return x_SetTimeMTSafe(); }

inline
CTime& CTime::operator+= (int days) { return AddDay(days); }

inline
CTime& CTime::operator-= (int days) { return AddDay(-days); }

inline
CTime& CTime::operator++ (void) { return AddDay( 1); }

inline
CTime& CTime::operator-- (void) { return AddDay(-1); }

inline
CTime& CTime::operator+= (const CTimeSpan& ts) { return AddTimeSpan(ts); }

inline
CTime& CTime::operator-= (const CTimeSpan& ts) { return AddTimeSpan(-ts); }

inline
CTime CTime::operator+ (const CTimeSpan& ts)
{
    CTime tmp(*this);
    tmp.AddTimeSpan(ts);
    return tmp;
}

inline
CTime CTime::operator- (const CTimeSpan& ts)
{
    CTime tmp(*this);
    tmp.AddTimeSpan(-ts);
    return tmp;
}

inline
CTimeSpan CTime::operator- (const CTime& t)
{
    return DiffTimeSpan(t);
}

inline
CTime::operator string(void) const { return AsString(); }

inline
CTime CTime::operator++ (int)
{
    CTime t = *this;
    AddDay(1);
    return t;
}

inline
CTime CTime::operator-- (int)
{
    CTime t = *this;
    AddDay(-1);
    return t;
}

inline
CTime CTime::operator+ (int days)
{
    CTime t = *this;
    t.AddDay(days);
    return t;
}

inline
CTime CTime::operator- (int days)
{
    CTime t = *this;
    t.AddDay(-days);
    return t;
}

inline
CTime& CTime::operator= (const string& str)
{
    x_Init(str, GetFormat());
    return *this;
}

inline
CTime& CTime::operator= (const CTime& t)
{
    if ( &t == this )
        return *this;
    m_Data = t.m_Data;
    return *this;
}

inline
bool CTime::operator!= (const CTime& t) const
{
    return !(*this == t);
}

inline
bool CTime::operator>= (const CTime& t) const
{
    return !(*this < t);
}

inline
bool CTime::operator<= (const CTime& t) const
{
    return !(*this > t);
}

inline
CTime& CTime::AddHour(int hours, EDaylight use_daylight)
{
    return x_AddHour(hours, use_daylight, true);
}

inline
bool CTime::IsEmpty() const
{
    return
        !Day()   &&  !Month()   &&  !Year()  &&
        !Hour()  &&  !Minute()  &&  !Second()  &&  !NanoSecond();
}

inline
double CTime::DiffDay(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0 / 60.0 / 24.0;
}

inline
double CTime::DiffHour(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0 / 60.0;
}

inline
double CTime::DiffMinute(const CTime& t) const
{
    return (double)DiffSecond(t) / 60.0;
}

inline
double CTime::DiffNanoSecond(const CTime& t) const
{
    long dNanoSec = NanoSecond() - t.NanoSecond();
    return (double) DiffSecond(t) * kNanoSecondsPerSecond + dNanoSec;
}

inline
bool CTime::IsLocalTime(void) const { return m_Data.tz == eLocal; }

inline
bool CTime::IsGmtTime(void) const { return m_Data.tz == eGmt; }

inline
CTime::ETimeZone CTime::GetTimeZoneFormat(void) const
{
    return m_Data.tz;
}

inline
CTime::ETimeZonePrecision CTime::GetTimeZonePrecision(void) const
{
    return m_Data.tzprec;
}

inline
CTime::ETimeZone CTime::SetTimeZoneFormat(ETimeZone val)
{
    ETimeZone tmp = m_Data.tz;
    m_Data.tz = val;
    return tmp;
}

inline
CTime::ETimeZonePrecision CTime::SetTimeZonePrecision(ETimeZonePrecision val)
{
    ETimeZonePrecision tmp = m_Data.tzprec;
    m_Data.tzprec = val;
    return tmp;
}

inline
TSeconds CTime::TimeZoneDiff(void) const
{
    TSeconds seconds = GetLocalTime().DiffSecond(GetGmtTime());
    return seconds;
}

inline
CTime CTime::GetLocalTime(void) const
{
    if ( IsLocalTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToLocalTime();
}

inline
CTime CTime::GetGmtTime(void) const
{
    if ( IsGmtTime() ) {
        return *this;
    }
    CTime t(*this);
    return t.ToGmtTime();
}

inline
CTime& CTime::ToLocalTime(void)
{
    ToTime(eLocal);
    return *this;
}

inline
CTime& CTime::ToGmtTime(void)
{
    ToTime(eGmt);
    return *this;
}

inline
bool CTime::x_NeedAdjustTime(void) const
{
    return GetTimeZoneFormat() == eLocal  &&  GetTimeZonePrecision() != eNone;
}


//
//  CTimeSpan
//

inline
CTimeSpan::CTimeSpan(void)
{
    Clear();
    return;
}

inline
CTimeSpan::CTimeSpan(long days, long hours, long minutes, long seconds,
                     long nanoseconds)
{
    m_Sec = ((days*24 + hours)*60 + minutes)*60 +
            seconds + nanoseconds/kNanoSecondsPerSecond;
    m_NanoSec = nanoseconds % kNanoSecondsPerSecond;
    x_Normalize();
}

inline
CTimeSpan::CTimeSpan(long seconds, long nanoseconds)
{
    m_Sec = seconds + nanoseconds/kNanoSecondsPerSecond;
    m_NanoSec = nanoseconds % kNanoSecondsPerSecond;
    x_Normalize();
}

inline
CTimeSpan::CTimeSpan(double seconds)
{
    m_Sec = long(seconds);
    m_NanoSec = long((seconds - m_Sec) * kNanoSecondsPerSecond);
    x_Normalize();
    return;
}

inline
CTimeSpan::CTimeSpan(const CTimeSpan& t)
{
    m_Sec = t.m_Sec;
    m_NanoSec = t.m_NanoSec;
}

inline
CTimeSpan& CTimeSpan::Clear(void) {
    m_Sec = 0;
    m_NanoSec = 0;
    return *this;
}

inline
ESign CTimeSpan::GetSign(void) const
{
    if ((m_Sec < 0) || (m_NanoSec < 0)) {
        return eNegative;
    }
    if (!m_Sec  &&  !m_NanoSec) {
        return eZero;
    }
    return ePositive;
}

inline
int CTimeSpan::x_Hour(void) const { return int((m_Sec / 3600L) % 24); }

inline
int CTimeSpan::x_Minute(void) const { return int((m_Sec / 60L) % 60); }

inline
int CTimeSpan::x_Second(void) const { return int(m_Sec % 60L); }

inline
long CTimeSpan::GetCompleteDays(void) const { return m_Sec / 86400L; }

inline
long CTimeSpan::GetCompleteHours(void) const { return m_Sec / 3600L; }

inline
long CTimeSpan::GetCompleteMinutes(void) const { return m_Sec / 60L; }

inline
long CTimeSpan::GetCompleteSeconds(void) const { return m_Sec; }

inline
long CTimeSpan::GetNanoSecondsAfterSecond(void) const { return m_NanoSec; }

inline
double CTimeSpan::GetAsDouble(void) const
{
    return m_Sec + double(m_NanoSec) / kNanoSecondsPerSecond;
}

inline
bool CTimeSpan::IsEmpty(void) { return m_Sec == 0  &&  m_NanoSec == 0; }

inline
CTimeSpan& CTimeSpan::operator= (const CTimeSpan& t)
{
    m_Sec = t.m_Sec;
    m_NanoSec = t.m_NanoSec;
    return *this;
}

inline
CTimeSpan& CTimeSpan::operator= (const string& str)
{
    x_Init(str, GetFormat());
    return *this;
}

inline
CTimeSpan::operator string(void) const { return AsString(); }

inline
CTimeSpan& CTimeSpan::operator+= (const CTimeSpan& t)
{
    m_Sec += t.m_Sec;
    m_NanoSec += t.m_NanoSec;
    x_Normalize();
    return *this;
}

inline
CTimeSpan CTimeSpan::operator+ (const CTimeSpan& t)
{
    CTimeSpan tnew(0, 0, 0, m_Sec + t.m_Sec, m_NanoSec + t.m_NanoSec);
    return tnew;
}

inline
CTimeSpan& CTimeSpan::operator-= (const CTimeSpan& t)
{
    m_Sec -= t.m_Sec;
    m_NanoSec -= t.m_NanoSec;
    x_Normalize();
    return *this;
}

inline
CTimeSpan CTimeSpan::operator- (const CTimeSpan& t)
{
    CTimeSpan tnew(0, 0, 0, m_Sec - t.m_Sec, m_NanoSec - t.m_NanoSec);
    return tnew;
}

inline
const CTimeSpan CTimeSpan::operator- (void) const
{
    CTimeSpan t;
    t.m_Sec     = -m_Sec;
    t.m_NanoSec = -m_NanoSec;
    return t;
}

inline
void CTimeSpan::Invert(void)
{
    m_Sec     = -m_Sec;
    m_NanoSec = -m_NanoSec;
}

inline
bool CTimeSpan::operator== (const CTimeSpan& t) const
{
    return m_Sec == t.m_Sec  &&  m_NanoSec == t.m_NanoSec;
}

inline
bool CTimeSpan::operator!= (const CTimeSpan& t) const
{
    return !(*this == t);
}

inline
bool CTimeSpan::operator> (const CTimeSpan& t) const
{
    if (m_Sec == t.m_Sec) {
        return m_NanoSec > t.m_NanoSec;
    }
    return m_Sec > t.m_Sec;
}


inline
bool CTimeSpan::operator< (const CTimeSpan& t) const
{
    if (m_Sec == t.m_Sec) {
        return m_NanoSec < t.m_NanoSec;
    }
    return m_Sec < t.m_Sec;
}

inline
bool CTimeSpan::operator>= (const CTimeSpan& t) const
{
    return !(*this < t);
}

inline
bool CTimeSpan::operator<= (const CTimeSpan& t) const
{
    return !(*this > t);
}


//
//  CStopWatch
//

inline
CStopWatch::CStopWatch(EStart state)
{
    m_Total = 0;
    m_State = eStop;
    if ( state == eStart ) {
        Start();
    }
}

inline
void CStopWatch::Start()
{
    m_State = eStart;
    m_Start = GetTimeMark();
}


inline
double CStopWatch::Elapsed() const
{
    if ( m_State == eStop ) {
        return m_Total;
    }
    return m_Total + GetTimeMark() - m_Start;
}


inline
void CStopWatch::Stop()
{
    m_Total += GetTimeMark() - m_Start;
    m_State = eStop;
}


inline
double CStopWatch::Restart()
{
    double previous = m_Start;
    double elapsed = m_Total + (m_Start = GetTimeMark()) - previous;
    m_Total = 0;
    m_State = eStart;
    return elapsed;
}


inline
CStopWatch::operator string(void) const
{
    return AsString();
}


inline
string CStopWatch::AsSmartString(
    CTimeSpan::ESmartStringPrecision precision,
    ERound                           rounding,
    CTimeSpan::ESmartStringZeroMode  zero_mode)
    const
{
    return CTimeSpan(Elapsed()).AsSmartString(precision, rounding, zero_mode);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.66  2006/12/20 16:52:37  ssikorsk
 * Added a stream write operator to CTime.
 *
 * Revision 1.65  2006/11/29 13:55:39  gouriano
 * Moved GetErrorCodeString method into cpp
 *
 * Revision 1.64  2006/11/01 20:10:47  ivanov
 * Cosmetics
 *
 * Revision 1.63  2006/10/24 19:11:55  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.62  2006/06/30 14:34:20  ivanov
 * Added inline to CStopWatch::AsSmartString
 *
 * Revision 1.61  2006/06/30 14:04:59  ivanov
 * + CStopWatch::AsSmartString
 *
 * Revision 1.60  2006/06/06 19:08:12  ivanov
 * CTime:: Use new type TSeconds in some methods for time representation
 *
 * Revision 1.59  2006/06/06 12:18:58  ivanov
 * Fixed compilation warnings on MSVC8 64-bit
 *
 * Revision 1.58  2006/03/31 15:55:01  ivanov
 * CStopWatch: fixed another bug with accumulated time and Elapsed()
 *
 * Revision 1.57  2006/03/29 14:10:31  ivanov
 * Fixed CStopWatch::Stop() to correctly count accumulated time
 *
 * Revision 1.56  2005/12/19 18:02:01  ucko
 * Use NCBI_DEPRECATED_CTOR in CStopWatch to restore compatibility with
 * MS Visual Studio 2005.
 *
 * Revision 1.55  2005/12/16 18:03:29  ivanov
 * + CStopWatch::Stop() to suspend a "timer"
 *
 * Revision 1.54  2005/12/01 15:55:02  ucko
 * Move deprecated CStopWatch constructor to ncbitime.cpp to avoid
 * widespread warnings.
 *
 * Revision 1.53  2005/12/01 15:49:38  ucko
 * Fix typo (= for ==) in CStopWatch::CStopWatch(EStart).
 *
 * Revision 1.52  2005/12/01 15:02:48  ucko
 * Correct placement of NCBI_DEPRECATED, which some GCC versions
 * (3.2.x and 3.3.x?) require to *follow* the declaration.
 * Simplify logic in CTimeSpan::GetAsDouble, eliminating gratuitous use
 * of abs().
 *
 * Revision 1.51  2005/12/01 14:50:05  ivanov
 * CStopWatch::
 *     - added new constructor CStopWatch(EStart)
 *     - CStopWatch(bool) declared as deprecated
 * CFastLocalTime::GetLocalTimezone() -- fixed comments
 *
 * Revision 1.50  2005/11/30 15:45:16  ivanov
 * Added comment to CFastLocalTime::GetLocalTimezone()
 * that it returns value in seconds
 *
 * Revision 1.49  2005/11/30 15:34:59  ivanov
 * + CFastLocalTime::GetLocalTimezone()
 *
 * Revision 1.48  2005/09/06 12:11:34  ivanov
 * CTime: using bit fields and fixed-size types for class members
 *
 * Revision 1.47  2005/05/31 13:18:56  ivanov
 * Minor comments fix
 *
 * Revision 1.46  2005/03/01 19:40:34  ivanov
 * + CTimeSpan::IsEmpty()
 *
 * Revision 1.45  2005/02/17 20:16:46  ivanov
 * Improved CFastLocalTime work in MT environment -- do not block all
 * other threads while one call Tuneup().
 *
 * Revision 1.44  2005/02/10 14:20:32  ivanov
 * Added quick and dirty getter of local time -- CFastLocalTime.
 * Also added global functions GetFastLocalTime() and TuneupFastLocalTime().
 *
 * Revision 1.43  2005/02/07 16:01:04  ivanov
 * Changed parameter type in the CTime::AddSecons() to long.
 * Fixed Workshop 64bits compiler warnings.
 *
 * Revision 1.42  2005/01/06 16:37:24  ivanov
 * Fixed comments for CTimeSpan
 *
 * Revision 1.41  2004/09/27 13:53:36  ivanov
 * + CTimeSpan::AsSmartString()
 *
 * Revision 1.40  2004/09/20 16:27:08  ivanov
 * CTime:: Added milliseconds, microseconds and AM/PM to string time format.
 *
 * Revision 1.39  2004/09/08 18:06:04  ivanov
 * Removed redundant #include <math.h>
 *
 * Revision 1.38  2004/09/07 21:24:30  ucko
 * Remove redundant CTimeSpan:: from Invert's declaration.
 *
 * Revision 1.37  2004/09/07 18:47:04  ivanov
 * CTimeSpan::
 *   - added new constructor CTimeSpan(long sec, long nanosec)
 *   - rename GetTotal*() -> GetComplete*()
 *   - renamed Get() - > GetAsDouble()
 *
 * Revision 1.36  2004/09/07 16:31:22  ivanov
 * Added CTimeSpan class and its support to CTime class (addition/subtraction)
 * CTime:: added new format letter support 'd', day without leading zero.
 * CStopWatch:: added operatir << to dumps the current stopwatch time to an
 * output stream.
 * Comments and other cosmetic changes.
 *
 * Revision 1.35  2004/08/19 13:02:17  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.34  2004/07/29 19:53:08  vasilche
 * Added CStopWatch::Restart() to reuse the same timer sequentially.
 *
 * Revision 1.33  2004/04/07 19:07:29  lavr
 * Document CStopWatch in a little more details
 *
 * Revision 1.32  2004/03/24 15:52:50  ivanov
 * Added new format symbol support 'z' (local time in format GMT{+|-}HHMM).
 * Added second parameter to AsString() method that specify an output
 * timezone.
 *
 * Revision 1.31  2004/03/10 19:56:38  gorelenk
 * Added NCBI_XNCBI_EXPORT prefix for functions AddYear, AddMonth, AddDay,
 * AddHour, AddMinute, AddSecond, AddNanoSecond and operators:
 * CTime operator + and CTime operator - .
 *
 * Revision 1.30  2004/01/26 18:07:22  siyan
 * Fixed errors in documentation on GetTimeT()
 *
 * Revision 1.29  2003/11/25 20:03:32  ivanov
 * Fixed misspelled eTZPrecisionDefault
 *
 * Revision 1.28  2003/11/25 19:53:33  ivanov
 * Renamed eDefault to eTZPrecisionDefault.
 * Added setters for various components of time -- Set*().
 * Added YearWeekNumber(), MonthWeekNumber().
 * Reimplemented AddYear() as AddMonth(years*12).
 *
 * Revision 1.27  2003/11/21 20:04:41  ivanov
 * + DaysInMonth()
 *
 * Revision 1.26  2003/11/18 11:58:24  siyan
 * Changed so @addtogroup does not cross namespace boundary
 *
 * Revision 1.25  2003/10/03 18:26:48  ivanov
 * Added month and day of week names conversion functions
 *
 * Revision 1.24  2003/09/11 13:26:13  siyan
 * Documentation changes
 *
 * Revision 1.23  2003/07/15 19:34:28  vakatov
 * Comment typo fix
 *
 * Revision 1.22  2003/04/16 20:28:26  ivanov
 * Added class CStopWatch
 *
 * Revision 1.21  2003/04/14 19:41:32  ivanov
 * Rollback to R1.19 -- accidental commit
 *
 * Revision 1.19  2003/04/01 19:18:43  siyan
 * Added doxygen support
 *
 * Revision 1.18  2003/02/10 22:36:55  ucko
 * Make string- and time_t-based constructors explicit, to avoid weird surprises.
 *
 * Revision 1.17  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.16  2002/10/17 16:55:01  ivanov
 * Added new time format symbols - 'b' and 'B' (month abbreviated and full name)
 *
 * Revision 1.15  2002/07/23 19:53:34  lebedev
 * NCBI_OS_MAC: Note about Daylight flag handling added
 *
 * Revision 1.14  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.13  2002/07/11 14:17:56  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.12  2002/05/13 13:56:30  ivanov
 * Added MT-Safe support
 *
 * Revision 1.11  2002/04/11 20:39:20  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.10  2001/07/06 15:11:30  ivanov
 * Added support DataBase-time's -- GetTimeDBI(), GetTimeDBU()
 *                                  SetTimeDBI(), SetTimeDBU()
 *
 * Revision 1.9  2001/07/04 19:41:07  vakatov
 * Get rid of an extra semicolon
 *
 * Revision 1.8  2001/06/12 16:56:36  vakatov
 * Added comments for the time constituents access methods
 *
 * Revision 1.7  2001/05/29 20:12:58  ivanov
 * Changed type of return value in NanoSecond().
 *
 * Revision 1.6  2001/05/29 16:14:34  ivanov
 * Return to nanosecond-revision. Corrected mistake of the work with local
 * time on Linux. Polish and improvement source code.
 * Renamed AsTimeT() -> GetTimerT().
 *
 * Revision 1.5  2001/04/30 22:01:29  lavr
 * Rollback to pre-nanosecond-revision due to necessity to use
 * configure to figure out names of global variables governing time zones
 *
 * Revision 1.4  2001/04/29 03:06:10  lavr
 * #include <time.h>" moved from .cpp to ncbitime.hpp
 *
 * Revision 1.3  2001/04/27 20:42:29  ivanov
 * Support for Local and UTC time added.
 * Support for work with nanoseconds added.
 *
 * Revision 1.2  2000/11/21 18:15:29  butanaev
 * Fixed bug in operator ++/-- (int)
 *
 * Revision 1.1  2000/11/20 22:17:42  vakatov
 * Added NCBI date/time class CTime ("ncbitime.[ch]pp") and
 * its test suite ("test/test_ncbitime.cpp")
 *
 * ===========================================================================
 */

#endif /* CORELIB__NCBITIME__HPP */
