/* A Bison parser, made from query_parser_bison.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	IDENT	257
# define	STRING	258
# define	NUM_INT	259
# define	AND	260
# define	NOT	261
# define	OR	262
# define	SUB	263
# define	XOR	264
# define	RANGE	265
# define	EQ	266
# define	NOTEQ	267
# define	GT	268
# define	GE	269
# define	LT	270
# define	LE	271
# define	BETWEEN	272

#line 33 "query_parser_bison.y"


#define YYSTYPE         CQueryParseTree::TNode*
#define YYPARSE_PARAM   parm
#define YYLEX_PARAM     parm
#define YYMAXDEPTH      100000

#define YYDEBUG         1
#define YYERROR_VERBOSE 1


/// Add child node(s) to the parent
/// @internal
///
inline static
void QTreeAddNode(void*                   parm,
                  CQueryParseTree::TNode* rnode,
                  CQueryParseTree::TNode* node1,
                  CQueryParseTree::TNode* node2)
{
    if (node1)
        rnode->AddNode(node1);
    if (node2)
        rnode->AddNode(node2);
        
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    env->AttachQueryTree(rnode);    
    env->ForgetPoolNodes(node1, node2);
}



#ifndef YYSTYPE
# define YYSTYPE int
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		55
#define	YYFLAG		-32768
#define	YYNTBASE	21

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 272 ? yytranslate[x] : 24)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      19,    20,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     6,     8,    10,    13,    16,    20,
      24,    28,    32,    36,    42,    46,    50,    54,    58,    62,
      66,    70,    74,    78,    81,    84,    87,    90,    93,    97,
     101,   105,   109,   113
};
static const short yyrhs[] =
{
      23,     0,     5,     0,     4,     0,     3,     0,    22,     0,
       4,     3,     0,    23,    23,     0,    19,    23,    20,     0,
      23,     6,    23,     0,    23,     9,    23,     0,    23,     8,
      23,     0,    23,    10,    23,     0,    22,    18,    22,     6,
      22,     0,    23,    11,    23,     0,    23,    12,    23,     0,
      23,    13,    23,     0,    23,    14,    23,     0,    23,    15,
      23,     0,    23,    16,    23,     0,    23,    17,    23,     0,
      19,    23,    20,     0,    23,     7,    23,     0,     7,    23,
       0,     1,     4,     0,     1,     3,     0,     1,     5,     0,
      23,     1,     0,    19,    23,     1,     0,    23,     8,     1,
       0,    23,    10,     1,     0,    23,     7,     1,     0,    23,
       6,     1,     0,    23,     9,     1,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,    91,   101,   108,   113,   119,   124,   133,   144,   154,
     159,   164,   169,   174,   188,   193,   198,   202,   206,   210,
     214,   218,   223,   228,   237,   246,   255,   264,   276,   286,
     296,   306,   316,   326
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "IDENT", "STRING", "NUM_INT", "AND", "NOT", 
  "OR", "SUB", "XOR", "RANGE", "EQ", "NOTEQ", "GT", "GE", "LT", "LE", 
  "BETWEEN", "'('", "')'", "input", "scalar_value", "exp", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    21,    22,    22,    22,    23,    23,    23,    23,    23,
      23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
      23,    23,    23,    23,    23,    23,    23,    23,    23,    23,
      23,    23,    23,    23
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     1,     1,     1,     2,     2,     3,     3,
       3,     3,     3,     5,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     2,     2,     2,     2,     2,     3,     3,
       3,     3,     3,     3
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,     4,     3,     2,     0,     0,     5,     0,    25,
      24,    26,     6,     0,     0,     0,    27,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      27,     8,     3,     0,    32,     0,    31,     0,    29,     0,
      33,     0,    30,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    13,     0,     0,     0
};

static const short yydefgoto[] =
{
      53,     7,    29
};

static const short yypact[] =
{
     365,    -1,-32768,    11,-32768,   365,   365,    -3,   328,-32768,
  -32768,-32768,-32768,    34,   345,     8,    -1,   370,   375,   392,
     397,   402,   365,   365,   365,   365,   365,   365,   365,    55,
       5,-32768,-32768,    10,    -1,    76,    -1,    97,    -1,   118,
      -1,   139,    -1,   160,   181,   202,   223,   244,   265,   286,
     307,     8,-32768,    17,    30,-32768
};

static const short yypgoto[] =
{
  -32768,   -15,     1
};


#define	YYLAST		421


static const short yytable[] =
{
      33,     8,     9,    10,    11,   -28,    13,    14,     9,    10,
      11,     2,    32,     4,    12,    15,    51,    54,    35,    37,
      39,    41,    43,    44,    45,    46,    47,    48,    49,    50,
      55,     0,     0,     0,   -23,    16,    52,     2,     3,     4,
     -23,   -23,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,     0,     6,   -23,    -7,    16,     0,     2,     3,
       4,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,     0,     6,    -7,    -9,    16,     0,     2,
       3,     4,    -9,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,     0,     6,    -9,   -22,    16,     0,
       2,     3,     4,   -22,   -22,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,     0,     6,   -22,   -11,    16,
       0,     2,     3,     4,   -11,   -11,   -11,    20,    21,    22,
      23,    24,    25,    26,    27,    28,     0,     6,   -11,   -10,
      16,     0,     2,     3,     4,   -10,   -10,   -10,   -10,    21,
      22,    23,    24,    25,    26,    27,    28,     0,     6,   -10,
     -12,    16,     0,     2,     3,     4,   -12,   -12,   -12,   -12,
     -12,    22,    23,    24,    25,    26,    27,    28,     0,     6,
     -12,   -14,    16,     0,     2,     3,     4,   -14,   -14,   -14,
     -14,   -14,   -14,    23,    24,    25,    26,    27,    28,     0,
       6,   -14,   -15,    16,     0,     2,     3,     4,   -15,   -15,
     -15,   -15,   -15,   -15,   -15,    24,    25,    26,    27,    28,
       0,     6,   -15,   -16,    16,     0,     2,     3,     4,   -16,
     -16,   -16,   -16,   -16,   -16,   -16,   -16,    25,    26,    27,
      28,     0,     6,   -16,   -17,    16,     0,     2,     3,     4,
     -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,   -17,    26,
      27,    28,     0,     6,   -17,   -18,    16,     0,     2,     3,
       4,   -18,   -18,   -18,   -18,   -18,   -18,   -18,   -18,   -18,
     -18,    27,    28,     0,     6,   -18,   -19,    16,     0,     2,
       3,     4,   -19,   -19,   -19,   -19,   -19,   -19,   -19,   -19,
     -19,   -19,   -19,    28,     0,     6,   -19,   -20,    16,     0,
       2,     3,     4,   -20,   -20,   -20,   -20,   -20,   -20,   -20,
     -20,   -20,   -20,   -20,   -20,     0,     6,   -20,    -1,    16,
       0,     2,     3,     4,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    30,     6,     2,     3,
       4,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,     0,     6,    31,     1,     0,     2,     3,
       4,    34,     5,     2,     3,     4,    36,     5,     2,     3,
       4,     0,     5,     0,     6,     0,     0,     0,     0,     6,
       0,     0,     0,    38,     6,     2,     3,     4,    40,     5,
       2,     3,     4,    42,     5,     2,     3,     4,     0,     5,
       0,     6,     0,     0,     0,     0,     6,     0,     0,     0,
       0,     6
};

static const short yycheck[] =
{
      15,     0,     3,     4,     5,     0,     5,     6,     3,     4,
       5,     3,     4,     5,     3,    18,     6,     0,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
       0,    -1,    -1,    -1,     0,     1,    51,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    -1,    19,    20,     0,     1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    -1,    19,    20,     0,     1,    -1,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    -1,    19,    20,     0,     1,    -1,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    -1,    19,    20,     0,     1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    -1,    19,    20,     0,
       1,    -1,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    -1,    19,    20,
       0,     1,    -1,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    -1,    19,
      20,     0,     1,    -1,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    -1,
      19,    20,     0,     1,    -1,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      -1,    19,    20,     0,     1,    -1,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    -1,    19,    20,     0,     1,    -1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    -1,    19,    20,     0,     1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    -1,    19,    20,     0,     1,    -1,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    -1,    19,    20,     0,     1,    -1,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    -1,    19,    20,     0,     1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,     1,    19,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    -1,    19,    20,     1,    -1,     3,     4,
       5,     1,     7,     3,     4,     5,     1,     7,     3,     4,
       5,    -1,     7,    -1,    19,    -1,    -1,    -1,    -1,    19,
      -1,    -1,    -1,     1,    19,     3,     4,     5,     1,     7,
       3,     4,     5,     1,     7,     3,     4,     5,    -1,     7,
      -1,    19,    -1,    -1,    -1,    -1,    19,    -1,    -1,    -1,
      -1,    19
};
#define YYPURE 1

/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 2:
#line 104 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 3:
#line 109 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 4:
#line 114 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 5:
#line 121 "query_parser_bison.y"
{
    ;
    break;}
case 6:
#line 125 "query_parser_bison.y"
{
    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eFieldSearch, yyvsp[-1], yyvsp[0]);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;
    break;}
case 7:
#line 134 "query_parser_bison.y"
{
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eAnd, yyvsp[-1], yyvsp[0]);
        yyval->GetValue().SetExplicit(false);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;
    break;}
case 8:
#line 145 "query_parser_bison.y"
{
        yyval = yyvsp[-1];
    ;
    break;}
case 9:
#line 155 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 10:
#line 160 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 11:
#line 165 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 12:
#line 170 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 13:
#line 175 "query_parser_bison.y"
{
        yyval = yyvsp[-3];
        yyval->AddNode(yyvsp[-4]);
        yyval->AddNode(yyvsp[-2]);
        yyval->AddNode(yyvsp[0]);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-2]);
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[0]);
    ;
    break;}
case 14:
#line 189 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 15:
#line 194 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 16:
#line 199 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 17:
#line 203 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 18:
#line 207 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 19:
#line 211 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 20:
#line 215 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 21:
#line 219 "query_parser_bison.y"
{ 
        yyval = yyvsp[-1];
    ;
    break;}
case 22:
#line 224 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 23:
#line 229 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[0], 0);
    ;
    break;}
case 24:
#line 238 "query_parser_bison.y"
{
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 25:
#line 247 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 26:
#line 256 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 27:
#line 265 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;
    break;}
case 28:
#line 277 "query_parser_bison.y"
{ 
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. Unbalanced parenthesis");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;
    break;}
case 29:
#line 287 "query_parser_bison.y"
{ 
        yyerrok; 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 30:
#line 297 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 31:
#line 307 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 32:
#line 317 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 33:
#line 327 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 349 "query_parser_bison.y"


