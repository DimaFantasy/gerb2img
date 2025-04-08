/*
 *
/*/
%{

#include "gerber.h"

%}
 
%union
{
	char * YS_string;
	double 	YS_float;
	int 	YS_int;
	NodeT	*YS_NodeT;
};


%defines
%parse-param {Gerber *g}
%lex-param {Gerber *g}
%error-verbose
%token-table

%token <YS_int> PARAMETER_ADD
%token PARAMETER_AM
%token PARAMETER
%token <YS_float> NUMBER
%token <YS_int> VARIABLE
%token <YS_int> CODE
%token <YS_string> SYMBOL_X
%token MACRONAME

%left '+' '-'
%left 'X' '/'
%nonassoc UNARY

%type <YS_NodeT> expr;
%type <YS_float> ad_number;

%%

gerber 				: 	data_block
					| 	gerber data_block

data_block 			:	'%' paramRS274_list '%'
				  	|	'%' PARAMETER_AM  MACRONAME am_primitive_list  asterisk  '%'
					|	command_list '*' { g->processDataBlock(); }						// Always process data block immediately after first asterisk
					| 	'*'																// Allow empty data blocks
					|	'%'																// Allow empty parameter blocks
//					|	{ throw string("empty file");}



paramRS274_list		:	paramRS274 asterisk
					|	paramRS274_list paramRS274 asterisk

paramRS274 			:	PARAMETER
					|	PARAMETER_ADD MACRONAME ',' ad_modifier_list { g->process_AD_block($1);  }
					|	PARAMETER_ADD MACRONAME { g->process_AD_block($1);  }

ad_modifier_list 	:	ad_number						{ g->variables.push_back($1); }
					|	ad_modifier_list 'X' ad_number	{ g->variables.push_back($3); }
					|	ad_modifier_list 'X'			{ g->warning("modified expected after X"); }

ad_number			: 	'-' NUMBER { $$ = -$2; }
					| 	'+' NUMBER { $$ = $2; }
					| 	NUMBER

am_primitive_list	: 	asterisk am_primitve
					| 	am_primitive_list asterisk am_primitve

am_primitve			: 	NUMBER ',' expr_list
					{
						g->macro_apertures.push_back(Aperture());
						g->macro_apertures.back().primitive = Aperture::PRIMITIVE( int($1) );
						g->macro_apertures.back().parameter = g->temporaryParameters;
						g->macro_apertures.back().nameMacro = g->temporaryNameMacro;
						g->macro_apertures.back().linenum_at_definition = g->currentLine;
						g->temporaryParameters.clear();
					}

expr_list 			:  	expr				{ g->temporaryParameters.push_back($1); }
					|  	expr_list ',' expr	{ g->temporaryParameters.push_back($3); }

expr				: 	VARIABLE  	{ $$ = new NodeT(NodeT::VAR		, &$1, &g->variables); }
					| 	NUMBER		{ $$ = new NodeT(NodeT::CONSTANT, &$1); }
					| 	expr '+' expr	{ $$ = new NodeT(NodeT::OPADD	, $1, $3); }
					| 	expr '-' expr	{ $$ = new NodeT(NodeT::OPSUB	, $1, $3); }
					| 	expr 'X' expr	{ $$ = new NodeT(NodeT::OPMUL	, $1, $3); }
					| 	expr '/' expr	{ $$ = new NodeT(NodeT::OPDIV	, $1, $3); }
					| 	'-' expr 	%prec UNARY	{ $$ = new NodeT(NodeT::OPNEG, $2); }
					| 	'+' expr 	%prec UNARY	{ $$ = $2; }
					| 	'(' expr ')'	{ $$ = $2; }

command_list		: 	command
					| 	command_list command;

command				: 	'D' NUMBER { g->process_D_command(int($2)); }
					| 	'G' NUMBER { g->process_G_command(int($2)); }
					| 	'M' NUMBER {  }
					| 	CODE								// a general token handled by yylex

asterisk			: 	'*'									// allow an asterisk can be one or more consecutive '*'
					|	asterisk  '*'

%%

