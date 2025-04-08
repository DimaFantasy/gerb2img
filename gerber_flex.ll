/*
 * Flex rules for RS274X files
/*/

%{
#include <string.h>
#include "./gerber.h"
#include "gerber_bison.h"


#define YY_DECL 	int yylex(Gerber *g)

#define YY_USER_ACTION  \
	if (g->repeat.X > 1 && g->repeat.Y > 1) \
	{ \
		g->repeat.buffer += yytext; \
	}

/*  
 *
 *
 */
void numberAfterChar(const char * str, char c, double *data, double multiplier=1)
{
	char *endptr;
	char *p = (char *)strchr((char *)str, c);
	if (p == 0) return;
	double x = strtod( p+1, &endptr);
	if (endptr == str) return;
	*data = x * multiplier;
}
%}

%option		header-file="lex.yy.h"
%option 	stack
%option 	noyywrap
%option 	posix-compat

/* A condition when extracting aperture macro names
 * The AMblock condition is required so send arithmatic operators  '+', '-', 'X' and '/' to the parser
 * The ADblock condition is required to send modifier delimeter character 'X' to the parser 
 */
%x	ADblock
%x	AMblock
%x 	macroname


/* ------------------------------------------------------------------------------------------------------------------
 *							token rules for RS274X file
 IJ[^*]*						{	g->layerName = yytext[2]; return PARAMETER; 	} 		// Image Justify
 *
 */
%%
<*>[ \t\r]						{ }										// root out all white spaces, CR
<*>\n							{	g->currentLine++;					// count the LF and hide from parser
							}
<INITIAL,ADblock,AMblock>[0-9]*\.?[0-9]+\.?[0-9]*	{				// get floating point number, and extract extranous '.' followed by digits.
								yylval.YS_float   = atof(yytext);	// Evaluate number but ignore '+' or '-' prefix as it handler in yylex()
								if (strchr(yytext,'.') != strrchr(yytext,'.') )  g->warning("extraneous '.' in number");
								return NUMBER;
							}
<AMblock>\$[0-9]+			{
								yylval.YS_int  = atoi(yytext+1)-1;		// $n Variables identifiers
								if (yylval.YS_int < 0 ) throw string("variable placeholder must be >= 1");
								return VARIABLE;
							}
AM							{	yy_push_state(macroname);				// AM paramater syntax
								BEGIN(AMblock);							// goto <AMblock> condition, exits on next '%' tocken, see below
								yy_push_state(macroname);
								return PARAMETER_AM;
							}
ADD[0-9]+  					{	yylval.YS_int = atoi(yytext+3);		// ADD paramater syntax (return D code in value)
								BEGIN(ADblock);					// goto <ADblock> condition, exits on next '*' tocken, see below
								yy_push_state(macroname);
								return PARAMETER_ADD;
							}
<macroname>[A-Za-z_]+[A-Za-z_0-9]*	{								// a string of the Aperture macro name
								g->temporaryNameMacro = yytext;				// save string
								yy_pop_state();
								return MACRONAME;
							}
FS.*X[0-9][0-9].*Y[0-9][0-9] 	{  							// FS Format Statement
								(strchr( yytext,'L') == 0 ) ? g->isOmitLeadingZeroes = false : g->isOmitLeadingZeroes = true;
								(strchr( yytext,'A') == 0 ) ? g->isCoordsAbsolute = false : g->isCoordsAbsolute = true;
								g->coordsInts[0] = *(strchr(yytext,'X')+1) - '0';
								g->coordsDecimals[0] = *(strchr(yytext,'X')+2) - '0';
								g->coordsInts[1] = *(strchr(yytext,'Y')+1) - '0';
								g->coordsDecimals[1] = *(strchr(yytext,'Y')+2) - '0';
								return PARAMETER;
							}
IF[^*]+\*					{															// IF, Include file.  Don't return to parser
								yytext[yyleng-1]=0;			// remove trailing '*'
								yyin = fopen(yytext+2,"r");
								if ( ! yyin)	throw string("cannot open include file '")+(yytext+2)+"'";
								yypush_buffer_state(yy_create_buffer( yyin, YY_BUF_SIZE ));
							}
G0*4[^*]*					{	return CODE; }											// G04 command. Skip remainder of data block.
ASAXBY						{	g->isAxisSwapped = false; return PARAMETER; }			// Axis Swap
ASAYBX						{	g->isAxisSwapped = true;
								g->warning("Ignoring AS (Axis Swap) parameter. A axis = X data, B axis = Y data.");
								return PARAMETER;
							}
SR([XYIJ][-+]?[0-9]*\.?[0-9]+)+ 	{
//							cout << "\nlast SR block "<<g->repeat.buffer<<"\n";
							g->repeat.I = 0;
							g->repeat.J = 0;
							double x=1,y=1;
							numberAfterChar(yytext, 'X', &x);
							numberAfterChar(yytext, 'Y', &y);
							g->repeat.X = int(x);
							g->repeat.Y = int(y);
							numberAfterChar(yytext, 'I', &g->repeat.I);
							numberAfterChar(yytext, 'J', &g->repeat.J);
							g->repeat.buffer.erase();
							g->warning("Ignoring SR (Step and Repeat) parameter");
							return PARAMETER;
							}

AD							{	return PARAMETER; 	}									// ignores empty AD parameter blocks
KO							{	throw string(yytext)+" KO Knockout parameter not supported";  }	// Reason is becuase the standard does not define KO properly
LPD							{ 	g->layerPolarityClear = false;  return PARAMETER; }		// Layer Polarity (draw dark)
LPC							{ 	g->layerPolarityClear = true; return PARAMETER; }		// Layer Polarity (draw clear)
IPPOS						{	g->imagePolarityDark = true; return PARAMETER; }		// Image Polarity set to dark
IPNEG						{	g->imagePolarityDark = false; return PARAMETER; }		// Image Polarity set to clear
MOIN						{ 	g->units = Gerber::INCH;  return PARAMETER; }  					// assign dimensions in inches 
MOMM						{ 	g->units = Gerber::MILLIMETER; return PARAMETER; }				// assign dimensions in millimetres
MI(A[01])?(B[01])?			{	bool preA = g->isMirrorAaxis;							// specify mirroring of A axis and/or B axis
								bool preB = g->isMirrorBaxis;
								if (strstr(yytext, "A0") != 0) g->isMirrorAaxis = false;
								if (strstr(yytext, "B0") != 0) g->isMirrorBaxis = false;
								if (strstr(yytext, "A1") != 0) g->isMirrorAaxis = true;
								if (strstr(yytext, "B1") != 0) g->isMirrorBaxis = true;
								if (preA != g->isMirrorAaxis)  g->scaleFactor[0] *= -1;
								if (preB != g->isMirrorBaxis)  g->scaleFactor[1] *= -1;
								return PARAMETER;
							}
M[0-9]+						{   int i = atoi(yytext+1);
								if (i == 2 ) { 	g->loadDefaults(); return '*'; }		// M2 reset Gerber parameters and continue reading
								if (i == 3 ) return 0;									// M3 stop reading program
								if (i <= 1 ) return '*';								// M0 / M1 just ignore, and continue reading as normal
								throw "Unknown "+string(yytext)+" code";
							}
X[-+]?[0-9]+				{  	g->X = g->getCoordinate(yytext+1, 0); return CODE; }
Y[-+]?[0-9]+				{  	g->Y = g->getCoordinate(yytext+1, 1); return CODE; }
I[-+]?[0-9]+				{  	g->I = g->getCoordinate(yytext+1, 0, true); return CODE; }
J[-+]?[0-9]+				{  	g->J = g->getCoordinate(yytext+1, 1, true); return CODE; }
(OF|IO)([AB][-+]?[0-9]*\.?[0-9]+)+  {														// Image Offset (treat OF and IO synonymously as RS273X Standard doesn't define)
								numberAfterChar(yytext, 'A', &g->imageOffsetPixels[0], g->dotsPerUnit() );
								numberAfterChar(yytext, 'B', &g->imageOffsetPixels[1], g->dotsPerUnit() );
								return PARAMETER;
							}
SF([AB][-+]?[0-9]*\.?[0-9]+)+  {														// Scale Factor
								numberAfterChar(yytext, 'A', &g->scaleFactor[0], g->optScaleX * (g->isMirrorAaxis ? -1 : 1) );
								numberAfterChar(yytext, 'B', &g->scaleFactor[1], g->optScaleY * (g->isMirrorBaxis ? -1 : 1) );
								return PARAMETER;
							}

IR[-+]?[0-9]*\.?[0-9]+		{	g->imageRotate = -atof(yytext+2)*M_PI/180.0; return PARAMETER; }	// Image Rotate about origin in degreese
LN[^*]*						{	g->layerName = yytext[2]; return PARAMETER; 	}		// Layer Name
IN[^*]*						{	g->imageName = yytext[2]; return PARAMETER; 	}		// Image Name
PF[^*]*						{	g->imageFilm = yytext[2]; return PARAMETER; 	}		// Image Film string for the operator 
(IJ|IC|KO)[^*]*				{	g->warning("ignoring parameter '%c%c'",yytext[0],yytext[1]); return PARAMETER; 	}									// safely ignore all these parameters
<AMblock>[%]				{	BEGIN(0); return yytext[0]; }							// '%' causes to exit AM blocks (note: '*' does not and must not end AM blocks)
<ADblock>\*					{	BEGIN(0); return yytext[0]; }							// '*' causes to exit AD blocks (possible subsequent AD blocks before '%')
<*>[A-Za-z0-9\+%\-/(),\*]	{	return toupper(yytext[0]); }							// return all other valid single characters
<*>.						{ }															// ignore all invalid characters
<<EOF>> 					{
								yypop_buffer_state();
								if ( !YY_CURRENT_BUFFER )
								{
									yyterminate();
								}
							}

%%

