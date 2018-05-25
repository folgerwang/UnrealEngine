// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
    
    /// <summary>
    /// Visual Studio for Mac project file generator implementation
    /// </summary>
    class VCMacProjectFileGenerator : VCProjectFileGenerator
    {
       
        /// <summary>
        /// Default constructor
        /// </summary>
        /// <param name="InOnlyGameProject">The single project to generate project files for, or null</param>
        /// <param name="InOverrideCompiler">Override the compiler version to use</param>
        public VCMacProjectFileGenerator(FileReference InOnlyGameProject, WindowsCompiler InOverrideCompiler)
			: base(InOnlyGameProject, VCProjectFileFormat.Default, InOverrideCompiler)
        {
            // no suo file, requires ole32
            bWriteSolutionOptionFile = false;
        }

        /// True if we should include IntelliSense data in the generated project files when possible
        override public bool ShouldGenerateIntelliSenseData()
        {
            return false;
        }

        /// <summary>
        /// Writes the project files to disk
        /// </summary>
        /// <returns>True if successful</returns>
        protected override bool WriteProjectFiles()
        {
            // This can be reset by higher level code when it detects that we don't have
            // VS2015 installed (TODO - add custom format for Mac?)
            ProjectFileFormat = VCProjectFileFormat.VisualStudio2015;

            // we can't generate native projects so clear them here, we will just
            // write out OtherProjectFiles and AutomationProjectFiles
            GeneratedProjectFiles.Clear();

            if (!base.WriteProjectFiles())
            {
                return false;
            }


            // Write AutomationReferences file
            if (AutomationProjectFiles.Any())
            {
                XNamespace NS = XNamespace.Get("http://schemas.microsoft.com/developer/msbuild/2003");

                DirectoryReference AutomationToolDir = DirectoryReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Programs", "AutomationTool");
                new XDocument(
                    new XElement(NS + "Project",
                        new XAttribute("ToolsVersion", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat)),
                        new XAttribute("DefaultTargets", "Build"),
                        new XElement(NS + "ItemGroup",
                            from AutomationProject in AutomationProjectFiles
                            select new XElement(NS + "ProjectReference",
                                new XAttribute("Include", AutomationProject.ProjectFilePath.MakeRelativeTo(AutomationToolDir)),
                                new XElement(NS + "Project", (AutomationProject as VCSharpProjectFile).ProjectGUID.ToString("B")),
                                new XElement(NS + "Name", AutomationProject.ProjectFilePath.GetFileNameWithoutExtension()),
                                new XElement(NS + "Private", "false")
                            )
                        )
                    )
                ).Save(FileReference.Combine(AutomationToolDir, "AutomationTool.csproj.References").FullName);
            }

            return true;
        }

    }

}
