// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using Microsoft.Win32;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class VisualStudioSourceCodeAccess : ModuleRules
	{
        public VisualStudioSourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform",
					"Projects",
					"Json",
					"VisualStudioSetup"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			bool bHasVisualStudioDTE;

			// In order to support building the plugin on build machines (which may not have the IDE installed), allow using an OLB rather than registered component.
			string DteOlbPath = Path.Combine(ModuleDirectory, "Private", "NotForLicensees", "dte80a.olb");
			if(File.Exists(DteOlbPath) && Target.WindowsPlatform.Compiler != WindowsCompiler.Clang)
			{
				PrivateDefinitions.Add("VSACCESSOR_HAS_DTE_OLB=1");
				bHasVisualStudioDTE = true;
			}
			else
			{
				PrivateDefinitions.Add("VSACCESSOR_HAS_DTE_OLB=0");
				try
				{
					// Interrogate the Win32 registry
					string DTEKey = null;
					switch (Target.WindowsPlatform.Compiler)
					{
						case WindowsCompiler.VisualStudio2017:
							DTEKey = "VisualStudio.DTE.15.0";
							break;
						case WindowsCompiler.VisualStudio2015_DEPRECATED:
							DTEKey = "VisualStudio.DTE.14.0";
							break;
					}
					bHasVisualStudioDTE = RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry32).OpenSubKey(DTEKey) != null;
				}
				catch
				{
					bHasVisualStudioDTE = false;
				}
			}

			if (bHasVisualStudioDTE && Target.WindowsPlatform.StaticAnalyzer != WindowsStaticAnalyzer.PVSStudio)
			{
				PublicDefinitions.Add("VSACCESSOR_HAS_DTE=1");
			}
			else
			{
				PublicDefinitions.Add("VSACCESSOR_HAS_DTE=0");
			}

			bBuildLocallyWithSNDBS = true;
		}
	}
}
