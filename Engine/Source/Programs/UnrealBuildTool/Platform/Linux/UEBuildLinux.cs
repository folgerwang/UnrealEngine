// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;
using System.Text.RegularExpressions;

namespace UnrealBuildTool
{
	/** Architecture as stored in the ini. */
	enum LinuxArchitecture
	{
		/** x86_64, most commonly used architecture.*/
		X86_64UnknownLinuxGnu,

		/** A.k.a. AArch32, ARM 32-bit with hardware floats */
		ArmUnknownLinuxGnueabihf,

		/** AArch64, ARM 64-bit */
		AArch64UnknownLinuxGnueabi,

		/** i686, Intel 32-bit */
		I686UnknownLinuxGnu
	}

	/// <summary>
	/// Linux-specific target settings
	/// </summary>
	public class LinuxTargetRules
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxTargetRules()
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Enables address sanitizer (ASan)
		/// </summary>
		[CommandLine("-EnableASan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableAddressSanitizer")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables thread sanitizer (TSan)
		/// </summary>
		[CommandLine("-EnableTSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableThreadSanitizer")]
		public bool bEnableThreadSanitizer = false;

		/// <summary>
		/// Enables undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableUBSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableUndefinedBehaviorSanitizer")]
		public bool bEnableUndefinedBehaviorSanitizer = false;

		/// <summary>
		/// Whether or not to preserve the portable symbol file produced by dump_syms
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/LinuxPlatform.LinuxTargetSettings")]
		public bool bPreservePSYM = false;
	}

	/// <summary>
	/// Read-only wrapper for Linux-specific target settings
	/// </summary>
	public class ReadOnlyLinuxTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private LinuxTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyLinuxTargetRules(LinuxTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>

        #region Read-only accessor properties
        #if !__MonoCS__
        #pragma warning disable CS1591
        #endif

		public bool bPreservePSYM
		{
			get { return Inner.bPreservePSYM; }
		}

		public bool bEnableAddressSanitizer
		{
			get { return Inner.bEnableAddressSanitizer; }
		}

		public bool bEnableThreadSanitizer
		{
			get { return Inner.bEnableThreadSanitizer; }
		}

		public bool bEnableUndefinedBehaviorSanitizer
		{
			get { return Inner.bEnableUndefinedBehaviorSanitizer; }
		}

        #if !__MonoCS__
        #pragma warning restore CS1591
        #endif
        #endregion
	}

	class LinuxPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Linux architecture (compiler target triplet)
		/// </summary>
		// FIXME: for now switching between architectures is hard-coded
		public const string DefaultArchitecture = "x86_64-unknown-linux-gnu";
		//public const string DefaultArchitecture = "arm-unknown-linux-gnueabihf";
		//public const string DefaultArchitecture = "aarch64-unknown-linux-gnueabi";

		/// <summary>
		/// SDK in use by the platform
		/// </summary>
		protected LinuxPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxPlatform(LinuxPlatformSDK InSDK) 
			: this(UnrealTargetPlatform.Linux, CppPlatform.Linux, InSDK)
		{
			SDK = InSDK;
		}

		public LinuxPlatform(UnrealTargetPlatform UnrealTarget, CppPlatform InCppPlatform, LinuxPlatformSDK InSDK)
			: base(UnrealTarget, InCppPlatform)
		{
			SDK = InSDK;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Find the default architecture for the given project
		/// </summary>
		public override string GetDefaultArchitecture(FileReference ProjectFile)
		{
			string ActiveArchitecture = DefaultArchitecture;

			// read settings from the config
			string EngineIniPath = ProjectFile != null ? ProjectFile.Directory.FullName : null;
			if (String.IsNullOrEmpty(EngineIniPath))
			{
				// If the project file hasn't been specified, try to get the path from -remoteini command line param
				EngineIniPath = UnrealBuildTool.GetRemoteIniPath();
			}
			DirectoryReference EngineIniDir = !String.IsNullOrEmpty(EngineIniPath) ? new DirectoryReference(EngineIniPath) : null;

			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, EngineIniDir, UnrealTargetPlatform.Linux);

			string LinuxArchitectureString;
			if (Ini.GetString("/Script/LinuxTargetPlatform.LinuxTargetSettings", "TargetArchitecture", out LinuxArchitectureString))
			{
				LinuxArchitecture Architecture;
				if (Enum.TryParse(LinuxArchitectureString, out Architecture))
				{
					switch (Architecture)
					{
						default:
							System.Console.WriteLine("Architecture enum value {0} does not map to a valid triplet.", Architecture);
							break;

						case LinuxArchitecture.X86_64UnknownLinuxGnu:
							ActiveArchitecture = "x86_64-unknown-linux-gnu";
							break;

						case LinuxArchitecture.ArmUnknownLinuxGnueabihf:
							ActiveArchitecture = "arm-unknown-linux-gnueabihf";
							break;

						case LinuxArchitecture.AArch64UnknownLinuxGnueabi:
							ActiveArchitecture = "aarch64-unknown-linux-gnueabi";
							break;

						case LinuxArchitecture.I686UnknownLinuxGnu:
							ActiveArchitecture = "i686-unknown-linux-gnu";
							break;
					}
				}
			}

			return ActiveArchitecture;
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public override string GetFolderNameForArchitecture(string Architecture)
		{
			// shorten the string (heuristically)
			uint Sum = 0;
			int Len = Architecture.Length;
			for (int Index = 0; Index < Len; ++Index)
			{
				Sum += (uint)(Architecture[Index]);
				Sum <<= 1;	// allowed to overflow
			}
			return Sum.ToString("X");
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			Target.bCompileSimplygon = false;
			Target.bCompileSimplygonSSF = false;
			// depends on arch, APEX cannot be as of November'16 compiled for AArch32/64
			Target.bCompileAPEX = Target.Architecture.StartsWith("x86_64");
			Target.bCompileNvCloth = Target.Architecture.StartsWith("x86_64");

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSimplygon = false;
				Target.bCompileSimplygonSSF = false;
				Target.bCompileCEF3 = false;
			}

			// Force using the ANSI allocator if ASan or TSan is enabled
			if (Target.LinuxPlatform.bEnableAddressSanitizer ||
				Target.LinuxPlatform.bEnableThreadSanitizer)
			{
				Target.GlobalDefinitions.Add("FORCE_ANSI_ALLOCATOR=1");
			}

			// check if OS update invalidated our build
			Target.bCheckSystemHeadersForModification = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux);

			// At the moment ICU has not been compiled for AArch64 and i686.
			if (Target.Architecture.StartsWith("aarch64") || Target.Architecture.StartsWith("i686"))
			{
				Target.bCompileICU = false;
			}

			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				// When generating project files we need intellisense generator to include info from all modules,
				// including editor-only third party libs
				Target.bCompileLeanAndMeanUE = false;
			}
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public override bool RequiresArchitectureSuffix()
		{
			// Linux ignores architecture-specific names, although it might be worth it to prepend architecture
			return false;
		}

		public override bool CanUseXGE()
		{
			// [RCL] 2018-05-02: disabling XGE even during a native build because the support is not ready and you can have mysterious build failures when ib_console is installed.
			// [RCL] 2018-07-10: enabling XGE for Windows to see if the crash from 2016 still persists. Please disable if you see spurious build errors that don't repro without XGE
			// [bschaefer] 2018-08-24: disabling XGE due to a bug where XGE seems to be lower casing folders names that are headers ie. misc/Header.h vs Misc/Header.h
			return false;//BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64;
		}

		public override bool CanUseParallelExecutor()
		{
			// No known problems with parallel executor, always use for build machines
			return true;
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			if (FileName.StartsWith("lib"))
			{
				return IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".a")
					|| IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".so");
			}
			else
			{
				return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".so")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".a");
			}
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extensions to use for debug info for the given binary type
		/// </summary>
		/// <param name="InTarget">Rules for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string[]    The debug info extensions (i.e. 'pdb')</returns>
		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					if (InTarget.LinuxPlatform.bPreservePSYM)
					{
						return new string[] {".psym", ".sym", ".debug"};
					}
					else
					{
						return new string[] {".sym", ".debug"};
					}
			}
			return new string [] {};
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (Target.bForceBuildTargetPlatforms && ModuleName == "TargetPlatform")
				{
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
				}
			}
		}

		/// <summary>
		/// Converts the passed in path from UBT host to compiler native format.
		/// </summary>
		public override string ConvertPath(string OriginalPath)
		{
			return LinuxToolChain.ConvertPath(OriginalPath);
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			bool bBuildShaderFormats = Target.bForceBuildShaderFormats;

			if (!Target.bBuildRequiresCookedData)
			{
				if (ModuleName == "TargetPlatform")
				{
					bBuildShaderFormats = true;
				}
			}

			// allow standalone tools to use target platform modules, without needing Engine
			if (ModuleName == "TargetPlatform")
			{
				if (Target.bForceBuildTargetPlatforms)
				{
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxNoEditorTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxClientTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxServerTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("AllDesktopTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
					Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
				}
			}
		}

		public virtual void SetUpSpecificEnvironment(CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("PLATFORM_LINUX=1");
			CompileEnvironment.Definitions.Add("PLATFORM_UNIX=1");

			CompileEnvironment.Definitions.Add("LINUX=1"); // For libOGG

			// this define does not set jemalloc as default, just indicates its support
			CompileEnvironment.Definitions.Add("PLATFORM_SUPPORTS_JEMALLOC=1");
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("WITH_DATABASE_SUPPORT=0");		//@todo linux: valid?

			// During the native builds, check the system includes as well (check toolchain when cross-compiling?)
			string BaseLinuxPath = SDK.GetBaseLinuxPathForArchitecture(Target.Architecture);
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux && String.IsNullOrEmpty(BaseLinuxPath))
			{
				CompileEnvironment.IncludePaths.SystemIncludePaths.Add(new DirectoryReference("/usr/include"));
			}

			if (CompileEnvironment.bAllowLTCG != LinkEnvironment.bAllowLTCG)
			{
				Log.TraceWarning("Inconsistency between LTCG settings in Compile and Link environments: link one takes priority");
				CompileEnvironment.bAllowLTCG = LinkEnvironment.bAllowLTCG;
			}

			// disable to LTO for modular builds
			if (CompileEnvironment.bAllowLTCG && Target.LinkType != TargetLinkType.Monolithic)
			{
				Log.TraceWarning("LTO (LTCG) for modular builds is not supported, disabling it");
				CompileEnvironment.bAllowLTCG = false;
				LinkEnvironment.bAllowLTCG = false;
			}

			// link with Linux libraries.
			LinkEnvironment.AdditionalLibraries.Add("pthread");

			// let this class or a sub class do settings specific to that class
			SetUpSpecificEnvironment(CompileEnvironment, LinkEnvironment);
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			LinuxToolChainOptions Options = LinuxToolChainOptions.None;

			if(Target.LinuxPlatform.bEnableAddressSanitizer)
			{
				Options |= LinuxToolChainOptions.EnableAddressSanitizer;
			}
			if(Target.LinuxPlatform.bEnableThreadSanitizer)
			{
				Options |= LinuxToolChainOptions.EnableThreadSanitizer;
			}
			if(Target.LinuxPlatform.bEnableUndefinedBehaviorSanitizer)
			{
				Options |= LinuxToolChainOptions.EnableUndefinedBehaviorSanitizer;
			}

			return new LinuxToolChain(Target.Architecture, SDK, Target.LinuxPlatform.bPreservePSYM, Options);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
		}
	}

	class LinuxPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// This is the SDK version we support
		/// </summary>
		static string ExpectedSDKVersion = "v12_clang-6.0.1-centos7";	// now unified for all the architectures

		/// <summary>
		/// Platform name (embeds architecture for now)
		/// </summary>
		static private string TargetPlatformName = "Linux_x64";

		/// <summary>
		/// Whether to compile with the verbose flag
		/// </summary>
		public bool bVerboseCompiler = false;

		/// <summary>
		/// Whether to link with the verbose flag
		/// </summary>
		public bool bVerboseLinker = false;

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public override string GetSDKTargetPlatformName()
		{
			return TargetPlatformName;
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected override string GetRequiredSDKString()
		{
			return ExpectedSDKVersion;
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.0";
		}

		protected override bool PreferAutoSDK()
		{
			// having LINUX_ROOT set (for legacy reasons or for convenience of cross-compiling certain third party libs) should not make UBT skip AutoSDKs
			return true;
		}

		public string HaveLinuxDependenciesFile()
		{
			// This file must have no extension so that GitDeps considers it a binary dependency - it will only be pulled by the Setup script if Linux is enabled.
			return "HaveLinuxDependencies";
		}

		public string SDKVersionFileName()
		{
			return "ToolchainVersion.txt";
		}

		protected static int GetLinuxToolchainVersionFromString(string SDKVersion)
		{
			// Example: v11_clang-5.0.0-centos7
			string FullVersionPattern = @"^v[0-9]+_.*$";
			Regex Regex = new Regex(FullVersionPattern);
			if (Regex.IsMatch(SDKVersion))
			{
				string VersionPattern = @"[0-9]+";
				Regex = new Regex(VersionPattern);
				Match Match = Regex.Match(SDKVersion);
				if (Match.Success)
				{
					int Version;
					bool bParsed = Int32.TryParse(Match.Value, out Version);
					if (bParsed)
					{
						return Version;
					}
				}
			}

			return -1;
		}

		public bool CheckSDKCompatible(string VersionString, out string ErrorMessage)
		{
			int Version = GetLinuxToolchainVersionFromString(VersionString);
			int ExpectedVersion = GetLinuxToolchainVersionFromString(ExpectedSDKVersion);
			if (Version >= 0 && ExpectedVersion >= 0 && Version != ExpectedVersion)
			{
				if (Version < ExpectedVersion)
				{
					ErrorMessage = "Toolchain found \"" + VersionString + "\" is older then the required version \"" + ExpectedSDKVersion + "\"";
					return false;
				}
				else
				{
					Log.TraceWarning("Toolchain \"{0}\" is newer than the expected version \"{1}\", you may run into compilation errors", VersionString, ExpectedSDKVersion);
				}
			}
			else if (VersionString != ExpectedSDKVersion)
			{
				ErrorMessage = "Failed to find a supported toolchain, found \"" + VersionString + "\", expected \"" + ExpectedSDKVersion + "\"";
				return false;
			}

			ErrorMessage = "";
			return true;
		}

		/// <summary>
		/// Returns the in-tree root for the Linux Toolchain for this host platform.
		/// </summary>
		private static DirectoryReference GetInTreeSDKRoot()
		{
			return DirectoryReference.Combine(UnrealBuildTool.RootDirectory, "Engine/Extras/ThirdPartyNotUE/SDKs", "Host" + BuildHostPlatform.Current.Platform, TargetPlatformName);
		}

		/// <summary>
		/// Whether a host can use its system sdk for this platform
		/// </summary>
		public virtual bool CanUseSystemCompiler()
		{
			return (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux);
		}

		/// <summary>
		/// Returns the root SDK path for all architectures
		/// WARNING: Do not cache this value - it may be changed after sourcing OutputEnvVars.txt
		/// </summary>
		/// <returns>Valid SDK string</returns>
		public virtual string GetSDKLocation()
		{
			// if new multi-arch toolchain is used, prefer it
			string MultiArchRoot = Environment.GetEnvironmentVariable("LINUX_MULTIARCH_ROOT");

			if (String.IsNullOrEmpty(MultiArchRoot))
			{
				// check if in-tree SDK is available
				DirectoryReference InTreeSDKVersionRoot = GetInTreeSDKRoot();
				if (InTreeSDKVersionRoot != null)
				{
					DirectoryReference InTreeSDKVersionPath = DirectoryReference.Combine(InTreeSDKVersionRoot, ExpectedSDKVersion);
					if (DirectoryReference.Exists(InTreeSDKVersionPath))
					{
						MultiArchRoot = InTreeSDKVersionPath.FullName;
					}
				}
			}
			return MultiArchRoot;
		}

		/// <summary>
		/// Returns the SDK path for a specific architecture
		/// WARNING: Do not cache this value - it may be changed after sourcing OutputEnvVars.txt
		/// </summary>
		/// <returns>Valid SDK string</returns>
		public virtual string GetBaseLinuxPathForArchitecture(string Architecture)
		{
			// if new multi-arch toolchain is used, prefer it
			string MultiArchRoot = GetSDKLocation();
			string BaseLinuxPath;

			if (!String.IsNullOrEmpty(MultiArchRoot))
			{
				BaseLinuxPath = Path.Combine(MultiArchRoot, Architecture);
			}
			else
			{
				// use cross linux toolchain if LINUX_ROOT is specified
				BaseLinuxPath = Environment.GetEnvironmentVariable("LINUX_ROOT");
			} 
			return BaseLinuxPath;
		}

		/// <summary>
		/// Whether the path contains a valid clang version
		/// </summary>
		private static bool IsValidClangPath(DirectoryReference BaseLinuxPath)
		{
			FileReference ClangPath = FileReference.Combine(BaseLinuxPath, @"bin", (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) ? "clang++.exe" : "clang++");
			return FileReference.Exists(ClangPath);
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform
		/// </summary>
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			// FIXME: UBT should loop across all the architectures and compile for all the selected ones.

			// do not cache this value - it may be changed after sourcing OutputEnvVars.txt
			string BaseLinuxPath = GetBaseLinuxPathForArchitecture(LinuxPlatform.DefaultArchitecture);

			// BaseLinuxPath is specified
			if (!String.IsNullOrEmpty(BaseLinuxPath))
			{
				// paths to our toolchains
				BaseLinuxPath = BaseLinuxPath.Replace("\"", "");

				if (IsValidClangPath(new DirectoryReference(BaseLinuxPath)))
				{
					return SDKStatus.Valid;
				}
			}

			if (CanUseSystemCompiler())
			{
				if (!String.IsNullOrEmpty(LinuxCommon.WhichClang()) || !String.IsNullOrEmpty(LinuxCommon.WhichGcc()))
				{
					return SDKStatus.Valid;
				}
			}

			return SDKStatus.Invalid;
		}
	}

	class LinuxPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			LinuxPlatformSDK SDK = new LinuxPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			if ((ProjectFileGenerator.bGenerateProjectFiles == true) || (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid))
			{
				FileReference LinuxTargetPlatformFile = FileReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Developer", "Linux", "LinuxTargetPlatform", "LinuxTargetPlatform.Build.cs");
				if (FileReference.Exists(LinuxTargetPlatformFile))
				{
					// Register this build platform for Linux
					Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.Linux.ToString());
					UEBuildPlatform.RegisterBuildPlatform(new LinuxPlatform(SDK));
					UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Unix);
				}
			}
		}
	}
}
