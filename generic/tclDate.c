#ifdef lint
static char TclDatesccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define TclDateclearin (TclDatechar=(-1))
#define TclDateerrok (TclDateerrflag=0)
#define YYRECOVERING (TclDateerrflag!=0)
#define YYPREFIX "TclDate"
/* 
 * tclDate.c --
 *
 *	This file is generated from a yacc grammar defined in
 *	the file tclGetDate.y.  It should not be edited directly.
 *
 * Copyright (c) 1992-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclDate.c,v 1.11 2000/01/12 01:16:08 ericm Exp $
 */

#include "tclInt.h"
#include "tclPort.h"

#ifdef MAC_TCL
#   define EPOCH           1904
#   define START_OF_TIME   1904
#   define END_OF_TIME     2039
#else
#   define EPOCH           1970
#   define START_OF_TIME   1902
#   define END_OF_TIME     2037
#endif

/*
 * The offset of tm_year of struct tm returned by localtime, gmtime, etc.
 * I don't know how universal this is; K&R II, the NetBSD manpages, and
 * ../compat/strftime.c all agree that tm_year is the year-1900.  However,
 * some systems may have a different value.  This #define should be the
 * same as in ../compat/strftime.c.
 */
#define TM_YEAR_BASE 1900

#define HOUR(x)         ((int) (60 * x))
#define SECSPERDAY      (24L * 60L * 60L)


/*
 *  An entry in the lexical lookup table.
 */
typedef struct _TABLE {
    char        *name;
    int         type;
    time_t      value;
} TABLE;


/*
 *  Daylight-savings mode:  on, off, or not yet known.
 */
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
 *  Meridian:  am, pm, or 24-hour style.
 */
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


/*
 *  Global variables.  We could get rid of most of these by using a good
 *  union as the yacc stack.  (This routine was originally written before
 *  yacc had the %union construct.)  Maybe someday; right now we only use
 *  the %union very rarely.
 */
static char     *TclDateInput;
static DSTMODE  TclDateDSTmode;
static time_t   TclDateDayOrdinal;
static time_t   TclDateDayNumber;
static int      TclDateHaveDate;
static int      TclDateHaveDay;
static int      TclDateHaveRel;
static int      TclDateHaveTime;
static int      TclDateHaveZone;
static time_t   TclDateTimezone;
static time_t   TclDateDay;
static time_t   TclDateHour;
static time_t   TclDateMinutes;
static time_t   TclDateMonth;
static time_t   TclDateSeconds;
static time_t   TclDateYear;
static MERIDIAN TclDateMeridian;
static time_t   TclDateRelMonth;
static time_t   TclDateRelSeconds;


/*
 * Prototypes of internal functions.
 */
static void	TclDateerror _ANSI_ARGS_((char *s));
static time_t	ToSeconds _ANSI_ARGS_((time_t Hours, time_t Minutes,
		    time_t Seconds, MERIDIAN Meridian));
static int	Convert _ANSI_ARGS_((time_t Month, time_t Day, time_t Year,
		    time_t Hours, time_t Minutes, time_t Seconds,
		    MERIDIAN Meridia, DSTMODE DSTmode, time_t *TimePtr));
static time_t	DSTcorrect _ANSI_ARGS_((time_t Start, time_t Future));
static time_t	RelativeDate _ANSI_ARGS_((time_t Start, time_t DayOrdinal,
		    time_t DayNumber));
static int	RelativeMonth _ANSI_ARGS_((time_t Start, time_t RelMonth,
		    time_t *TimePtr));
static int	LookupWord _ANSI_ARGS_((char *buff));
static int	TclDatelex _ANSI_ARGS_((void));

int
TclDateparse _ANSI_ARGS_((void));
typedef union {
    time_t              Number;
    enum _MERIDIAN      Meridian;
} YYSTYPE;
#define tAGO 257
#define tDAY 258
#define tDAYZONE 259
#define tID 260
#define tMERIDIAN 261
#define tMINUTE_UNIT 262
#define tMONTH 263
#define tMONTH_UNIT 264
#define tSEC_UNIT 265
#define tSNUMBER 266
#define tUNUMBER 267
#define tZONE 268
#define tEPOCH 269
#define tDST 270
#define tISOBASE 271
#define YYERRCODE 256
short TclDatelhs[] = {                                        -1,
    0,    0,    2,    2,    2,    2,    2,    2,    2,    3,
    3,    3,    3,    3,    4,    4,    4,    6,    6,    6,
    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
    8,    8,    8,    7,    7,   10,   10,   10,   10,   10,
   10,   10,   10,   10,    9,    1,    1,
};
short TclDatelen[] = {                                         2,
    0,    2,    1,    1,    1,    1,    1,    1,    1,    2,
    4,    5,    6,    7,    2,    1,    1,    1,    2,    2,
    3,    5,    1,    5,    5,    2,    4,    2,    1,    3,
    3,    7,    2,    2,    1,    2,    3,    1,    3,    2,
    1,    3,    2,    1,    1,    0,    1,
};
short TclDatedefred[] = {                                      1,
    0,    0,   17,   38,    0,   44,   41,    0,    0,   29,
    0,    0,    2,    3,    4,    5,    6,    7,    8,    9,
    0,   19,    0,   20,   10,   36,    0,   43,   40,    0,
    0,    0,   15,    0,   33,    0,   34,    0,   30,    0,
    0,    0,    0,    0,   31,   37,   42,   39,   27,   47,
    0,    0,   11,    0,    0,    0,    0,    0,   12,   24,
   25,   22,    0,    0,   13,    0,   14,   32,
};
short TclDatedgoto[] = {                                       1,
   53,   13,   14,   15,   16,   17,   18,   19,   20,   21,
};
short TclDatesindex[] = {                                      0,
  -45,  -36,    0,    0, -263,    0,    0,  -33, -254,    0,
 -258, -246,    0,    0,    0,    0,    0,    0,    0,    0,
 -248,    0,  -22,    0,    0,    0, -244,    0,    0, -243,
 -261, -241,    0, -264,    0, -245,    0, -240,    0,  -40,
  -17,  -15,  -16,  -26,    0,    0,    0,    0,    0,    0,
 -234, -233,    0, -232, -231, -230, -229,  -34,    0,    0,
    0,    0,  -19, -227,    0, -226,    0,    0,
};
short TclDaterindex[] = {                                      0,
    0,    1,    0,    0,    0,    0,    0,   92,   15,    0,
  106,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   29,    0,   43,    0,    0,    0,   71,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   85,
    0,    0,   57,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   85,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};
short TclDategindex[] = {                                      0,
  -14,    0,    0,    0,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 375
short TclDatetable[] = {                                      12,
   18,   41,   44,   23,   52,   42,   45,   22,   37,   34,
   64,   31,   35,   32,   16,   33,   46,   51,   47,   48,
   36,   38,   39,   40,   30,   43,   49,   54,   35,   55,
   56,   57,   58,   59,   60,   61,   62,   63,   66,   67,
   68,    0,   26,   65,    0,   18,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   21,    0,    0,   16,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   28,    0,    0,   35,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   46,    0,    0,   26,    0,    0,
    0,   45,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   21,    0,    0,    0,   23,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   28,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   23,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    2,    3,    0,    0,    4,    5,    6,    7,
   50,    8,    9,   10,   24,   11,   50,   25,   26,   27,
   28,   29,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   18,   18,
    0,    0,   18,   18,   18,   18,    0,   18,   18,   18,
    0,   18,   16,   16,    0,    0,   16,   16,   16,   16,
    0,   16,   16,   16,    0,   16,   35,   35,    0,    0,
   35,   35,   35,   35,    0,   35,   35,   35,    0,   35,
   26,   26,    0,    0,   26,   26,   26,   26,    0,   26,
   26,   26,    0,   26,   21,   21,    0,    0,   21,   21,
   21,   21,    0,   21,   21,   21,    0,   21,   28,   28,
    0,    0,   28,   28,   28,   28,    0,    0,   28,   28,
    0,   28,   46,   46,    0,    0,   46,   46,   46,   46,
   45,   46,   46,   46,    0,   46,    0,    0,   45,   45,
   45,    0,   45,   23,   23,    0,    0,   23,   23,   23,
   23,    0,   23,    0,   23,
};
short TclDatecheck[] = {                                      45,
    0,  263,  267,  267,   45,  267,  271,   44,  257,  268,
   45,   45,  271,   47,    0,  270,  262,   58,  264,  265,
  267,   44,  267,  267,   58,  267,  267,   45,    0,   45,
   47,   58,  267,  267,  267,  267,  267,  267,   58,  267,
  267,   -1,    0,   58,   -1,   45,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,    0,   -1,   -1,   45,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
    0,   -1,   -1,   45,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,    0,   -1,   -1,   45,   -1,   -1,
   -1,    0,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   45,   -1,   -1,   -1,    0,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   45,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   45,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  258,  259,   -1,   -1,  262,  263,  264,  265,
  261,  267,  268,  269,  258,  271,  261,  261,  262,  263,
  264,  265,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  258,  259,
   -1,   -1,  262,  263,  264,  265,   -1,  267,  268,  269,
   -1,  271,  258,  259,   -1,   -1,  262,  263,  264,  265,
   -1,  267,  268,  269,   -1,  271,  258,  259,   -1,   -1,
  262,  263,  264,  265,   -1,  267,  268,  269,   -1,  271,
  258,  259,   -1,   -1,  262,  263,  264,  265,   -1,  267,
  268,  269,   -1,  271,  258,  259,   -1,   -1,  262,  263,
  264,  265,   -1,  267,  268,  269,   -1,  271,  258,  259,
   -1,   -1,  262,  263,  264,  265,   -1,   -1,  268,  269,
   -1,  271,  258,  259,   -1,   -1,  262,  263,  264,  265,
  259,  267,  268,  269,   -1,  271,   -1,   -1,  267,  268,
  269,   -1,  271,  258,  259,   -1,   -1,  262,  263,  264,
  265,   -1,  267,   -1,  269,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 271
#if YYDEBUG
char *TclDatename[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,"','","'-'",0,"'/'",0,0,0,0,0,0,0,0,0,0,"':'",0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"tAGO","tDAY",
"tDAYZONE","tID","tMERIDIAN","tMINUTE_UNIT","tMONTH","tMONTH_UNIT","tSEC_UNIT",
"tSNUMBER","tUNUMBER","tZONE","tEPOCH","tDST","tISOBASE",
};
char *TclDaterule[] = {
"$accept : spec",
"spec :",
"spec : spec item",
"item : time",
"item : zone",
"item : date",
"item : day",
"item : rel",
"item : iso",
"item : number",
"time : tUNUMBER tMERIDIAN",
"time : tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER '-' tUNUMBER",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid",
"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER '-' tUNUMBER",
"zone : tZONE tDST",
"zone : tZONE",
"zone : tDAYZONE",
"day : tDAY",
"day : tDAY ','",
"day : tUNUMBER tDAY",
"date : tUNUMBER '/' tUNUMBER",
"date : tUNUMBER '/' tUNUMBER '/' tUNUMBER",
"date : tISOBASE",
"date : tUNUMBER '-' tMONTH '-' tUNUMBER",
"date : tUNUMBER '-' tUNUMBER '-' tUNUMBER",
"date : tMONTH tUNUMBER",
"date : tMONTH tUNUMBER ',' tUNUMBER",
"date : tUNUMBER tMONTH",
"date : tEPOCH",
"date : tUNUMBER tMONTH tUNUMBER",
"iso : tISOBASE tZONE tISOBASE",
"iso : tISOBASE tZONE tUNUMBER ':' tUNUMBER ':' tUNUMBER",
"iso : tISOBASE tISOBASE",
"rel : relunit tAGO",
"rel : relunit",
"relunit : tUNUMBER tMINUTE_UNIT",
"relunit : '-' tUNUMBER tMINUTE_UNIT",
"relunit : tMINUTE_UNIT",
"relunit : '-' tUNUMBER tSEC_UNIT",
"relunit : tUNUMBER tSEC_UNIT",
"relunit : tSEC_UNIT",
"relunit : '-' tUNUMBER tMONTH_UNIT",
"relunit : tUNUMBER tMONTH_UNIT",
"relunit : tMONTH_UNIT",
"number : tUNUMBER",
"o_merid :",
"o_merid : tMERIDIAN",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int TclDatedebug;
int TclDatenerrs;
int TclDateerrflag;
int TclDatechar;
short *TclDatessp;
YYSTYPE *TclDatevsp;
YYSTYPE TclDateval;
YYSTYPE TclDatelval;
short TclDatess[YYSTACKSIZE];
YYSTYPE TclDatevs[YYSTACKSIZE];
#define TclDatestacksize YYSTACKSIZE

/*
 * Month and day table.
 */
static TABLE    MonthDayTable[] = {
    { "january",        tMONTH,  1 },
    { "february",       tMONTH,  2 },
    { "march",          tMONTH,  3 },
    { "april",          tMONTH,  4 },
    { "may",            tMONTH,  5 },
    { "june",           tMONTH,  6 },
    { "july",           tMONTH,  7 },
    { "august",         tMONTH,  8 },
    { "september",      tMONTH,  9 },
    { "sept",           tMONTH,  9 },
    { "october",        tMONTH, 10 },
    { "november",       tMONTH, 11 },
    { "december",       tMONTH, 12 },
    { "sunday",         tDAY, 0 },
    { "monday",         tDAY, 1 },
    { "tuesday",        tDAY, 2 },
    { "tues",           tDAY, 2 },
    { "wednesday",      tDAY, 3 },
    { "wednes",         tDAY, 3 },
    { "thursday",       tDAY, 4 },
    { "thur",           tDAY, 4 },
    { "thurs",          tDAY, 4 },
    { "friday",         tDAY, 5 },
    { "saturday",       tDAY, 6 },
    { NULL }
};

/*
 * Time units table.
 */
static TABLE    UnitsTable[] = {
    { "year",           tMONTH_UNIT,    12 },
    { "month",          tMONTH_UNIT,    1 },
    { "fortnight",      tMINUTE_UNIT,   14 * 24 * 60 },
    { "week",           tMINUTE_UNIT,   7 * 24 * 60 },
    { "day",            tMINUTE_UNIT,   1 * 24 * 60 },
    { "hour",           tMINUTE_UNIT,   60 },
    { "minute",         tMINUTE_UNIT,   1 },
    { "min",            tMINUTE_UNIT,   1 },
    { "second",         tSEC_UNIT,      1 },
    { "sec",            tSEC_UNIT,      1 },
    { NULL }
};

/*
 * Assorted relative-time words.
 */
static TABLE    OtherTable[] = {
    { "tomorrow",       tMINUTE_UNIT,   1 * 24 * 60 },
    { "yesterday",      tMINUTE_UNIT,   -1 * 24 * 60 },
    { "today",          tMINUTE_UNIT,   0 },
    { "now",            tMINUTE_UNIT,   0 },
    { "last",           tUNUMBER,       -1 },
    { "this",           tMINUTE_UNIT,   0 },
    { "next",           tUNUMBER,       1 },
#if 0
    { "first",          tUNUMBER,       1 },
    { "second",         tUNUMBER,       2 },
    { "third",          tUNUMBER,       3 },
    { "fourth",         tUNUMBER,       4 },
    { "fifth",          tUNUMBER,       5 },
    { "sixth",          tUNUMBER,       6 },
    { "seventh",        tUNUMBER,       7 },
    { "eighth",         tUNUMBER,       8 },
    { "ninth",          tUNUMBER,       9 },
    { "tenth",          tUNUMBER,       10 },
    { "eleventh",       tUNUMBER,       11 },
    { "twelfth",        tUNUMBER,       12 },
#endif
    { "ago",            tAGO,   1 },
    { "epoch",          tEPOCH,   0 },
    { NULL }
};

/*
 * The timezone table.  (Note: This table was modified to not use any floating
 * point constants to work around an SGI compiler bug).
 */
static TABLE    TimezoneTable[] = {
    { "gmt",    tZONE,     HOUR( 0) },      /* Greenwich Mean */
    { "ut",     tZONE,     HOUR( 0) },      /* Universal (Coordinated) */
    { "utc",    tZONE,     HOUR( 0) },
    { "uct",    tZONE,     HOUR( 0) },      /* Universal Coordinated Time */
    { "wet",    tZONE,     HOUR( 0) },      /* Western European */
    { "bst",    tDAYZONE,  HOUR( 0) },      /* British Summer */
    { "wat",    tZONE,     HOUR( 1) },      /* West Africa */
    { "at",     tZONE,     HOUR( 2) },      /* Azores */
#if     0
    /* For completeness.  BST is also British Summer, and GST is
     * also Guam Standard. */
    { "bst",    tZONE,     HOUR( 3) },      /* Brazil Standard */
    { "gst",    tZONE,     HOUR( 3) },      /* Greenland Standard */
#endif
    { "nft",    tZONE,     HOUR( 7/2) },    /* Newfoundland */
    { "nst",    tZONE,     HOUR( 7/2) },    /* Newfoundland Standard */
    { "ndt",    tDAYZONE,  HOUR( 7/2) },    /* Newfoundland Daylight */
    { "ast",    tZONE,     HOUR( 4) },      /* Atlantic Standard */
    { "adt",    tDAYZONE,  HOUR( 4) },      /* Atlantic Daylight */
    { "est",    tZONE,     HOUR( 5) },      /* Eastern Standard */
    { "edt",    tDAYZONE,  HOUR( 5) },      /* Eastern Daylight */
    { "cst",    tZONE,     HOUR( 6) },      /* Central Standard */
    { "cdt",    tDAYZONE,  HOUR( 6) },      /* Central Daylight */
    { "mst",    tZONE,     HOUR( 7) },      /* Mountain Standard */
    { "mdt",    tDAYZONE,  HOUR( 7) },      /* Mountain Daylight */
    { "pst",    tZONE,     HOUR( 8) },      /* Pacific Standard */
    { "pdt",    tDAYZONE,  HOUR( 8) },      /* Pacific Daylight */
    { "yst",    tZONE,     HOUR( 9) },      /* Yukon Standard */
    { "ydt",    tDAYZONE,  HOUR( 9) },      /* Yukon Daylight */
    { "hst",    tZONE,     HOUR(10) },      /* Hawaii Standard */
    { "hdt",    tDAYZONE,  HOUR(10) },      /* Hawaii Daylight */
    { "cat",    tZONE,     HOUR(10) },      /* Central Alaska */
    { "ahst",   tZONE,     HOUR(10) },      /* Alaska-Hawaii Standard */
    { "nt",     tZONE,     HOUR(11) },      /* Nome */
    { "idlw",   tZONE,     HOUR(12) },      /* International Date Line West */
    { "cet",    tZONE,    -HOUR( 1) },      /* Central European */
    { "cest",   tDAYZONE, -HOUR( 1) },      /* Central European Summer */
    { "met",    tZONE,    -HOUR( 1) },      /* Middle European */
    { "mewt",   tZONE,    -HOUR( 1) },      /* Middle European Winter */
    { "mest",   tDAYZONE, -HOUR( 1) },      /* Middle European Summer */
    { "swt",    tZONE,    -HOUR( 1) },      /* Swedish Winter */
    { "sst",    tDAYZONE, -HOUR( 1) },      /* Swedish Summer */
    { "fwt",    tZONE,    -HOUR( 1) },      /* French Winter */
    { "fst",    tDAYZONE, -HOUR( 1) },      /* French Summer */
    { "eet",    tZONE,    -HOUR( 2) },      /* Eastern Europe, USSR Zone 1 */
    { "bt",     tZONE,    -HOUR( 3) },      /* Baghdad, USSR Zone 2 */
    { "it",     tZONE,    -HOUR( 7/2) },    /* Iran */
    { "zp4",    tZONE,    -HOUR( 4) },      /* USSR Zone 3 */
    { "zp5",    tZONE,    -HOUR( 5) },      /* USSR Zone 4 */
    { "ist",    tZONE,    -HOUR(11/2) },    /* Indian Standard */
    { "zp6",    tZONE,    -HOUR( 6) },      /* USSR Zone 5 */
#if     0
    /* For completeness.  NST is also Newfoundland Stanard, nad SST is
     * also Swedish Summer. */
    { "nst",    tZONE,    -HOUR(13/2) },    /* North Sumatra */
    { "sst",    tZONE,    -HOUR( 7) },      /* South Sumatra, USSR Zone 6 */
#endif  /* 0 */
    { "wast",   tZONE,    -HOUR( 7) },      /* West Australian Standard */
    { "wadt",   tDAYZONE, -HOUR( 7) },      /* West Australian Daylight */
    { "jt",     tZONE,    -HOUR(15/2) },    /* Java (3pm in Cronusland!) */
    { "cct",    tZONE,    -HOUR( 8) },      /* China Coast, USSR Zone 7 */
    { "jst",    tZONE,    -HOUR( 9) },      /* Japan Standard, USSR Zone 8 */
    { "cast",   tZONE,    -HOUR(19/2) },    /* Central Australian Standard */
    { "cadt",   tDAYZONE, -HOUR(19/2) },    /* Central Australian Daylight */
    { "east",   tZONE,    -HOUR(10) },      /* Eastern Australian Standard */
    { "eadt",   tDAYZONE, -HOUR(10) },      /* Eastern Australian Daylight */
    { "gst",    tZONE,    -HOUR(10) },      /* Guam Standard, USSR Zone 9 */
    { "nzt",    tZONE,    -HOUR(12) },      /* New Zealand */
    { "nzst",   tZONE,    -HOUR(12) },      /* New Zealand Standard */
    { "nzdt",   tDAYZONE, -HOUR(12) },      /* New Zealand Daylight */
    { "idle",   tZONE,    -HOUR(12) },      /* International Date Line East */
    /* ADDED BY Marco Nijdam */
    { "dst",    tDST,     HOUR( 0) },       /* DST on (hour is ignored) */
    /* End ADDED */
    {  NULL  }
};

/*
 * Military timezone table.
 */
static TABLE    MilitaryTable[] = {
    { "a",      tZONE,  HOUR(  1) },
    { "b",      tZONE,  HOUR(  2) },
    { "c",      tZONE,  HOUR(  3) },
    { "d",      tZONE,  HOUR(  4) },
    { "e",      tZONE,  HOUR(  5) },
    { "f",      tZONE,  HOUR(  6) },
    { "g",      tZONE,  HOUR(  7) },
    { "h",      tZONE,  HOUR(  8) },
    { "i",      tZONE,  HOUR(  9) },
    { "k",      tZONE,  HOUR( 10) },
    { "l",      tZONE,  HOUR( 11) },
    { "m",      tZONE,  HOUR( 12) },
    { "n",      tZONE,  HOUR(- 1) },
    { "o",      tZONE,  HOUR(- 2) },
    { "p",      tZONE,  HOUR(- 3) },
    { "q",      tZONE,  HOUR(- 4) },
    { "r",      tZONE,  HOUR(- 5) },
    { "s",      tZONE,  HOUR(- 6) },
    { "t",      tZONE,  HOUR(- 7) },
    { "u",      tZONE,  HOUR(- 8) },
    { "v",      tZONE,  HOUR(- 9) },
    { "w",      tZONE,  HOUR(-10) },
    { "x",      tZONE,  HOUR(-11) },
    { "y",      tZONE,  HOUR(-12) },
    { "z",      tZONE,  HOUR(  0) },
    { NULL }
};


/*
 * Dump error messages in the bit bucket.
 */
static void
TclDateerror(s)
    char  *s;
{
}


static time_t
ToSeconds(Hours, Minutes, Seconds, Meridian)
    time_t      Hours;
    time_t      Minutes;
    time_t      Seconds;
    MERIDIAN    Meridian;
{
    if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 59)
        return -1;
    switch (Meridian) {
    case MER24:
        if (Hours < 0 || Hours > 23)
            return -1;
        return (Hours * 60L + Minutes) * 60L + Seconds;
    case MERam:
        if (Hours < 1 || Hours > 12)
            return -1;
        return ((Hours % 12) * 60L + Minutes) * 60L + Seconds;
    case MERpm:
        if (Hours < 1 || Hours > 12)
            return -1;
        return (((Hours % 12) + 12) * 60L + Minutes) * 60L + Seconds;
    }
    return -1;  /* Should never be reached */
}

/*
 *-----------------------------------------------------------------------------
 *
 * Convert --
 *
 *      Convert a {month, day, year, hours, minutes, seconds, meridian, dst}
 *      tuple into a clock seconds value.
 *
 * Results:
 *      0 or -1 indicating success or failure.
 *
 * Side effects:
 *      Fills TimePtr with the computed value.
 *
 *-----------------------------------------------------------------------------
 */
static int
Convert(Month, Day, Year, Hours, Minutes, Seconds, Meridian, DSTmode, TimePtr)
    time_t      Month;
    time_t      Day;
    time_t      Year;
    time_t      Hours;
    time_t      Minutes;
    time_t      Seconds;
    MERIDIAN    Meridian;
    DSTMODE     DSTmode;
    time_t     *TimePtr;
{
    static int  DaysInMonth[12] = {
        31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    time_t tod;
    time_t Julian;
    int i;

    /* Figure out how many days are in February for the given year.
     * Every year divisible by 4 is a leap year.
     * But, every year divisible by 100 is not a leap year.
     * But, every year divisible by 400 is a leap year after all.
     */
    DaysInMonth[1] = (Year % 4 == 0) && (Year % 100 != 0 || Year % 400 == 0)
                    ? 29 : 28;

    /* Check the inputs for validity */
    if (Month < 1 || Month > 12
	    || Year < START_OF_TIME || Year > END_OF_TIME
	    || Day < 1 || Day > DaysInMonth[(int)--Month])
        return -1;

    /* Start computing the value.  First determine the number of days
     * represented by the date, then multiply by the number of seconds/day.
     */
    for (Julian = Day - 1, i = 0; i < Month; i++)
        Julian += DaysInMonth[i];
    if (Year >= EPOCH) {
        for (i = EPOCH; i < Year; i++)
            Julian += 365 + (((i % 4) == 0) &&
	            (((i % 100) != 0) || ((i % 400) == 0)));
    } else {
        for (i = Year; i < EPOCH; i++)
            Julian -= 365 + (((i % 4) == 0) &&
	            (((i % 100) != 0) || ((i % 400) == 0)));
    }
    Julian *= SECSPERDAY;

    /* Add the timezone offset ?? */
    Julian += TclDateTimezone * 60L;

    /* Add the number of seconds represented by the time component */
    if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
        return -1;
    Julian += tod;

    /* Perform a preliminary DST compensation ?? */
    if (DSTmode == DSTon
     || (DSTmode == DSTmaybe && TclpGetDate((TclpTime_t)&Julian, 0)->tm_isdst))
        Julian -= 60 * 60;
    *TimePtr = Julian;
    return 0;
}


static time_t
DSTcorrect(Start, Future)
    time_t      Start;
    time_t      Future;
{
    time_t      StartDay;
    time_t      FutureDay;

    StartDay = (TclpGetDate((TclpTime_t)&Start, 0)->tm_hour + 1) % 24;
    FutureDay = (TclpGetDate((TclpTime_t)&Future, 0)->tm_hour + 1) % 24;
    return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(Start, DayOrdinal, DayNumber)
    time_t      Start;
    time_t      DayOrdinal;
    time_t      DayNumber;
{
    struct tm   *tm;
    time_t      now;

    now = Start;
    tm = TclpGetDate((TclpTime_t)&now, 0);
    now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
    now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
    return DSTcorrect(Start, now);
}


static int
RelativeMonth(Start, RelMonth, TimePtr)
    time_t Start;
    time_t RelMonth;
    time_t *TimePtr;
{
    struct tm *tm;
    time_t Month;
    time_t Year;
    time_t Julian;
    int result;

    if (RelMonth == 0) {
        *TimePtr = 0;
        return 0;
    }
    tm = TclpGetDate((TclpTime_t)&Start, 0);
    Month = 12 * (tm->tm_year + TM_YEAR_BASE) + tm->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    result = Convert(Month, (time_t) tm->tm_mday, Year,
	    (time_t) tm->tm_hour, (time_t) tm->tm_min, (time_t) tm->tm_sec,
	    MER24, DSTmaybe, &Julian);
    /*
     * The following iteration takes into account the case were we jump
     * into a "short month".  Far example, "one month from Jan 31" will
     * fail because there is no Feb 31.  The code below will reduce the
     * day and try converting the date until we succed or the date equals
     * 28 (which always works unless the date is bad in another way).
     */

    while ((result != 0) && (tm->tm_mday > 28)) {
	tm->tm_mday--;
	result = Convert(Month, (time_t) tm->tm_mday, Year,
		(time_t) tm->tm_hour, (time_t) tm->tm_min, (time_t) tm->tm_sec,
		MER24, DSTmaybe, &Julian);
    }
    if (result != 0) {
	return -1;
    }
    *TimePtr = DSTcorrect(Start, Julian);
    return 0;
}


static int
LookupWord(buff)
    char                *buff;
{
    register char *p;
    register char *q;
    register TABLE *tp;
    int i;
    int abbrev;

    /*
     * Make it lowercase.
     */

    Tcl_UtfToLower(buff);

    if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
        TclDatelval.Meridian = MERam;
        return tMERIDIAN;
    }
    if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
        TclDatelval.Meridian = MERpm;
        return tMERIDIAN;
    }

    /*
     * See if we have an abbreviation for a month.
     */
    if (strlen(buff) == 3) {
        abbrev = 1;
    } else if (strlen(buff) == 4 && buff[3] == '.') {
        abbrev = 1;
        buff[3] = '\0';
    } else {
        abbrev = 0;
    }

    for (tp = MonthDayTable; tp->name; tp++) {
        if (abbrev) {
            if (strncmp(buff, tp->name, 3) == 0) {
                TclDatelval.Number = tp->value;
                return tp->type;
            }
        } else if (strcmp(buff, tp->name) == 0) {
            TclDatelval.Number = tp->value;
            return tp->type;
        }
    }

    for (tp = TimezoneTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            TclDatelval.Number = tp->value;
            return tp->type;
        }
    }

    for (tp = UnitsTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            TclDatelval.Number = tp->value;
            return tp->type;
        }
    }

    /*
     * Strip off any plural and try the units table again.
     */
    i = strlen(buff) - 1;
    if (buff[i] == 's') {
        buff[i] = '\0';
        for (tp = UnitsTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                TclDatelval.Number = tp->value;
                return tp->type;
            }
	}
    }

    for (tp = OtherTable; tp->name; tp++) {
        if (strcmp(buff, tp->name) == 0) {
            TclDatelval.Number = tp->value;
            return tp->type;
        }
    }

    /*
     * Military timezones.
     */
    if (buff[1] == '\0' && !(*buff & 0x80)
	    && isalpha(UCHAR(*buff))) {	/* INTL: ISO only */
        for (tp = MilitaryTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                TclDatelval.Number = tp->value;
                return tp->type;
            }
	}
    }

    /*
     * Drop out any periods and try the timezone table again.
     */
    for (i = 0, p = q = buff; *q; q++)
        if (*q != '.') {
            *p++ = *q;
        } else {
            i++;
	}
    *p = '\0';
    if (i) {
        for (tp = TimezoneTable; tp->name; tp++) {
            if (strcmp(buff, tp->name) == 0) {
                TclDatelval.Number = tp->value;
                return tp->type;
            }
	}
    }
    
    return tID;
}


static int
TclDatelex()
{
    register char       c;
    register char       *p;
    char                buff[20];
    int                 Count;

    for ( ; ; ) {
        while (isspace(UCHAR(*TclDateInput))) {
            TclDateInput++;
	}

        if (isdigit(UCHAR(c = *TclDateInput))) { /* INTL: digit */
            for (TclDatelval.Number = 0;
		    isdigit(UCHAR(c = *TclDateInput++)); ) { /* INTL: digit */
                TclDatelval.Number = 10 * TclDatelval.Number + c - '0';
	    }
            TclDateInput--;
	    if (TclDatelval.Number >= 100000) {
		return tISOBASE;
	    } else {
		return tUNUMBER;
	    }
        }
        if (!(c & 0x80) && isalpha(UCHAR(c))) {	/* INTL: ISO only. */
            for (p = buff; isalpha(UCHAR(c = *TclDateInput++)) /* INTL: ISO only. */
		     || c == '.'; ) {
                if (p < &buff[sizeof buff - 1]) {
                    *p++ = c;
		}
	    }
            *p = '\0';
            TclDateInput--;
            return LookupWord(buff);
        }
        if (c != '(') {
            return *TclDateInput++;
	}
        Count = 0;
        do {
            c = *TclDateInput++;
            if (c == '\0') {
                return c;
	    } else if (c == '(') {
                Count++;
	    } else if (c == ')') {
                Count--;
	    }
        } while (Count > 0);
    }
}

/*
 * Specify zone is of -50000 to force GMT.  (This allows BST to work).
 */

int
TclGetDate(p, now, zone, timePtr)
    char *p;
    unsigned long now;
    long zone;
    unsigned long *timePtr;
{
    struct tm *tm;
    time_t Start;
    time_t Time;
    time_t tod;
    int thisyear;

    TclDateInput = p;
    tm = TclpGetDate((TclpTime_t) &now, 0);
    thisyear = tm->tm_year + TM_YEAR_BASE;
    TclDateYear = thisyear;
    TclDateMonth = tm->tm_mon + 1;
    TclDateDay = tm->tm_mday;
    TclDateTimezone = zone;
    if (zone == -50000) {
        TclDateDSTmode = DSToff;  /* assume GMT */
        TclDateTimezone = 0;
    } else {
        TclDateDSTmode = DSTmaybe;
    }
    TclDateHour = 0;
    TclDateMinutes = 0;
    TclDateSeconds = 0;
    TclDateMeridian = MER24;
    TclDateRelSeconds = 0;
    TclDateRelMonth = 0;
    TclDateHaveDate = 0;
    TclDateHaveDay = 0;
    TclDateHaveRel = 0;
    TclDateHaveTime = 0;
    TclDateHaveZone = 0;

    if (TclDateparse() || TclDateHaveTime > 1 || TclDateHaveZone > 1 || TclDateHaveDate > 1 ||
	    TclDateHaveDay > 1) {
        return -1;
    }
    
    if (TclDateHaveDate || TclDateHaveTime || TclDateHaveDay) {
	if (TclDateYear < 0) {
	    TclDateYear = -TclDateYear;
	}
	/*
	 * The following line handles years that are specified using
	 * only two digits.  The line of code below implements a policy
	 * defined by the X/Open workgroup on the millinium rollover.
	 * Note: some of those dates may not actually be valid on some
	 * platforms.  The POSIX standard startes that the dates 70-99
	 * shall refer to 1970-1999 and 00-38 shall refer to 2000-2038.
	 * This later definition should work on all platforms.
	 */

	if (TclDateYear < 100) {
	    if (TclDateYear >= 69) {
		TclDateYear += 1900;
	    } else {
		TclDateYear += 2000;
	    }
	}
	if (Convert(TclDateMonth, TclDateDay, TclDateYear, TclDateHour, TclDateMinutes, TclDateSeconds,
		TclDateMeridian, TclDateDSTmode, &Start) < 0) {
            return -1;
	}
    } else {
        Start = now;
        if (!TclDateHaveRel) {
            Start -= ((tm->tm_hour * 60L * 60L) +
		    tm->tm_min * 60L) +	tm->tm_sec;
	}
    }

    Start += TclDateRelSeconds;
    if (RelativeMonth(Start, TclDateRelMonth, &Time) < 0) {
        return -1;
    }
    Start += Time;

    if (TclDateHaveDay && !TclDateHaveDate) {
        tod = RelativeDate(Start, TclDateDayOrdinal, TclDateDayNumber);
        Start += tod;
    }

    *timePtr = Start;
    return 0;
}
#define YYABORT goto TclDateabort
#define YYREJECT goto TclDateabort
#define YYACCEPT goto TclDateaccept
#define YYERROR goto TclDateerrlab
int
TclDateparse()
{
    register int TclDatem, TclDaten, TclDatestate;
#if YYDEBUG
    register char *TclDates;
    extern char *getenv();

    if (TclDates = getenv("YYDEBUG"))
    {
        TclDaten = *TclDates;
        if (TclDaten >= '0' && TclDaten <= '9')
            TclDatedebug = TclDaten - '0';
    }
#endif

    TclDatenerrs = 0;
    TclDateerrflag = 0;
    TclDatechar = (-1);

    TclDatessp = TclDatess;
    TclDatevsp = TclDatevs;
    *TclDatessp = TclDatestate = 0;

TclDateloop:
    if ((TclDaten = TclDatedefred[TclDatestate])) goto TclDatereduce;
    if (TclDatechar < 0)
    {
        if ((TclDatechar = TclDatelex()) < 0) TclDatechar = 0;
#if YYDEBUG
        if (TclDatedebug)
        {
            TclDates = 0;
            if (TclDatechar <= YYMAXTOKEN) TclDates = TclDatename[TclDatechar];
            if (!TclDates) TclDates = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, TclDatestate, TclDatechar, TclDates);
        }
#endif
    }
    if ((TclDaten = TclDatesindex[TclDatestate]) && (TclDaten += TclDatechar) >= 0 &&
            TclDaten <= YYTABLESIZE && TclDatecheck[TclDaten] == TclDatechar)
    {
#if YYDEBUG
        if (TclDatedebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, TclDatestate, TclDatetable[TclDaten]);
#endif
        if (TclDatessp >= TclDatess + TclDatestacksize - 1)
        {
            goto TclDateoverflow;
        }
        *++TclDatessp = TclDatestate = TclDatetable[TclDaten];
        *++TclDatevsp = TclDatelval;
        TclDatechar = (-1);
        if (TclDateerrflag > 0)  --TclDateerrflag;
        goto TclDateloop;
    }
    if ((TclDaten = TclDaterindex[TclDatestate]) && (TclDaten += TclDatechar) >= 0 &&
            TclDaten <= YYTABLESIZE && TclDatecheck[TclDaten] == TclDatechar)
    {
        TclDaten = TclDatetable[TclDaten];
        goto TclDatereduce;
    }
    if (TclDateerrflag) goto TclDateinrecovery;
#ifdef lint
    goto TclDatenewerror;
TclDatenewerror:
#endif
    TclDateerror("syntax error");
#ifdef lint
    goto TclDateerrlab;
#endif
    ++TclDatenerrs;
TclDateinrecovery:
    if (TclDateerrflag < 3)
    {
        TclDateerrflag = 3;
        for (;;)
        {
            if ((TclDaten = TclDatesindex[*TclDatessp]) && (TclDaten += YYERRCODE) >= 0 &&
                    TclDaten <= YYTABLESIZE && TclDatecheck[TclDaten] == YYERRCODE)
            {
#if YYDEBUG
                if (TclDatedebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *TclDatessp, TclDatetable[TclDaten]);
#endif
                if (TclDatessp >= TclDatess + TclDatestacksize - 1)
                {
                    goto TclDateoverflow;
                }
                *++TclDatessp = TclDatestate = TclDatetable[TclDaten];
                *++TclDatevsp = TclDatelval;
                goto TclDateloop;
            }
            else
            {
#if YYDEBUG
                if (TclDatedebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *TclDatessp);
#endif
                if (TclDatessp <= TclDatess) goto TclDateabort;
                --TclDatessp;
                --TclDatevsp;
            }
        }
    }
    else
    {
        if (TclDatechar == 0) goto TclDateabort;
#if YYDEBUG
        if (TclDatedebug)
        {
            TclDates = 0;
            if (TclDatechar <= YYMAXTOKEN) TclDates = TclDatename[TclDatechar];
            if (!TclDates) TclDates = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, TclDatestate, TclDatechar, TclDates);
        }
#endif
        TclDatechar = (-1);
        goto TclDateloop;
    }
TclDatereduce:
#if YYDEBUG
    if (TclDatedebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, TclDatestate, TclDaten, TclDaterule[TclDaten]);
#endif
    TclDatem = TclDatelen[TclDaten];
    TclDateval = TclDatevsp[1-TclDatem];
    switch (TclDaten)
    {
case 3:
{
            TclDateHaveTime++;
        }
break;
case 4:
{
            TclDateHaveZone++;
        }
break;
case 5:
{
            TclDateHaveDate++;
        }
break;
case 6:
{
            TclDateHaveDay++;
        }
break;
case 7:
{
            TclDateHaveRel++;
        }
break;
case 8:
{
	    TclDateHaveTime++;
	    TclDateHaveDate++;
	}
break;
case 10:
{
            TclDateHour = TclDatevsp[-1].Number;
            TclDateMinutes = 0;
            TclDateSeconds = 0;
            TclDateMeridian = TclDatevsp[0].Meridian;
        }
break;
case 11:
{
            TclDateHour = TclDatevsp[-3].Number;
            TclDateMinutes = TclDatevsp[-1].Number;
            TclDateSeconds = 0;
            TclDateMeridian = TclDatevsp[0].Meridian;
        }
break;
case 12:
{
            TclDateHour = TclDatevsp[-4].Number;
            TclDateMinutes = TclDatevsp[-2].Number;
            TclDateMeridian = MER24;
            TclDateDSTmode = DSToff;
            TclDateTimezone = (TclDatevsp[0].Number % 100 + (TclDatevsp[0].Number / 100) * 60);
        }
break;
case 13:
{
            TclDateHour = TclDatevsp[-5].Number;
            TclDateMinutes = TclDatevsp[-3].Number;
            TclDateSeconds = TclDatevsp[-1].Number;
            TclDateMeridian = TclDatevsp[0].Meridian;
        }
break;
case 14:
{
            TclDateHour = TclDatevsp[-6].Number;
            TclDateMinutes = TclDatevsp[-4].Number;
            TclDateSeconds = TclDatevsp[-2].Number;
            TclDateMeridian = MER24;
            TclDateDSTmode = DSToff;
            TclDateTimezone = (TclDatevsp[0].Number % 100 + (TclDatevsp[0].Number / 100) * 60);
        }
break;
case 15:
{
            TclDateTimezone = TclDatevsp[-1].Number;
            TclDateDSTmode = DSTon;
        }
break;
case 16:
{
            TclDateTimezone = TclDatevsp[0].Number;
            TclDateDSTmode = DSToff;
        }
break;
case 17:
{
            TclDateTimezone = TclDatevsp[0].Number;
            TclDateDSTmode = DSTon;
        }
break;
case 18:
{
            TclDateDayOrdinal = 1;
            TclDateDayNumber = TclDatevsp[0].Number;
        }
break;
case 19:
{
            TclDateDayOrdinal = 1;
            TclDateDayNumber = TclDatevsp[-1].Number;
        }
break;
case 20:
{
            TclDateDayOrdinal = TclDatevsp[-1].Number;
            TclDateDayNumber = TclDatevsp[0].Number;
        }
break;
case 21:
{
            TclDateMonth = TclDatevsp[-2].Number;
            TclDateDay = TclDatevsp[0].Number;
        }
break;
case 22:
{
            TclDateMonth = TclDatevsp[-4].Number;
            TclDateDay = TclDatevsp[-2].Number;
            TclDateYear = TclDatevsp[0].Number;
        }
break;
case 23:
{
	    TclDateYear = TclDatevsp[0].Number / 10000;
	    TclDateMonth = (TclDatevsp[0].Number % 10000)/100;
	    TclDateDay = TclDatevsp[0].Number % 100;
	}
break;
case 24:
{
	    TclDateDay = TclDatevsp[-4].Number;
	    TclDateMonth = TclDatevsp[-2].Number;
	    TclDateYear = TclDatevsp[0].Number;
	}
break;
case 25:
{
            TclDateMonth = TclDatevsp[-2].Number;
            TclDateDay = TclDatevsp[0].Number;
            TclDateYear = TclDatevsp[-4].Number;
        }
break;
case 26:
{
            TclDateMonth = TclDatevsp[-1].Number;
            TclDateDay = TclDatevsp[0].Number;
        }
break;
case 27:
{
            TclDateMonth = TclDatevsp[-3].Number;
            TclDateDay = TclDatevsp[-2].Number;
            TclDateYear = TclDatevsp[0].Number;
        }
break;
case 28:
{
            TclDateMonth = TclDatevsp[0].Number;
            TclDateDay = TclDatevsp[-1].Number;
        }
break;
case 29:
{
	    TclDateMonth = 1;
	    TclDateDay = 1;
	    TclDateYear = EPOCH;
	}
break;
case 30:
{
            TclDateMonth = TclDatevsp[-1].Number;
            TclDateDay = TclDatevsp[-2].Number;
            TclDateYear = TclDatevsp[0].Number;
        }
break;
case 31:
{
            if (TclDatevsp[-1].Number != HOUR(- 7)) goto TclDateabort;
	    TclDateYear = TclDatevsp[-2].Number / 10000;
	    TclDateMonth = (TclDatevsp[-2].Number % 10000)/100;
	    TclDateDay = TclDatevsp[-2].Number % 100;
	    TclDateHour = TclDatevsp[0].Number / 10000;
	    TclDateMinutes = (TclDatevsp[0].Number % 10000)/100;
	    TclDateSeconds = TclDatevsp[0].Number % 100;
        }
break;
case 32:
{
            if (TclDatevsp[-5].Number != HOUR(- 7)) goto TclDateabort;
	    TclDateYear = TclDatevsp[-6].Number / 10000;
	    TclDateMonth = (TclDatevsp[-6].Number % 10000)/100;
	    TclDateDay = TclDatevsp[-6].Number % 100;
	    TclDateHour = TclDatevsp[-4].Number;
	    TclDateMinutes = TclDatevsp[-2].Number;
	    TclDateSeconds = TclDatevsp[0].Number;
        }
break;
case 33:
{
	    TclDateYear = TclDatevsp[-1].Number / 10000;
	    TclDateMonth = (TclDatevsp[-1].Number % 10000)/100;
	    TclDateDay = TclDatevsp[-1].Number % 100;
	    TclDateHour = TclDatevsp[0].Number / 10000;
	    TclDateMinutes = (TclDatevsp[0].Number % 10000)/100;
	    TclDateSeconds = TclDatevsp[0].Number % 100;
        }
break;
case 34:
{
            TclDateRelSeconds = -TclDateRelSeconds;
            TclDateRelMonth = -TclDateRelMonth;
        }
break;
case 36:
{
            TclDateRelSeconds += TclDatevsp[-1].Number * TclDatevsp[0].Number * 60L;
        }
break;
case 37:
{
            TclDateRelSeconds -= TclDatevsp[-1].Number * TclDatevsp[0].Number * 60L;
        }
break;
case 38:
{
            TclDateRelSeconds += TclDatevsp[0].Number * 60L;
        }
break;
case 39:
{
            TclDateRelSeconds -= TclDatevsp[-1].Number;
        }
break;
case 40:
{
            TclDateRelSeconds += TclDatevsp[-1].Number;
        }
break;
case 41:
{
            TclDateRelSeconds++;
        }
break;
case 42:
{
            TclDateRelMonth -= TclDatevsp[-1].Number * TclDatevsp[0].Number;
        }
break;
case 43:
{
            TclDateRelMonth += TclDatevsp[-1].Number * TclDatevsp[0].Number;
        }
break;
case 44:
{
            TclDateRelMonth += TclDatevsp[0].Number;
        }
break;
case 45:
{
	if (TclDateHaveTime && TclDateHaveDate && !TclDateHaveRel) {
	    TclDateYear = TclDatevsp[0].Number;
	} else {
	    TclDateHaveTime++;
	    if (TclDatevsp[0].Number < 100) {
		TclDateHour = TclDatevsp[0].Number;
		TclDateMinutes = 0;
	    } else {
		TclDateHour = TclDatevsp[0].Number / 100;
		TclDateMinutes = TclDatevsp[0].Number % 100;
	    }
	    TclDateSeconds = 0;
	    TclDateMeridian = MER24;
	}
    }
break;
case 46:
{
            TclDateval.Meridian = MER24;
        }
break;
case 47:
{
            TclDateval.Meridian = TclDatevsp[0].Meridian;
        }
break;
    }
    TclDatessp -= TclDatem;
    TclDatestate = *TclDatessp;
    TclDatevsp -= TclDatem;
    TclDatem = TclDatelhs[TclDaten];
    if (TclDatestate == 0 && TclDatem == 0)
    {
#if YYDEBUG
        if (TclDatedebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        TclDatestate = YYFINAL;
        *++TclDatessp = YYFINAL;
        *++TclDatevsp = TclDateval;
        if (TclDatechar < 0)
        {
            if ((TclDatechar = TclDatelex()) < 0) TclDatechar = 0;
#if YYDEBUG
            if (TclDatedebug)
            {
                TclDates = 0;
                if (TclDatechar <= YYMAXTOKEN) TclDates = TclDatename[TclDatechar];
                if (!TclDates) TclDates = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, TclDatechar, TclDates);
            }
#endif
        }
        if (TclDatechar == 0) goto TclDateaccept;
        goto TclDateloop;
    }
    if ((TclDaten = TclDategindex[TclDatem]) && (TclDaten += TclDatestate) >= 0 &&
            TclDaten <= YYTABLESIZE && TclDatecheck[TclDaten] == TclDatestate)
        TclDatestate = TclDatetable[TclDaten];
    else
        TclDatestate = TclDatedgoto[TclDatem];
#if YYDEBUG
    if (TclDatedebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *TclDatessp, TclDatestate);
#endif
    if (TclDatessp >= TclDatess + TclDatestacksize - 1)
    {
        goto TclDateoverflow;
    }
    *++TclDatessp = TclDatestate;
    *++TclDatevsp = TclDateval;
    goto TclDateloop;
TclDateoverflow:
    TclDateerror("yacc stack overflow");
TclDateabort:
    return (1);
TclDateaccept:
    return (0);
}
