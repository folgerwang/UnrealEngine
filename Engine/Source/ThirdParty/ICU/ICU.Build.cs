// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class ICU : ModuleRules
{
	enum EICULinkType
	{
		None,
		Static,
		Dynamic
	}

	public ICU(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bNeedsDlls = false;

		string ICUVersion = "icu4c-53_1";
		string ICURootPath = Target.UEThirdPartySourceDirectory + "ICU/" + ICUVersion + "/";

		// Includes
		PublicSystemIncludePaths.Add(ICURootPath + "include" + "/");

		string PlatformFolderName = Target.Platform.ToString();

		string TargetSpecificPath = ICURootPath + PlatformFolderName + "/";
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			TargetSpecificPath = ICURootPath + "Linux/";
		}

		// make all Androids use the Android directory
		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			TargetSpecificPath = ICURootPath + "Android/";
		}

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			string VSVersionFolderName = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			TargetSpecificPath += VSVersionFolderName + "/";

			string[] LibraryNameStems =
			{
				"dt",   // Data
				"uc",   // Unicode Common
				"in",   // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			// Library Paths
			PublicLibraryPaths.Add(TargetSpecificPath + "lib" + "/");

			EICULinkType ICULinkType = (Target.LinkType == TargetLinkType.Monolithic)? EICULinkType.Static : EICULinkType.Dynamic;
			switch(ICULinkType)
			{
			case EICULinkType.Static:
				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "sicu" + Stem + LibraryNamePostfix + "." + "lib";
					PublicAdditionalLibraries.Add(LibraryName);
				}
				break;
			case EICULinkType.Dynamic:
				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "icu" + Stem + LibraryNamePostfix + "." + "lib";
					PublicAdditionalLibraries.Add(LibraryName);
				}

				foreach (string Stem in LibraryNameStems)
				{
					string LibraryName = "icu" + Stem + LibraryNamePostfix + "53" + "." + "dll";
					PublicDelayLoadDLLs.Add(LibraryName);
				}

				if(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
				{
					string BinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/VS{2}/", ICUVersion, Target.Platform.ToString(), Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
					foreach(string Stem in LibraryNameStems)
					{
						string LibraryName = BinariesDir + String.Format("icu{0}{1}53.dll", Stem, LibraryNamePostfix);
						RuntimeDependencies.Add(LibraryName);
					}
				}

				bNeedsDlls = true;

				break;
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string StaticLibraryExtension = "a";

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
					TargetSpecificPath += Target.Architecture + "/";
			}
			else
			{
					PublicLibraryPaths.Add(TargetSpecificPath + "ARMv7/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "ARM64/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x86/lib");
					PublicLibraryPaths.Add(TargetSpecificPath + "x64/lib");
			}

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			// Library Paths
			// Temporarily? only link statically on Linux too
			//EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.Android || Target.IsMonolithic) ? EICULinkType.Static : EICULinkType.Dynamic;
			EICULinkType ICULinkType = EICULinkType.Static;
			switch (ICULinkType)
			{
				case EICULinkType.Static:
					foreach (string Stem in LibraryNameStems)
					{
						string LibraryName = "icu" + Stem + LibraryNamePostfix;
						if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
						{
							// Linux needs the path, not just the filename, to avoid linking to system lib instead of a bundled one.
							PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + "lib" + LibraryName + "." + StaticLibraryExtension); 
						}
						else
						{
							// other platforms will just use the library name
							PublicAdditionalLibraries.Add(LibraryName);
						}
					}
					break;
				case EICULinkType.Dynamic:
					if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
					{
						string PathToBinary = String.Format("$(EngineDir)/Binaries/ThirdParty/ICU/{0}/{1}/{2}/", ICUVersion, Target.Platform.ToString(),
							Target.Architecture);

						foreach (string Stem in LibraryNameStems)
						{
							string LibraryName = "icu" + Stem + LibraryNamePostfix;
							string LibraryPath = Target.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Linux/" + Target.Architecture + "/";

							PublicLibraryPaths.Add(LibraryPath);
							PublicAdditionalLibraries.Add(LibraryName);

							// add runtime dependencies (for staging)
							RuntimeDependencies.Add(PathToBinary + "lib" + LibraryName + ".so");
							RuntimeDependencies.Add(PathToBinary + "lib" + LibraryName + ".so.53");  // version-dependent
						}
					}
					break;
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			string StaticLibraryExtension = "a";
			string DynamicLibraryExtension = "dylib";

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
				"d" : string.Empty;

			EICULinkType ICULinkType = (Target.Platform == UnrealTargetPlatform.IOS || (Target.LinkType == TargetLinkType.Monolithic)) ? EICULinkType.Static : EICULinkType.Dynamic;
			// Library Paths
			switch (ICULinkType)
			{
				case EICULinkType.Static:
					foreach (string Stem in LibraryNameStems)
					{
						string LibraryName = "libicu" + Stem + LibraryNamePostfix + "." + StaticLibraryExtension;
						PublicAdditionalLibraries.Add(TargetSpecificPath + "lib/" + LibraryName);
					}
					break;
				case EICULinkType.Dynamic:
					foreach (string Stem in LibraryNameStems)
					{
						if (Target.Platform == UnrealTargetPlatform.Mac)
						{
							string LibraryName = "libicu" + Stem + ".53.1" + LibraryNamePostfix + "." + DynamicLibraryExtension;
							string LibraryPath = Target.UEThirdPartyBinariesDirectory + "ICU/icu4c-53_1/Mac/" + LibraryName;

							PublicDelayLoadDLLs.Add(LibraryPath);
							RuntimeDependencies.Add(LibraryPath);
						}
					}

					bNeedsDlls = true;

					break;
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			// we don't bother with debug libraries on HTML5. Mainly because debugging isn't viable on html5 currently
			string StaticLibraryExtension = "bc";

			string[] LibraryNameStems =
			{
				"data", // Data
				"uc",   // Unicode Common
				"i18n", // Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};

			string OpimizationSuffix = "";
			if (Target.bCompileForSize)
			{
				OpimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OpimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OpimizationSuffix = "_O3";
				}
			}

			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = "libicu" + Stem + OpimizationSuffix + "." + StaticLibraryExtension;
				PublicAdditionalLibraries.Add(TargetSpecificPath + LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug) ?
				"d" : string.Empty;
			string LibraryExtension = "lib";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "PS4/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Switch)
		{
			string LibraryNamePrefix = "sicu";
			string[] LibraryNameStems =
			{
				"dt",	// Data
				"uc",   // Unicode Common
				"in",	// Internationalization
				"le",   // Layout Engine
				"lx",   // Layout Extensions
				"io"	// Input/Output
			};
			string LibraryNamePostfix = ""; //(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : string.Empty;
			string LibraryExtension = "a";
			foreach (string Stem in LibraryNameStems)
			{
				string LibraryName = ICURootPath + "Switch/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
				PublicAdditionalLibraries.Add(LibraryName);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Use reflection to allow type not to exist if console code is not present
			System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
			if (XboxOnePlatformType != null)
			{
				string LibraryNamePrefix = "sicu";
				string[] LibraryNameStems =
				{
					"dt",	// Data
					"uc",   // Unicode Common
					"in",	// Internationalization
					"le",   // Layout Engine
					"lx",   // Layout Extensions
					"io"	// Input/Output
				};
				string LibraryNamePostfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ?
					"d" : string.Empty;
				string LibraryExtension = "lib";
				foreach (string Stem in LibraryNameStems)
				{
					System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
					string LibraryName = ICURootPath + "XboxOne/VS" + VersionName.ToString() + "/lib/" + LibraryNamePrefix + Stem + LibraryNamePostfix + "." + LibraryExtension;
					PublicAdditionalLibraries.Add(LibraryName);
				}
			}
		}

		// common defines (this used to be inside an if TargetPlatform == ___ block that looked to include every platform known to man, so just removed the if)
		// Definitions
		PublicDefinitions.Add("U_USING_ICU_NAMESPACE=0"); // Disables a using declaration for namespace "icu".
		PublicDefinitions.Add("U_STATIC_IMPLEMENTATION"); // Necessary for linking to ICU statically.
		PublicDefinitions.Add("UNISTR_FROM_CHAR_EXPLICIT=explicit"); // Makes UnicodeString constructors for ICU character types explicit.
		PublicDefinitions.Add("UNISTR_FROM_STRING_EXPLICIT=explicit"); // Makes UnicodeString constructors for "char"/ICU string types explicit.
		PublicDefinitions.Add("UCONFIG_NO_TRANSLITERATION=1"); // Disables declarations and compilation of unused ICU transliteration functionality.
		

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			// Definitions
			PublicDefinitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
			PublicDefinitions.Add("U_PLATFORM=U_PF_ORBIS");
		}

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// Definitions
			PublicDefinitions.Add("ICU_NO_USER_DATA_OVERRIDE=1");
			PublicDefinitions.Add("U_PLATFORM=U_PF_DURANGO");
		}

		PublicDefinitions.Add("NEEDS_ICU_DLLS=" + (bNeedsDlls ? "1" : "0"));
	}
}
