/*
  Copyright (C) 2015-2016 Open Meridian Project

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LEXERKOD_LEXER_H_
#define LEXERKOD_LEXER_H_

#include "StdAfx.h"

#include "Scintilla.h"
#include "ILexer.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "LexerModule.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "Platform.h"
#include "SciLexer.h"
#include "CharClassify.h"
#include "PropSetSimple.h"

#include "PluginInterface.h"
#include "Version.h"
#include "OptionSet.h"
#include "SparseState.h"
#include "SubStyles.h"
#include "StyleContext.h"

// Kod style defines
#define SCE_KOD_DEFAULT      0 // 0 SCE_C_DEFAULT
#define SCE_KOD_IDENTIFIER   1 // DEFAULT 11 SCE_C_IDENTIFIER
#define SCE_KOD_NUMBER       2 // NUMBER 4 SCE_C_NUMBER
#define SCE_KOD_OPERATOR     3 // OPERATOR 10 SCE_C_OPERATOR
#define SCE_KOD_STRING       4 // STRING 6 SCE_C_STRING
#define SCE_KOD_PREPROCESSOR 5 // PREPROCESSOR 9 SCE_C_PREPROCESSOR

#define SCE_KOD_KEYWORD      6 // KEYWORDS 5 SCE_C_WORD
#define SCE_KOD_CCALL        7 // C CALLS 16 SCE_C_WORD2
#define SCE_KOD_SENDMSG      8 // SENDMESSAGE SCE_C_WORD3
#define SCE_KOD_WORDOPS      9 // SCE_C_WORD3

#define SCE_KOD_MESSAGE      10 // VERBATIM 13 SCE_C_VERBATIM
#define SCE_KOD_CLASS        11
#define SCE_KOD_CONSTANT     12
#define SCE_KOD_PARAMETER    13

#define SCE_KOD_COMMENT      15 // COMMENT 1 SCE_C_COMMENT
#define SCE_KOD_COMMENTLINE  16 // COMMENT LINE 2 SCE_C_COMMENTLINE
#define SCE_KOD_COMMENTDOC   17 // COMMENT DOC 3
#define SCE_KOD_COMMENTLINEDOC 18  // COMMENT LINE DOC 15
#define SCE_KOD_COMMENTDOCKEYWORD 19 // COMMENT DOC KEYWORD 17
#define SCE_KOD_COMMENTDOCKEYWORDERROR 20 // COMMENT DOC KEYWORD ERROR 18

#define SCE_KOD_ESCAPESEQUENCE 21 // SCE_C_ESCAPESEQUENCE 27
#define SCE_KOD_USERLITERAL    22 // SCE_C_USERLITERAL 25
#define SCE_KOD_TASKMARKER     23 // SCE_C_TASKMARKER 26
#define SCE_KOD_STRINGEOL      24 //SCE_C_STRINGEOL

#endif
