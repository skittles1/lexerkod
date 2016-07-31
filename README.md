LexerKod
--------------

LexerKod is a Notepad++ plugin that adds colorising and folding for
Blakod (kod), the scripting language used in the MMORPG [Meridian 59](https://www.meridiannext.com).

LexerKod includes support for:
* Single line C++ // comments
* Multi-line C-style /* */ comments, nesting allowed
* Coloring and autocomplete of C calls with parameter hints
(autocomplete must be enabled in Notepad++'s settings)
* Coloring of class (&) and message (@) names
* Coloring of constants declared in the current file, and those
declared in any included constant files
* Coloring of keywords, numbers, operators and $ symbol
* #region and #endregion folding
* Folding at brackets and multi-line comments

The plugin works with two Notepad++ themes, the default (white background)
theme and the Deep Black theme.

Download Plugin
--------------
[Direct download link for the plugin.](https://github.com/skittles1/lexerkod/blob/master/WixSharpInstaller/LexerKod.msi?raw=true)

Examples
--------------
<p align="center">
   <img src="http://i.imgur.com/IOceOcS.png" width="512">
</p>
<p align="center">
   <img src="http://i.imgur.com/RiT0Fmb.png" width="512">
</p>

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
