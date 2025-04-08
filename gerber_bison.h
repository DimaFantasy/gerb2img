

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     PARAMETER_ADD = 258,
     PARAMETER_AM = 259,
     PARAMETER = 260,
     NUMBER = 261,
     VARIABLE = 262,
     CODE = 263,
     SYMBOL_X = 264,
     MACRONAME = 265,
     UNARY = 266
   };
#endif
/* Tokens.  */
#define PARAMETER_ADD 258
#define PARAMETER_AM 259
#define PARAMETER 260
#define NUMBER 261
#define VARIABLE 262
#define CODE 263
#define SYMBOL_X 264
#define MACRONAME 265
#define UNARY 266




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1685 of yacc.c  */
#line 11 "src/gerber_bison.yy"

	char * YS_string;
	double 	YS_float;
	int 	YS_int;
	NodeT	*YS_NodeT;



/* Line 1685 of yacc.c  */
#line 82 "src/gerber_bison.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


