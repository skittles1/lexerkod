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

HWND g_NppWindow = nullptr;

namespace LexerKod {

void About()
{
   MessageBox(
      g_NppWindow,
      L"Kod lexer adapted by Delerium using the following projects:\n"
      L"https://github.com/poiru/rainlexer\n"
      L"https://github.com/notepad-plus-plus/notepad-plus-plus\n\n"
      L"LexerKod includes proper multi-line comment, bracket and #region/#endregion "
      L"folding, support for //, /* */ and % comment types, colors for C calls, "
      L"class and message names (anything prefixed with @ or &) keywords and constants. "
      L"Constants are loaded from include files provided they are in the same "
      L"directory as the kod file, or in the kod\\include folder. Please report "
      L"any bugs you find as an issue on the github page:\n"
      L"https://github.com/skittles1/lexerkod\n",
      LEXERKOD_TITLE,
      MB_OK);
}

//
// Notepad++ exports
//

BOOL isUnicode()
{
   return TRUE;
}

const WCHAR* getName()
{
   return L"&Kod";
}

FuncItem* getFuncsArray(int* count)
{
   static FuncItem funcItems[] =
   {
      { L"&About...", About, 0, false, nullptr }
   };

   *count = _countof(funcItems);

   return funcItems;
}

void setInfo(NppData data)
{
   g_NppWindow = data._nppHandle;
}

void beNotified(SCNotification* scn)
{
}

LRESULT messageProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   return TRUE;
}

}   // namespace LexerKod
