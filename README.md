LexerKod
--------------

LexerKod is a Notepad++ plugin that adds colorising and folding for
Blakod (kod), the scripting language used in the MMORPG Meridian 59.

LexerKod includes support for:
* Single line C++ // and kod % style comments
* Multi-line C-style /* */ comments, nesting allowed
* Coloring and autocomplete of C calls with parameter hints
(autocomplete must be enabled in Notepad++'s settings)
* Coloring of class (&) and message (@) names
* Coloring of constants declared in the current file, and those
declared in any included constant files
* Coloring of keywords, numbers, operators and $ symbol
* #region and #endregion folding
* Folding at brackets and multi-line comments

As of v1.03 the only themes available are the default (white background)
and Notepad++ Deep Black themes.

Download Plugin
--------------
You can obtain the plugin at https://drive.google.com/open?id=0B1S4dg5X_oh_VDZZbll3UzRKclE

![Deep Black](http://i.imgur.com/IOceOcS.png)
![Default](http://i.imgur.com/RiT0Fmb.png)

Build
--------------
Building LexerKod requires first building the Notepad++ and Scintilla
projects from https://github.com/notepad-plus-plus/notepad-plus-plus.

Building the WiX# installer requires installing the WixSharp NuGet package
in Visual Studio using the command Install-Package WixSharp.

Credits
--------------
Code based on RainLexer https://github.com/poiru/rainlexer and LexerCPP.cxx
from the Notepad++ project https://github.com/notepad-plus-plus/notepad-plus-plus.