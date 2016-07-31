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

#include "LexerKod.h"

extern HWND g_NppWindow;

using namespace std;

// Use an unnamed namespace to protect these functions and classes from name conflicts.
namespace {

#pragma region Lexing Helper Functions

// All from CPP lexer.
bool IsSpaceEquiv(int state)
{ 
   return ((state >= SCE_KOD_COMMENT && state <= SCE_KOD_COMMENTDOCKEYWORDERROR)
         || state == SCE_KOD_DEFAULT);
}

// Preconditions: sc.currentPos points to a character after '+' or '-'.
// The test for pos reaching 0 should be redundant,
// and is in only for safety measures.
// Limitation: this code will give the incorrect answer for code like
// a = b+++/ptn/...
// Putting a space between the '++' post-inc operator and the '+' binary op
// fixes this, and is highly recommended for readability anyway.
bool FollowsPostfixOperator(StyleContext &sc, LexAccessor &styler)
{
   int pos = (int)sc.currentPos;

   while (--pos > 0)
   {
      char ch = styler[pos];
      if (ch == '+' || ch == '-')
         return styler[pos - 1] == ch;
   }

   return false;
}

bool FollowsReturnKeyword(StyleContext &sc, LexAccessor &styler)
{
   // Don't look at styles, so no need to flush.
   int pos = (int)sc.currentPos;
   int currentLine = styler.GetLine(pos);
   int lineStartPos = styler.LineStart(currentLine);
   while (--pos > lineStartPos)
   {
      char ch = styler.SafeGetCharAt(pos);
      if (ch != ' ' && ch != '\t')
         break;
   }
   const char *retBack = "nruter";
   const char *s = retBack;
   while (*s && pos >= lineStartPos && styler.SafeGetCharAt(pos) == *s)
   {
      s++;
      pos--;
   }

   return !*s;
}

bool IsRegionLine(StyleContext &sc, LexAccessor &styler)
{
   // Don't look at styles, so no need to flush.
   int pos = (int)sc.currentPos;
   int currentLine = styler.GetLine(pos);
   int lineEndPos = styler.LineEnd(currentLine);
   if (pos == lineEndPos)
      return false;
   if (sc.Match("#region"))
      pos += 6;
   else if (sc.Match("#endregion"))
      pos += 9;
   else
      return false;

   while (++pos < lineEndPos)
   {
      char ch = styler.SafeGetCharAt(pos,'\n');
      if (ch == '\n')
         break;
      if (ch == ';' || ch == '=')
         return false;
   }

   return true;
}

bool IsParameter(StyleContext &sc, LexAccessor &styler)
{
   // Don't look at styles, so no need to flush.
   int pos = (int)sc.currentPos;
   int currentLine = styler.GetLine(pos);
   int lineStartPos = styler.LineStart(currentLine);
   int lineEndPos = styler.LineEnd(currentLine);

   while (++pos < lineEndPos)
   {
      char ch = styler.SafeGetCharAt(pos, '\n');
      if (ch == '#' || ch == '\n' || ch == ';' || ch == ')'
         || ch == '(' || ch == '\\' || ch == '*')
         return false;
      if (ch == '=')
         return true;
   }

   return false;
}

bool IsSpaceOrTab(int ch)
{
   return ch == ' ' || ch == '\t';
}

bool OnlySpaceOrTab(const std::string &s)
{
   for (std::string::const_iterator it = s.begin(); it != s.end(); ++it)
   {
      if (!IsSpaceOrTab(*it))
         return false;
   }

   return true;
}

std::vector<std::string> StringSplit(const std::string &text, int separator)
{
   std::vector<std::string> vs(text.empty() ? 0 : 1);
   for (std::string::const_iterator it = text.begin(); it != text.end(); ++it)
   {
      if (*it == separator)
         vs.push_back(std::string());
      else
         vs.back() += *it;
   }

   return vs;
}

struct BracketPair
{
   std::vector<std::string>::iterator itBracket;
   std::vector<std::string>::iterator itEndBracket;
};

BracketPair FindBracketPair(std::vector<std::string> &tokens)
{
   BracketPair bp;
   std::vector<std::string>::iterator itTok = std::find(tokens.begin(), tokens.end(), "(");
   bp.itBracket = tokens.end();
   bp.itEndBracket = tokens.end();
   if (itTok != tokens.end())
   {
      bp.itBracket = itTok;
      size_t nest = 0;
      while (itTok != tokens.end())
      {
         if (*itTok == "(")
            nest++;
         else if (*itTok == ")")
         {
            nest--;
            if (nest == 0)
            {
               bp.itEndBracket = itTok;
               return bp;
            }
         }
         ++itTok;
      }
   }
   bp.itBracket = tokens.end();

   return bp;
}

void highlightTaskMarker(StyleContext &sc, LexAccessor &styler, int activity,
   WordList &markerList, bool caseSensitive)
{
   if ((isoperator(sc.chPrev) || IsASpace(sc.chPrev)) && markerList.Length())
   {
      const int lengthMarker = 50;
      char marker[lengthMarker + 1];
      int currPos = (int)sc.currentPos;
      int i = 0;
      while (i < lengthMarker)
      {
         char ch = styler.SafeGetCharAt(currPos + i);
         if (IsASpace(ch) || isoperator(ch))
            break;

         if (caseSensitive)
            marker[i] = ch;
         else
            marker[i] = static_cast<char>(tolower(ch));
         i++;
      }
      marker[i] = '\0';
      if (markerList.InList(marker))
         sc.SetState(SCE_KOD_TASKMARKER | activity);
   }
}

struct EscapeSequence
{
   int digitsLeft;
   CharacterSet setHexDigits;
   CharacterSet setOctDigits;
   CharacterSet setNoneNumeric;
   CharacterSet *escapeSetValid;
   EscapeSequence()
   {
      digitsLeft = 0;
      escapeSetValid = 0;
      setHexDigits = CharacterSet(CharacterSet::setDigits, "ABCDEFabcdef");
      setOctDigits = CharacterSet(CharacterSet::setNone, "01234567");
   }
   void resetEscapeState(int nextChar)
   {
      digitsLeft = 0;
      escapeSetValid = &setNoneNumeric;
      if (nextChar == 'U')
      {
         digitsLeft = 9;
         escapeSetValid = &setHexDigits;
      }
      else if (nextChar == 'u')
      {
         digitsLeft = 5;
         escapeSetValid = &setHexDigits;
      }
      else if (nextChar == 'x')
      {
         digitsLeft = 5;
         escapeSetValid = &setHexDigits;
      }
      else if (setOctDigits.Contains(nextChar))
      {
         digitsLeft = 3;
         escapeSetValid = &setOctDigits;
      }
   }
   bool atEscapeEnd(int currChar) const
   {
      return (digitsLeft <= 0) || !escapeSetValid->Contains(currChar);
   }
};

std::string GetRestOfLine(LexAccessor &styler, int start, bool allowSpace)
{
   std::string restOfLine;
   int i = 0;
   char ch = styler.SafeGetCharAt(start, '\n');
   int endLine = styler.LineEnd(styler.GetLine(start));
   while (((start + i) < endLine) && (ch != '\r'))
   {
      char chNext = styler.SafeGetCharAt(start + i + 1, '\n');
      if (ch == '/' && (chNext == '/' || chNext == '*'))
         break;
      if (allowSpace || (ch != ' '))
         restOfLine += ch;
      i++;
      ch = chNext;
   }

   return restOfLine;
}

bool IsStreamCommentStyle(int style)
{
   return style == SCE_KOD_COMMENT ||
      style == SCE_KOD_COMMENTDOC ||
      style == SCE_KOD_COMMENTDOCKEYWORD ||
      style == SCE_KOD_COMMENTDOCKEYWORDERROR;
}

void AdvanceToCommentClose(StyleContext sc)
{
   if (sc.More())
      sc.Forward();
   else
      return;

   while (sc.More() && !sc.Match("*/"))
   {
      if (sc.Match("/*"))
      {
         AdvanceToCommentClose(sc);
      }
      if (sc.More())
         sc.Forward();
   }
}

#pragma endregion

#pragma region Options

// Options used for LexerKod
struct OptionsKod
{
   bool backQuotedStrings;
   bool escapeSequence;
   bool fold;
   bool foldSyntaxBased;
   bool foldComment;
   bool foldCommentMultiline;
   bool foldCommentExplicit;
   std::string foldExplicitStart;
   std::string foldExplicitEnd;
   bool foldExplicitAnywhere;
   bool foldPreprocessor;
   bool foldCompact;
   bool foldAtElse;

   OptionsKod()
   {
      escapeSequence = false;
      fold = true;
      foldSyntaxBased = true;
      foldComment = true;
      foldCommentMultiline = true;
      foldCommentExplicit = true;
      foldExplicitStart = "";
      foldExplicitEnd = "";
      foldExplicitAnywhere = false;
      foldPreprocessor = true;
      foldCompact = false;
      foldAtElse = false;
   }
};

const char *const kodWordLists[] =
{
   "Keywords and identifiers",
   "C function calls (except message passing)",
   "Message passing C function calls",
   "Word operators",
   "Kod constants",
   "Task marker and error marker keywords",
   0,
};

struct OptionSetKod : public OptionSet<OptionsKod> {
   OptionSetKod() {
      DefineProperty("lexer.kod.escape.sequence", &OptionsKod::escapeSequence,
         "Set to 1 to enable highlighting of escape sequences in strings");

      DefineProperty("fold", &OptionsKod::fold);

      DefineProperty("fold.kod.syntax.based", &OptionsKod::foldSyntaxBased,
         "Set this property to 0 to disable syntax based folding.");

      DefineProperty("fold.comment", &OptionsKod::foldComment,
         "This option enables folding multi-line comments and explicit fold points when using the C++ lexer. "
         "Explicit fold points allows adding extra folding by placing a //{ comment at the start and a //} "
         "at the end of a section that should fold.");

      DefineProperty("fold.kod.comment.multiline", &OptionsKod::foldCommentMultiline,
         "Set this property to 0 to disable folding multi-line comments when fold.comment=1.");

      DefineProperty("fold.kod.comment.explicit", &OptionsKod::foldCommentExplicit,
         "Set this property to 0 to disable folding explicit fold points when fold.comment=1.");

      DefineProperty("fold.kod.explicit.start", &OptionsKod::foldExplicitStart,
         "The string to use for explicit fold start points, replacing the standard //{.");

      DefineProperty("fold.kod.explicit.end", &OptionsKod::foldExplicitEnd,
         "The string to use for explicit fold end points, replacing the standard //}.");

      DefineProperty("fold.kod.explicit.anywhere", &OptionsKod::foldExplicitAnywhere,
         "Set this property to 1 to enable explicit fold points anywhere, not just in line comments.");

      DefineProperty("fold.preprocessor", &OptionsKod::foldPreprocessor,
         "Enables #region/#endregion folding.");

      DefineProperty("fold.compact", &OptionsKod::foldCompact);

      DefineProperty("fold.at.else", &OptionsKod::foldAtElse,
         "This option enables kod folding on a \"} else {\" line of an if statement.");

      DefineWordListSets(kodWordLists);
   }
};

const char styleSubable[] = { SCE_KOD_IDENTIFIER, SCE_KOD_COMMENTDOCKEYWORD, 0 };

#pragma endregion

} // End unnamed namespace

#pragma region LexerKod Setup

class LexerKod final : public ILexerWithSubStyles
{
   bool caseSensitive;
   CharacterSet setWord;
   CharacterSet setArithmethicOp;
   CharacterSet setRelOp;
   CharacterSet setWordStart;
   WordList keywords;
   WordList keywords2;
   WordList keywords3;
   WordList keywords4;
   WordList constantList;
   string constIncludeList;
   WordList markerList;

   struct IncludeFile
   {
      ULARGE_INTEGER writeTime;
      std::string constants;
      IncludeFile(const ULARGE_INTEGER &writeTime_, const std::string &constants_ = "") : writeTime(writeTime_), constants(constants_)
      {
      }
      IncludeFile &operator = (const std::string &constants_)
      {
         constants = constants_;
         //constants.clear();
         return *this;
      }
      IncludeFile &operator = (const ULARGE_INTEGER &writeTime_)
      {
         writeTime = writeTime_;
         return *this;
      }
   };

   typedef std::map<std::string, IncludeFile> IncludeFiles;
   IncludeFiles includeFiles;

   OptionsKod options;
   OptionSetKod osKod;
   EscapeSequence escapeSeq;
   SparseState<std::string> rawStringTerminators;
   enum { activeFlag = 0x40 };
   enum { ssIdentifier, ssDocKeyword };
   SubStyles subStyles;
private:
   WordList m_WordLists[6];
public:
   explicit LexerKod(bool caseSensitive_) :
      caseSensitive(caseSensitive_),
      setWord(CharacterSet::setAlphaNum, "._", 0x80, true),
      setArithmethicOp(CharacterSet::setNone, "+-/*%"),
      setRelOp(CharacterSet::setNone, "=<>"),
      subStyles(styleSubable, 0x80, 0x40, activeFlag)
   {
   }
   virtual ~LexerKod()
   {
   }
   void SCI_METHOD Release()
   {
      delete this;
   }
   int SCI_METHOD Version() const
   {
      return lvSubStyles;
   }
   const char * SCI_METHOD PropertyNames()
   {
      return osKod.PropertyNames();
   }
   int SCI_METHOD PropertyType(const char *name)
   {
      return osKod.PropertyType(name);
   }
   const char * SCI_METHOD DescribeProperty(const char *name)
   {
      return osKod.DescribeProperty(name);
   }
   int SCI_METHOD PropertySet(const char *key, const char *val);
   const char * SCI_METHOD DescribeWordListSets()
   {
      return osKod.DescribeWordListSets();
   }
   std::string ParseIncludeFile(char *file);
   void AddConstants(IDocument *pAccess, int initStyle, LexAccessor styler);
   int SCI_METHOD WordListSet(int n, const char *wl);
   void SCI_METHOD Lex(unsigned int startPos, int length, int initStyle, IDocument *pAccess);
   void SCI_METHOD Fold(unsigned int startPos, int length, int initStyle, IDocument *pAccess);

   void * SCI_METHOD PrivateCall(int, void *)
   {
      return 0;
   }

   int SCI_METHOD LineEndTypesSupported()
   {
      return SC_LINE_END_TYPE_UNICODE;
   }

   int SCI_METHOD AllocateSubStyles(int styleBase, int numberStyles)
   {
      return subStyles.Allocate(styleBase, numberStyles);
   }
   int SCI_METHOD SubStylesStart(int styleBase)
   {
      return subStyles.Start(styleBase);
   }
   int SCI_METHOD SubStylesLength(int styleBase)
   {
      return subStyles.Length(styleBase);
   }
   int SCI_METHOD StyleFromSubStyle(int subStyle)
   {
      int styleBase = subStyles.BaseStyle(MaskActive(subStyle));
      int active = subStyle & activeFlag;
      return styleBase | active;
   }
   int SCI_METHOD PrimaryStyleFromStyle(int style)
   {
      return MaskActive(style);
   }
   void SCI_METHOD FreeSubStyles()
   {
      subStyles.Free();
   }
   void SCI_METHOD SetIdentifiers(int style, const char *identifiers)
   {
      subStyles.SetIdentifiers(style, identifiers);
   }
   int SCI_METHOD DistanceToSecondaryStyles()
   {
      return activeFlag;
   }
   const char * SCI_METHOD GetSubStyleBases()
   {
      return styleSubable;
   }
   static ILexer *LexerFactoryKod()
   {
      return new LexerKod(true);
   }
   static int MaskActive(int style)
   {
      return style & ~activeFlag;
   }
};

int SCI_METHOD LexerKod::PropertySet(const char *key, const char *val)
{
   if (osKod.PropertySet(&options, key, val))
   {
      return 0;
   }

   return -1;
}

bool IsConstantLine(StyleContext &sc, LexAccessor &styler, char *word)
{
   int pos = (int)sc.currentPos;
   int currentLine = styler.GetLine(pos);
   int lineStartPos = styler.LineStart(currentLine);
   int lineEndPos = styler.LineEnd(currentLine);

   if (pos == lineEndPos)
      return false;

   if (sc.ch == '\n')
      return false;
   int wordpos = 0;
   for (int i = lineStartPos; i < lineEndPos; ++i)
   {
      char ch = styler.SafeGetCharAt(i, '\n');
      if ((ch == '//' && styler.SafeGetCharAt(i + 1, '\n') == '//') || ch == '\n')
         return false;
      if (ch == ' ' || ch == '\t' || ch == '\r')
         continue;
      if (ch == '=')
      {
         word[wordpos] = ' ';
         word[wordpos + 1] = 0;
         return true;
      }
      word[wordpos] = ch;
      ++wordpos;
   }

   return false;
}

int SCI_METHOD LexerKod::WordListSet(int n, const char *wl)
{
   WordList *wordListN = 0;
   switch (n)
   {
   case 0:
      wordListN = &keywords;
      break;
   case 1:
      wordListN = &keywords2;
      break;
   case 2:
      wordListN = &keywords3;
      break;
   case 3:
      wordListN = &keywords4;
      break;
   case 4:
      wordListN = &constantList;
      break;
   case 5:
      wordListN = &markerList;
      break;
   }
   int firstModification = -1;
   if (wordListN)
   {
      WordList wlNew;
      wlNew.Set(wl);
      if (*wordListN != wlNew)
      {
         wordListN->Set(wl);
         firstModification = 0;
      }
   }
   return firstModification;
}

#pragma endregion

#pragma region Kod Constants

/*
 * GetCurrentKodFilePath: Puts the current kod file path into the passed char pointer.
 *   Calling function should ensure enough space is available, MAX_PATH + 1.
 */
bool GetCurrentKodFilePath(char *path)
{
   wchar_t wcCurrentPath[MAX_PATH + 1]; // Current dir path, wide char.
   // Get directory of opened file.
   SendMessage(g_NppWindow, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)wcCurrentPath);
   // Zero terminate for safety, and convert to char.
   wcCurrentPath[MAX_PATH] = 0;
   wcstombs(path, wcCurrentPath, MAX_PATH + 1);
   if (!strcmp(path, "") || strlen(path) <= 1)
      return false;
   return true;
}

/*
 * ParseIncludeFile: Parse an include file and append constants to string.
 *   file is just the name of the include file, ParseIncludeFile has to find
 *   and open it.
 */
std::string LexerKod::ParseIncludeFile(char *file)
{
   FILE *fp;
   char fullPath[MAX_PATH + 1];         // Current dir path + include file.
   char currentPath[MAX_PATH + 1];      // Current dir path, char.
   char kodPath[MAX_PATH + 1];          // Used to build kod\include path.
   char line[1024];

   // Obtain the current kod file path.
   // BUG (in Notepad++): Notepad++ won't return the correct current path
   // for this file if it was already open when Notepad++ was started.
   // As soon as any action is taken, the path will be correctly returned.
   if (!GetCurrentKodFilePath(currentPath))
      return "";

   // Copy to fullpath, zero terminate for safety.
   strcpy(fullPath, currentPath);
   fullPath[MAX_PATH] = 0;

   // Append include filename.
   PathAppendA(fullPath, file);

   // Map element used for comparison, also where we'll store the strings
   // after parsing the file, as it'll either be a pointer to the existing
   // mapped entry or the returned new one.
   std::pair<std::map<std::string, IncludeFile>::iterator, bool> insertElement;

   // Structure for getting file write time.
   WIN32_FILE_ATTRIBUTE_DATA  wfad;
   BOOL fileResult = 0;

   // First attempt to open include file in the current directory.
   if (!(fp = fopen(fullPath, "r")))
   {
      // If that fails, try standard kod include directory \kod\include.
      char *strToken;
      strToken = strtok(currentPath, "\\");
      if (!strToken)
         return "";

      while (strToken)
      {
         PathAppendA(kodPath, strToken);
         if (!strcmp(strToken, "kod"))
         {
            PathAppendA(kodPath, "include");
            PathAppendA(kodPath, file);
            break;
         }
         strToken = strtok(NULL, "\\");
      }

      if (!kodPath || !strToken || (!(fp = fopen(kodPath, "r"))))
         return "";
      // Get file attributes to compare file write time to stored.
      fileResult = GetFileAttributesExA(kodPath, GetFileExInfoStandard, &wfad);
   }
   else
   {
      // Get file attributes to compare file write time to stored.
      fileResult = GetFileAttributesExA(fullPath, GetFileExInfoStandard, &wfad);
   }

   if (!fileResult)
   {
      // Couldn't get file attributes - return empty string.
      fclose(fp);
      return "";
   }

   ULARGE_INTEGER lastWrite;
   lastWrite.HighPart = wfad.ftLastWriteTime.dwHighDateTime;
   lastWrite.LowPart = wfad.ftLastWriteTime.dwLowDateTime;

   std::string wcFile;
   wcFile.assign(file);

   IncludeFile includeFile(lastWrite, "");
   insertElement = includeFiles.insert(std::pair<std::string, IncludeFile>(wcFile, includeFile));
   if (!insertElement.second)
   {
      if (insertElement.first->second.writeTime.QuadPart == lastWrite.QuadPart)
      {
         // Use stored constants.
         fclose(fp);
         return insertElement.first->second.constants;
      }
      else
      {
         // Update write time in insertElement, parse file. Strings stored later.
         insertElement.first->second.writeTime = lastWrite;
      }
   }

   // Parse include file, assume the whole thing is a list of constants.
   // If valid constant assignments are found, add them to constant string.
   int commentDepth = 0;
   int visibleChars = 0;
   int wordFound = 0;
   char constWord[1024];
   int wordpos = 0;
   while (fgets(line, 1024, fp))
   {
      visibleChars = 0;
      wordFound = false;
      wordpos = 0;
      if (line[0] == '\n')
         continue;
      for (size_t i = 0; i < strlen(line); ++i)
      {
         if (line[i] == '\\' && i + 1 < strlen(line) && line[i + 1] == '\\')
            break;
         if (line[i] == '\\' && i + 1 < strlen(line) && line[i + 1] == '*')
            ++commentDepth;
         else if (line[i] == '*' && i + 1 < strlen(line) && line[i + 1] == '\\' && commentDepth)
            --commentDepth;
         else if (!wordFound && !commentDepth && line[i] != ' ' && line[i] != '\t' && line[i] != '=')
         {
            constWord[wordpos] = line[i];
            ++wordpos;
            ++visibleChars;
         }
         else if (line[i] == '=' && visibleChars && !commentDepth && !wordFound)
         {
            constWord[wordpos] = ' ';
            constWord[wordpos + 1] = 0;
            includeFile.constants.append(constWord);
            wordFound = true;
         }
      }
   }

   fclose(fp);

   insertElement.first->second.constants = includeFile.constants;
   return includeFile.constants;
}

/*
 * AddConstants: Build the constantWords word list.
 */
void LexerKod::AddConstants(IDocument *pAccess, int initStyle, LexAccessor styler)
{
   StyleContext sc1(0, pAccess->Length(), initStyle, styler, static_cast<unsigned char>(0xff));
   std::string constantWords;
   char constantWord[256], includeFile[256];

   int lineConstantsStart = 0;
   int lineConstantsEnd = 0;
   for (; sc1.More();)
   {
      if (sc1.Match("//"))
      {
         while (sc1.More() && sc1.ch != '\n')
            sc1.Forward();
      }
      else if (sc1.Match("/*"))
      {
         AdvanceToCommentClose(sc1);
      }
      else if (!lineConstantsStart && sc1.Match("constants:"))
      {
         lineConstantsStart = styler.GetLine(sc1.currentPos);
      }
      else if ((sc1.Match("resources:")
         || sc1.Match("classvars:") || sc1.Match("properties:") || sc1.Match("messages:")))
      {
         if (lineConstantsStart && !lineConstantsEnd)
            lineConstantsEnd = styler.GetLine(sc1.currentPos);
         break;
      }
      else if (sc1.Match("include "))
      {
         sc1.ForwardBytes(8);
         int filePos = 0;
         while (sc1.More())
         {
            if (sc1.ch == ' ' || sc1.ch == '\t')
            {
               sc1.Forward();
               continue;
            }
            else if (sc1.ch == '\n')
            {
               includeFile[filePos] = 0;
               break;
            }
            includeFile[filePos] = sc1.ch;
            ++filePos;
            sc1.Forward();
         }
         constantWords.append(ParseIncludeFile(includeFile));
      }
      else if (lineConstantsStart && sc1.ch == '=' && IsConstantLine(sc1, styler, constantWord))
      {
         constantWords.append(constantWord);
      }

      sc1.Forward();
   }
   WordListSet(4, constantWords.c_str());
}

#pragma endregion

#pragma region Lex And Fold

void SCI_METHOD LexerKod::Lex(unsigned int startPos, int length, int initStyle, IDocument *pAccess)
{
   LexAccessor styler(pAccess);

   CharacterSet setOKBeforeRE(CharacterSet::setNone, "([{=,:;!%^&*|?~+-");
   CharacterSet setCouldBePostOp(CharacterSet::setNone, "+-");

   CharacterSet setDoxygen(CharacterSet::setAlpha, "$@\\&<>#{}[]");

   setWordStart = CharacterSet(CharacterSet::setAlpha, "_", 0x80, true);

   CharacterSet setInvalidRawFirst(CharacterSet::setNone, " )\\\t\v\f\n");

   int chPrevNonWhite = ' ';
   int visibleChars = 0;
   int styleBeforeDCKeyword = SCE_KOD_DEFAULT;
   int styleBeforeTaskMarker = SCE_KOD_DEFAULT;
   bool continuationLine = false;
   bool seenDocKeyBrace = false;

   int lineCurrent = styler.GetLine(startPos);
   if ((MaskActive(initStyle) == SCE_KOD_PREPROCESSOR) ||
      (MaskActive(initStyle) == SCE_KOD_COMMENTLINE) ||
      (MaskActive(initStyle) == SCE_KOD_COMMENTLINEDOC))
   {
      // Set continuationLine if last character of previous line is '\'
      if (lineCurrent > 0)
      {
         int endLinePrevious = styler.LineEnd(lineCurrent - 1);
         if (endLinePrevious > 0)
            continuationLine = styler.SafeGetCharAt(endLinePrevious - 1) == '\\';
      }
   }

   int curNcLevel = lineCurrent > 0 ? styler.GetLineState(lineCurrent - 1) : 0;

   // look back to set chPrevNonWhite properly for better regex colouring
   if (startPos > 0)
   {
      int back = startPos;
      while (--back && IsSpaceEquiv(MaskActive(styler.StyleAt(back))))
         ;
      if (MaskActive(styler.StyleAt(back)) == SCE_KOD_OPERATOR)
         chPrevNonWhite = styler.SafeGetCharAt(back);
   }

   // Check constants
   AddConstants(pAccess, initStyle, styler);

   StyleContext sc(startPos, length, initStyle, styler, static_cast<unsigned char>(0xff));

   std::string rawStringTerminator = rawStringTerminators.ValueAt(lineCurrent - 1);
   SparseState<std::string> rawSTNew(lineCurrent);

   int activitySet = 0;

   const WordClassifier &classifierIdentifiers = subStyles.Classifier(SCE_KOD_IDENTIFIER);
   const WordClassifier &classifierDocKeyWords = subStyles.Classifier(SCE_KOD_COMMENTDOCKEYWORD);

   int lineEndNext = styler.LineEnd(lineCurrent);

   for (; sc.More();)
   {
      if (sc.atLineStart)
      {
         if (sc.state == SCE_KOD_STRING)
         {
            // Prevent SCE_KOD_STRINGEOL from leaking back to previous line which
            // ends with a line continuation by locking in the state up to this position.
            sc.SetState(sc.state);
         }
         // Using MaskActive() is not needed in the following statement.
         // Inside inactive preprocessor declaration, state will be reset anyway at the end of this block.
         if ((MaskActive(sc.state) == SCE_KOD_PREPROCESSOR) && (!continuationLine))
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         // Reset states to beginning of colourise so no surprises
         // if different sets of lines lexed.
         visibleChars = 0;

         lineCurrent = styler.GetLine(sc.currentPos);
         styler.SetLineState(lineCurrent, curNcLevel);
      }

      if (sc.atLineEnd)
      {
         lineCurrent++;
         lineEndNext = styler.LineEnd(lineCurrent);

         if (rawStringTerminator != "")
            rawSTNew.Set(lineCurrent - 1, rawStringTerminator);
      }

      // Handle line continuation generically.
      if (sc.ch == '\\')
      {
         if (static_cast<int>((sc.currentPos + 1)) >= lineEndNext)
         {
            lineCurrent++;
            lineEndNext = styler.LineEnd(lineCurrent);
            sc.Forward();
            if (sc.ch == '\r' && sc.chNext == '\n')
            {
               // Even in UTF-8, \r and \n are separate
               sc.Forward();
            }
            continuationLine = true;
            sc.Forward();

            continue;
         }
      }

      const bool atLineEndBeforeSwitch = sc.atLineEnd;

      // Determine if the current state should terminate.
      switch (MaskActive(sc.state))
      {
      case SCE_KOD_OPERATOR:
         sc.SetState(SCE_KOD_DEFAULT | activitySet);
         break;
      case SCE_KOD_NUMBER:
         // We accept almost anything because of hex. and number suffixes
         if (sc.ch == '_')
         {
            sc.ChangeState(SCE_KOD_USERLITERAL | activitySet);
         }
         else if (!(setWord.Contains(sc.ch)
            || (sc.ch == '\'')
            || ((sc.ch == '+' || sc.ch == '-') && (sc.chPrev == 'e' || sc.chPrev == 'E' ||
            sc.chPrev == 'p' || sc.chPrev == 'P'))))
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         break;
      case SCE_KOD_USERLITERAL:
         if (!(setWord.Contains(sc.ch)))
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         break;
      case SCE_KOD_IDENTIFIER:
         if (sc.atLineStart || sc.atLineEnd || !setWord.Contains(sc.ch) || (sc.ch == '.'))
         {
            char s[1000];
            if (caseSensitive)
               sc.GetCurrent(s, sizeof(s));
            else
               sc.GetCurrentLowered(s, sizeof(s));

            if (keywords.InList(s))
            {
               sc.ChangeState(SCE_KOD_KEYWORD | activitySet);
            }
            else if (keywords2.InList(s))
            {
               sc.ChangeState(SCE_KOD_CCALL | activitySet);
            }
            else if (keywords3.InList(s))
            {
               sc.ChangeState(SCE_KOD_SENDMSG | activitySet);
            }
            else if (keywords4.InList(s))
            {
               sc.ChangeState(SCE_KOD_WORDOPS | activitySet);
            }
            else if (constantList.InList(s))
            {
               sc.ChangeState(SCE_KOD_CONSTANT | activitySet);
            }
            else
            {
               int subStyle = classifierIdentifiers.ValueFor(s);
               if (subStyle >= 0)
               {
                  sc.ChangeState(subStyle | activitySet);
               }
            }
            const bool literalString = sc.ch == '\"';
            if (literalString)
            {
               size_t lenS = strlen(s);
               bool valid =
                  (lenS == 0) ||
                  ((lenS == 1) && ((s[0] == 'L') || (s[0] == 'u') || (s[0] == 'U'))) ||
                  ((lenS == 2) && literalString && (s[0] == 'u') && (s[1] == '8'));
               if (valid && literalString)
               {
                  sc.ChangeState(SCE_KOD_STRING | activitySet);
               }
               else
               {
                  sc.SetState(SCE_KOD_DEFAULT | activitySet);
               }
            }
            else
            {
               sc.SetState(SCE_KOD_DEFAULT | activitySet);
            }
         }
         break;
      case SCE_KOD_COMMENT:
         if (!sc.atLineEnd && sc.Match('*', '/'))
         {
            if (curNcLevel > 0)
               --curNcLevel;
            lineCurrent = styler.GetLine(sc.currentPos);
            styler.SetLineState(lineCurrent, curNcLevel);
            sc.Forward();
            if (curNcLevel < 1)
               sc.ForwardSetState(SCE_KOD_DEFAULT | activitySet);
            else
               sc.ForwardSetState(SCE_KOD_COMMENT | activitySet);
         }
         else
         {
            if (sc.Match('/', '*'))
            {
               ++curNcLevel;
               lineCurrent = styler.GetLine(sc.currentPos);
               styler.SetLineState(lineCurrent, curNcLevel);
               sc.Forward();	// Eat the * so it isn't used for the end of the comment
               sc.ForwardSetState(SCE_KOD_COMMENT | activitySet);
            }

            styleBeforeTaskMarker = SCE_KOD_COMMENT;
            highlightTaskMarker(sc, styler, activitySet, markerList, caseSensitive);
         }
         break;
      case SCE_KOD_COMMENTDOC:
         if (!sc.atLineEnd && sc.Match('*', '/'))
         {
            sc.Forward();
            sc.ForwardSetState(SCE_KOD_DEFAULT | activitySet);
         }
         else if (sc.ch == '@' || sc.ch == '\\') { // JavaDoc and Doxygen support
            // Verify that we have the conditions to mark a comment-doc-keyword
            if ((IsASpace(sc.chPrev) || sc.chPrev == '*') && (!IsASpace(sc.chNext)))
            {
               styleBeforeDCKeyword = SCE_KOD_COMMENTDOC;
               sc.SetState(SCE_KOD_COMMENTDOCKEYWORD | activitySet);
            }
         }
         break;
      case SCE_KOD_COMMENTLINE:
         if (sc.atLineStart && !continuationLine)
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         else {
            styleBeforeTaskMarker = SCE_KOD_COMMENTLINE;
            highlightTaskMarker(sc, styler, activitySet, markerList, caseSensitive);
         }
         break;
      case SCE_KOD_COMMENTLINEDOC:
         if (sc.atLineStart && !continuationLine)
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         else if (sc.ch == '@' || sc.ch == '\\') { // JavaDoc and Doxygen support
            // Verify that we have the conditions to mark a comment-doc-keyword
            if ((IsASpace(sc.chPrev) || sc.chPrev == '/' || sc.chPrev == '!') && (!IsASpace(sc.chNext)))
            {
               styleBeforeDCKeyword = SCE_KOD_COMMENTLINEDOC;
               sc.SetState(SCE_KOD_COMMENTDOCKEYWORD | activitySet);
            }
         }
         break;
      case SCE_KOD_COMMENTDOCKEYWORD:
         if ((styleBeforeDCKeyword == SCE_KOD_COMMENTDOC) && sc.Match('*', '/'))
         {
            sc.ChangeState(SCE_KOD_COMMENTDOCKEYWORDERROR);
            sc.Forward();
            sc.ForwardSetState(SCE_KOD_DEFAULT | activitySet);
            seenDocKeyBrace = false;
         }
         else if (sc.ch == '[' || sc.ch == '{')
         {
            seenDocKeyBrace = true;
         }
         else if (!setDoxygen.Contains(sc.ch)
            && !(seenDocKeyBrace && (sc.ch == ',' || sc.ch == '.')))
         {
            char s[100];
            if (caseSensitive)
            {
               sc.GetCurrent(s, sizeof(s));
            }
            else
            {
               sc.GetCurrentLowered(s, sizeof(s));
            }
            if (!(IsASpace(sc.ch) || (sc.ch == 0)))
            {
               sc.ChangeState(SCE_KOD_COMMENTDOCKEYWORDERROR | activitySet);
            }
            else if (!keywords3.InList(s + 1))
            {
               int subStyleCDKW = classifierDocKeyWords.ValueFor(s + 1);
               if (subStyleCDKW >= 0)
               {
                  sc.ChangeState(subStyleCDKW | activitySet);
               }
               else
               {
                  sc.ChangeState(SCE_KOD_COMMENTDOCKEYWORDERROR | activitySet);
               }
            }
            sc.SetState(styleBeforeDCKeyword | activitySet);
            seenDocKeyBrace = false;
         }
         break;
      case SCE_KOD_STRING:
         if (sc.atLineEnd)
         {
            sc.ChangeState(SCE_KOD_STRINGEOL | activitySet);
         }
         else if (sc.ch == '\\')
         {
            if (options.escapeSequence)
            {
               sc.SetState(SCE_KOD_ESCAPESEQUENCE | activitySet);
               escapeSeq.resetEscapeState(sc.chNext);
            }
            sc.Forward(); // Skip all characters after the backslash
         }
         else if (sc.ch == '\"')
         {
            if (sc.chNext == '_')
            {
               sc.ChangeState(SCE_KOD_USERLITERAL | activitySet);
            }
            else
            {
               sc.ForwardSetState(SCE_KOD_DEFAULT | activitySet);
            }
         }
         break;
      case SCE_KOD_ESCAPESEQUENCE:
         escapeSeq.digitsLeft--;
         if (!escapeSeq.atEscapeEnd(sc.ch))
         {
            break;
         }
         if (sc.ch == '"')
         {
            sc.SetState(SCE_KOD_STRING | activitySet);
            sc.ForwardSetState(SCE_KOD_DEFAULT | activitySet);
         }
         else if (sc.ch == '\\')
         {
            escapeSeq.resetEscapeState(sc.chNext);
            sc.Forward();
         }
         else
         {
            sc.SetState(SCE_KOD_STRING | activitySet);
            if (sc.atLineEnd)
            {
               sc.ChangeState(SCE_KOD_STRINGEOL | activitySet);
            }
         }
         break;
      case SCE_KOD_MESSAGE:
      case SCE_KOD_CLASS:
         if (sc.ch == ',' || sc.ch == ')' || sc.ch == '\n' || sc.ch == ' ' || sc.ch == ']')
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         break;
      case SCE_KOD_PARAMETER:
         if (sc.ch == '=')
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         break;
      case SCE_KOD_STRINGEOL:
         if (sc.atLineStart)
         {
            sc.SetState(SCE_KOD_DEFAULT | activitySet);
         }
         break;
      case SCE_KOD_TASKMARKER:
         if (isoperator(sc.ch) || IsASpace(sc.ch))
         {
            sc.SetState(styleBeforeTaskMarker | activitySet);
            styleBeforeTaskMarker = SCE_KOD_DEFAULT;
         }
      }

      if (sc.atLineEnd && !atLineEndBeforeSwitch)
      {
         // State exit processing consumed characters up to end of line.
         lineCurrent++;
         lineEndNext = styler.LineEnd(lineCurrent);
      }

      // Determine if a new state should be entered.
      if (MaskActive(sc.state) == SCE_KOD_DEFAULT)
      {
         if (sc.ch == '@' && visibleChars && !sc.atLineEnd && IsUpperCase(sc.chNext))
         {
            sc.SetState(SCE_KOD_MESSAGE | activitySet);
         }
         else if (sc.ch == '&' && !sc.atLineEnd && IsUpperCase(sc.chNext))
         {
            sc.SetState(SCE_KOD_CLASS | activitySet);
         }
         else if (IsADigit(sc.ch) || (sc.ch == '.' && !!sc.atLineEnd && IsADigit(sc.chNext)))
         {
            sc.SetState(SCE_KOD_NUMBER | activitySet);
         }
         else if (!sc.atLineEnd && setWordStart.Contains(sc.ch))
         {
            sc.SetState(SCE_KOD_IDENTIFIER | activitySet);
         }
         else if (!sc.atLineEnd && sc.Match('/', '*'))
         {
            if (sc.Match("/**") || sc.Match("/*!"))
            {	// Support of Qt/Doxygen doc. style
               sc.SetState(SCE_KOD_COMMENTDOC | activitySet);
            }
            else
            {
               ++curNcLevel;
               lineCurrent = styler.GetLine(sc.currentPos);
               styler.SetLineState(lineCurrent, curNcLevel);
               sc.SetState(SCE_KOD_COMMENT | activitySet);
            }
            sc.Forward();	// Eat the * so it isn't used for the end of the comment
         }
         else if (!sc.atLineEnd && sc.Match('/', '/'))
         {
            if ((sc.Match("///") && !sc.Match("////")) || sc.Match("//!"))
               // Support of Qt/Doxygen doc. style
               sc.SetState(SCE_KOD_COMMENTLINEDOC | activitySet);
            else
               sc.SetState(SCE_KOD_COMMENTLINE | activitySet);
         }
         else if (sc.ch == '\"')
         {
            sc.SetState(SCE_KOD_STRING | activitySet);
         }
         else if (isoperator(sc.ch) || sc.ch == '$')
         {
            sc.SetState(SCE_KOD_OPERATOR | activitySet);
         }
         else if (visibleChars == 0 && IsRegionLine(sc, styler))
         {
            // Preprocessor commands are alone on their line
            sc.SetState(SCE_KOD_PREPROCESSOR | activitySet);
         }
         else if (sc.ch == '#' && IsParameter(sc, styler))
         {
            // Parameter colors from = to the end of the parameter word.
            sc.SetState(SCE_KOD_PARAMETER | activitySet);
         }
      }

      if (!IsASpace(sc.ch) && !IsSpaceEquiv(MaskActive(sc.state)))
      {
         chPrevNonWhite = sc.ch;
         visibleChars++;
      }
      continuationLine = false;
      sc.Forward();
   }
   const bool rawStringsChanged = rawStringTerminators.Merge(rawSTNew, lineCurrent);
   if (rawStringsChanged)
      styler.ChangeLexerState(startPos, startPos + length);
   sc.Complete();
}

void SCI_METHOD LexerKod::Fold(unsigned int startPos, int length, int initStyle, IDocument *pAccess)
{
   if (!options.fold)
      return;

   LexAccessor styler(pAccess);

   unsigned int endPos = startPos + length;
   int visibleChars = 0;
   bool inLineComment = false;
   int lineCurrent = styler.GetLine(startPos);
   int levelCurrent = SC_FOLDLEVELBASE;
   if (lineCurrent > 0)
      levelCurrent = styler.LevelAt(lineCurrent - 1) >> 16;
   unsigned int lineStartNext = styler.LineStart(lineCurrent + 1);
   int levelMinCurrent = levelCurrent;
   int levelNext = levelCurrent;
   char chNext = styler[startPos];
   int styleNext = MaskActive(styler.StyleAt(startPos));
   int style = MaskActive(initStyle);
   const bool userDefinedFoldMarkers = !options.foldExplicitStart.empty() && !options.foldExplicitEnd.empty();

   for (unsigned int i = startPos; i < endPos; i++)
   {
      char ch = chNext;
      chNext = styler.SafeGetCharAt(i + 1);
      int stylePrev = style;
      style = styleNext;
      styleNext = MaskActive(styler.StyleAt(i + 1));
      bool atEOL = i == (lineStartNext - 1);
      if ((style == SCE_KOD_COMMENTLINE) || (style == SCE_KOD_COMMENTLINEDOC))
         inLineComment = true;
      if (options.foldComment && options.foldCommentMultiline && IsStreamCommentStyle(style) && !inLineComment)
      {
         if (!IsStreamCommentStyle(stylePrev))
         {
            levelNext++;
         }
         else if (!IsStreamCommentStyle(styleNext) && !atEOL)
         {
            // Comments don't end at end of line and the next character may be unstyled.
            levelNext--;
         }
      }
      if (options.foldComment && options.foldCommentExplicit && ((style == SCE_KOD_COMMENTLINE) || options.foldExplicitAnywhere))
      {
         if (userDefinedFoldMarkers)
         {
            if (styler.Match(i, options.foldExplicitStart.c_str()))
            {
               levelNext++;
            }
            else if (styler.Match(i, options.foldExplicitEnd.c_str()))
            {
               levelNext--;
            }
         }
         else
         {
            if ((ch == '/') && (chNext == '/'))
            {
               char chNext2 = styler.SafeGetCharAt(i + 2);
               if (chNext2 == '{')
               {
                  levelNext++;
               }
               else if (chNext2 == '}')
               {
                  levelNext--;
               }
            }
         }
      }
      if (options.foldPreprocessor && (style == SCE_KOD_PREPROCESSOR))
      {
         if (ch == '#')
         {
            unsigned int j = i + 1;
            while ((j < endPos) && IsASpaceOrTab(styler.SafeGetCharAt(j)))
            {
               j++;
            }
            if (styler.Match(j, "region"))
            {
               levelNext++;
            }
            else if (styler.Match(j, "endregion"))
            {
               levelNext--;
            }
         }
      }
      if (options.foldSyntaxBased && (style == SCE_KOD_OPERATOR))
      {
         if (ch == '{' || ch == '[')
         {
            // Measure the minimum before a '{' to allow
            // folding on "} else {"
            if (levelMinCurrent > levelNext)
            {
               levelMinCurrent = levelNext;
            }
            levelNext++;
         }
         else if (ch == '}' || ch == ']')
         {
            levelNext--;
         }
      }
      if (!IsASpace(ch))
         visibleChars++;
      if (atEOL || (i == endPos - 1))
      {
         int levelUse = levelCurrent;
         if (options.foldSyntaxBased && options.foldAtElse)
         {
            levelUse = levelMinCurrent;
         }
         int lev = levelUse | levelNext << 16;
         if (visibleChars == 0 && options.foldCompact)
            lev |= SC_FOLDLEVELWHITEFLAG;
         if (levelUse < levelNext)
            lev |= SC_FOLDLEVELHEADERFLAG;
         if (lev != styler.LevelAt(lineCurrent))
         {
            styler.SetLevel(lineCurrent, lev);
         }
         lineCurrent++;
         lineStartNext = styler.LineStart(lineCurrent + 1);
         levelCurrent = levelNext;
         levelMinCurrent = levelCurrent;
         if (atEOL && (i == static_cast<unsigned int>(styler.Length() - 1)))
         {
            // There is an empty line at end of file so give it same level and empty
            styler.SetLevel(lineCurrent, (levelCurrent | levelCurrent << 16) | SC_FOLDLEVELWHITEFLAG);
         }
         visibleChars = 0;
         inLineComment = false;
      }
   }
}

#pragma endregion

#pragma region Scintilla Exports
int SCI_METHOD GetLexerCount()
{
	return 1;
}

void SCI_METHOD GetLexerName(unsigned int index, char* name, int buflength)
{
	strncpy(name, "Kod", buflength);
	name[buflength - 1] = '\0';
}

void SCI_METHOD GetLexerStatusText(unsigned int index, WCHAR* desc, int buflength)
{
	wcsncpy(desc, L"Kod language file", buflength);
	desc[buflength - 1] = L'\0';
}

LexerFactoryFunction SCI_METHOD GetLexerFactory(unsigned int index)
{
   return LexerKod::LexerFactoryKod;
}
#pragma endregion

