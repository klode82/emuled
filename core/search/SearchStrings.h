// LINUX: search parser messages, in English (translations go to the web interface). The text is that of emule.rc.
// So Parser.y and Scanner.l stay identical to the original: GetResString(IDS_X) returns the text itself.
#pragma once
#include "StdAfx.h"
#define IDS_SEARCH_UNTERMSTRING "Unterminated string"
#define IDS_SEARCH_GENERALERROR "General error"
#define IDS_SEARCH_MISSINGORRIGHT "Missing right side for OR operator"
#define IDS_SEARCH_MISSINGNOTRIGHT "Missing right side for NOT operator"
#define IDS_SEARCH_MISSINGANDRIGHT "Missing right side for AND operator"
#define IDS_SEARCH_MISSINGEXPRPARANT "Missing expression for '(' ')'"
#define IDS_SEARCH_MISSINGCLOSINGPARANT "Missing closing ')'"
#define IDS_SEARCH_EXPRERROR "Failed to parse search expression"
#define IDS_SEARCH_TOOCOMPLEX "Expression too complex"
#define IDS_SEARCH_UNKATTR "Unknown search attribute '%s'"
#define IDS_SEARCH_ATTRERR "Syntax error in '%s' search attribute"
#define IDS_SEARCH_OPRERR "Syntax error in comparison operator '%s'"
#define IDS_SEARCH_BADOPERATORCOMBINATION "Bad combination of operators: with a Kad search the keyword cannot be part of an OR or of the right side of a NOT"
#define IDS_ERR_BADED2KLINK "Ill-formed eD2K link"
#include "ResString.h"
