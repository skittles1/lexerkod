using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using System.Xml.XPath;
using System.Text;
using Microsoft.Win32;
using Microsoft.Deployment.WindowsInstaller;
using WixSharp;
using io = System.IO;

class Script
{
    static public void Main(string[] args)
    {
        string nppPath;

        using (RegistryKey registryKey = Registry.LocalMachine.OpenSubKey("Software\\Wow6432Node\\Notepad++"))
        {
            nppPath = (string)registryKey.GetValue("");
        }
        nppPath = nppPath + "\\";

        string appConfigPath = "%AppDataFolder%\\Notepad++\\plugins\\config";

        var project = new Project("LexerKod")
            {
                UI = WUI.WixUI_ProgressOnly,
                Dirs = new[]
                {
                    new Dir(nppPath + @"plugins",
                      new File(@"..\Release\LexerKod.dll"),
                        new Dir(@"APIs",
                          new File(@"..\LexerKod\AutoComplete\kod.xml"))),
                    new Dir(appConfigPath,
                        new File(@"..\LexerKod\LangData\LexerKod.xml"))
                },
                Actions = new WixSharp.Action[]
                {
                    new ManagedAction(@"OnSetupStartup", Return.check, When.Before, Step.LaunchConditions, 
                                    Condition.NOT_Installed, Sequence.InstallUISequence),
                    new ElevatedManagedAction(@"AddThemes", Return.ignore, When.After, Step.InstallFiles, Condition.NOT_BeingRemoved)
                    {
                        Execute = WixSharp.Execute.commit
                    }
                },

                MajorUpgradeStrategy = MajorUpgradeStrategy.Default,
                Version = new Version("1.0.4.0"),
                GUID = new Guid("11f8c9e0-b62c-41a7-8aeb-1692f86755e3")
            };
        Compiler.PreserveTempFiles = false;

        var file = Compiler.BuildMsi(project);
    }
}

public class CustomActions
{
    [CustomAction]
    public static ActionResult OnSetupStartup(Session session)
    {
        Process[] processlist = Process.GetProcesses();
        if (processlist.Length != 0)
        {
            foreach (Process process in processlist)
            {
                try
                {
                    if (process.Modules[0].FileName.ToLower().Contains("notepad++.exe"))
                    {
                        process.Kill();
                        session.Log("Closed notepad++.exe");
                    }
                }
                catch (Exception)
                {
                }
            }
        }

        return ActionResult.Success;
    }

    [CustomAction]
    public static ActionResult AddThemes(Session session)
    {
        var fileName = io.Path.Combine(Environment.GetFolderPath(
                            Environment.SpecialFolder.ApplicationData), "Notepad++\\themes\\Deep Black.xml");
        session.Log("------------------ " + fileName);

        var xdoc = XDocument.Load(fileName);
        XElement kodElement = xdoc.XPathSelectElement("NotepadPlus/LexerStyles/*[@name='Kod']");
        if (kodElement != null)
        {
            session.Log("------------------ " + kodElement.Attribute("name").Value);
            kodElement.Remove();
            xdoc.Save(fileName);
        }
        XElement kodStyle = new XElement("LexerType",
                new XAttribute("name", "Kod"),
                new XAttribute("desc", "Kod"),
                new XAttribute("ext", "kod khd lkod"),
                new XAttribute("excluded", "no"));
        kodStyle.Add(AddWordStyle("DEFAULT", "0", "FFFFFF", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("IDENTIFIER", "1", "FFFFFF", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("NUMBER", "2", "73DCFF", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("OPERATOR", "3", "FFCC00", "000000", "", "1", "", null));
        kodStyle.Add(AddWordStyle("STRING", "4", "FFFF20", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("PREPROCESSOR", "5", "C0C0C0", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("KEYWORDS", "6", "FF7700", "000000", "", "1", "", "instre1"));
        kodStyle.Add(AddWordStyle("C CALLS", "7", "00FFFF", "000000", "", "0", "", "instre2"));
        kodStyle.Add(AddWordStyle("SENDMSG", "8", "00FFFF", "000000", "", "0", "", "type1"));
        kodStyle.Add(AddWordStyle("WORD OPERATORS", "9", "FFCC00", "000000", "", "0", "", "type2"));
        kodStyle.Add(AddWordStyle("MESSAGE", "10", "CFFFBF", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("CLASS", "11", "BFCFFF", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("CONSTANT", "12", "D057BE", "000000", "", "0", "", "type3"));
        kodStyle.Add(AddWordStyle("PARAMETER", "13", "00FF00", "000000", "", "0", "", null));
        kodStyle.Add(AddWordStyle("COMMENT", "15", "009900", "000000", "", "2", "", null));
        kodStyle.Add(AddWordStyle("COMMENT LINE", "16", "009900", "000000", "", "2", "", null));
        kodStyle.Add(AddWordStyle("COMMENT DOC", "17", "009900", "000000", "", "2", "", null));
        kodStyle.Add(AddWordStyle("COMMENT LINE DOC", "18", "009900", "000000", "", "2", "", null));
        kodStyle.Add(AddWordStyle("COMMENT DOC KEYWORD", "19", "009900", "000000", "", "2", "", null));
        kodStyle.Add(AddWordStyle("COMMENT DOC KEYWORD ERROR", "20", "00BB00", "000000", "", "2", "", null));

        XElement styleElement = xdoc.XPathSelectElement("NotepadPlus/LexerStyles");

        styleElement.Add(kodStyle);
        xdoc.Save(fileName);

        return ActionResult.Success;
    }

    static XElement AddWordStyle(string name, string styleID, string fgColor,
                                 string bgColor, string fontName, string fontStyle,
                                 string fontSize, string keywordClass)
    {
        XElement wordStyle = new XElement("WordsStyle",
                    new XAttribute("name", name),
                    new XAttribute("styleID", styleID),
                    new XAttribute("fgColor", fgColor),
                    new XAttribute("bgColor", bgColor),
                    new XAttribute("fontName", fontName),
                    new XAttribute("fontStyle", fontStyle),
                    new XAttribute("fontSize", fontSize));
        if (keywordClass != null)
        {
            XAttribute kClass = new XAttribute("keywordClass", keywordClass);
            wordStyle.Add(kClass);
        }
        return wordStyle;
    }
}