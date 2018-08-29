// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using Ionic.Zip;
using Ionic.Zlib;
using Tools.DotNETCommon;
using System.Security.Cryptography.X509Certificates;

namespace UnrealBuildTool
{
	class IOSToolChainSettings : AppleToolChainSettings
	{
		/// <summary>
		/// Which version of the iOS SDK to target at build time
		/// </summary>
		[XmlConfigFile(Category = "IOSToolChain")]
		public string IOSSDKVersion = "latest";
		public readonly float IOSSDKVersionFloat = 0.0f;

		/// <summary>
		/// Which version of the iOS to allow at build time
		/// </summary>
		[XmlConfigFile(Category = "IOSToolChain")]
		public string BuildIOSVersion = "7.0";

		/// <summary>
		/// Directory for the developer binaries
		/// </summary>
		public string ToolchainDir = "";

		/// <summary>
		/// Location of the SDKs
		/// </summary>
		public readonly string BaseSDKDir;
		public readonly string BaseSDKDirSim;

		public readonly string DevicePlatformName;
		public readonly string SimulatorPlatformName;

		public IOSToolChainSettings() : this("iPhoneOS", "iPhoneSimulator")
		{
		}

		protected IOSToolChainSettings(string DevicePlatformName, string SimulatorPlatformName) : base(true)
		{
			XmlConfig.ApplyTo(this);

			this.DevicePlatformName = DevicePlatformName;
			this.SimulatorPlatformName = SimulatorPlatformName;

			// update cached paths
			BaseSDKDir = XcodeDeveloperDir + "Platforms/" + DevicePlatformName + ".platform/Developer/SDKs";
			BaseSDKDirSim = XcodeDeveloperDir + "Platforms/" + SimulatorPlatformName + ".platform/Developer/SDKs";
			ToolchainDir = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/";

			// make sure SDK is selected
			SelectSDK(BaseSDKDir, DevicePlatformName, ref IOSSDKVersion, true);

			// convert to float for easy comparison
			IOSSDKVersionFloat = float.Parse(IOSSDKVersion, System.Globalization.CultureInfo.InvariantCulture);
		}
	}

	class IOSToolChain : AppleToolChain
	{
		protected IOSProjectSettings ProjectSettings;

		public IOSToolChain(FileReference InProjectFile, IOSProjectSettings InProjectSettings)
			: this(CppPlatform.IOS, InProjectFile, InProjectSettings, () => new IOSToolChainSettings())
		{
		}

		protected IOSToolChain(CppPlatform TargetPlatform, FileReference InProjectFile, IOSProjectSettings InProjectSettings, Func<IOSToolChainSettings> InCreateSettings)
			: base(TargetPlatform, InProjectFile)
		{
			ProjectSettings = InProjectSettings;
			Settings = new Lazy<IOSToolChainSettings>(InCreateSettings);
		}

		// ***********************************************************************
		// * NOTE:
		// *  Do NOT change the defaults to set your values, instead you should set the environment variables
		// *  properly in your system, as other tools make use of them to work properly!
		// *  The defaults are there simply for examples so you know what to put in your env vars...
		// ***********************************************************************

		// If you are looking for where to change the remote compile server name, look in RemoteToolChain.cs

		/// <summary>
		/// If this is set, then we don't do any post-compile steps except moving the executable into the proper spot on the Mac
		/// </summary>
		[XmlConfigFile]
		public static bool bUseDangerouslyFastMode = false;

		/// <summary>
		/// The lazily constructed settings for the toolchain
		/// </summary>
		private Lazy<IOSToolChainSettings> Settings;

		/// <summary>
		/// Which compiler frontend to use
		/// </summary>
		private const string IOSCompiler = "clang++";

		/// <summary>
		/// Which linker frontend to use
		/// </summary>
		private const string IOSLinker = "clang++";

		/// <summary>
		/// Which library archiver to use
		/// </summary>
		private const string IOSArchiver = "libtool";

		public static List<FileReference> BuiltBinaries = new List<FileReference>();

		private bool bUseMallocProfiler;

		/// <summary>
		/// Additional frameworks stored locally so we have access without LinkEnvironment
		/// </summary>
		public static List<UEBuildFramework> RememberedAdditionalFrameworks = new List<UEBuildFramework>();

        public override string GetSDKVersion()
        {
            return Settings.Value.IOSSDKVersionFloat.ToString();
        }

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Target.bCreateStubIPA && Binary.Type != UEBuildBinaryType.StaticLibrary)
			{
				FileReference StubFile = FileReference.Combine(Binary.OutputFilePath.Directory, Binary.OutputFilePath.GetFileNameWithoutExtension() + ".stub");
				BuildProducts.Add(StubFile, BuildProductType.Package);

                if (CppPlatform == CppPlatform.TVOS)
                {
					FileReference AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "AssetCatalog", "Assets.car");
                    BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
                }
				else if (CppPlatform == CppPlatform.IOS && Settings.Value.IOSSDKVersionFloat >= 11.0f)
				{
					int Index = Binary.OutputFilePath.GetFileNameWithoutExtension().IndexOf("-");
					string OutputFile = Binary.OutputFilePath.GetFileNameWithoutExtension().Substring(0, Index > 0 ? Index : Binary.OutputFilePath.GetFileNameWithoutExtension().Length);
					FileReference AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "Assets.car");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon20x20@2x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon20x20@2x~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon20x20@3x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon20x20~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon29x29@2x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon29x29@2x~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon29x29@3x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon29x29~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon40x40@2x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon40x40@2x~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon40x40@3x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon40x40~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon60x60@2x.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon76x76@2x~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon76x76~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
					AssetFile = FileReference.Combine(Binary.OutputFilePath.Directory, "Payload", OutputFile + ".app", "AppIcon83.5x83.5@2x~ipad.png");
					BuildProducts.Add(AssetFile, BuildProductType.RequiredResource);
				}
			}
            if ((ProjectSettings.bGeneratedSYMFile == true || ProjectSettings.bGeneratedSYMBundle == true) && (ProjectSettings.bGenerateCrashReportSymbols || bUseMallocProfiler) && Binary.Type == UEBuildBinaryType.Executable)
            {
                FileReference DebugFile = FileReference.Combine(Binary.OutputFilePath.Directory, Binary.OutputFilePath.GetFileNameWithoutExtension() + ".udebugsymbols");
                BuildProducts.Add(DebugFile, BuildProductType.SymbolFile);
            }
        }

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="OutputType">Type of build product</param>
		public override bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return OutputType == BuildProductType.Executable;
		}

		public override void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
			base.SetUpGlobalEnvironment(Target);
			bUseMallocProfiler = Target.bUseMallocProfiler;
		}

		string GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
            string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";

			if (!Utils.IsRunningOnMono)
			{
				Result += " -fdiagnostics-format=msvc";
			}

			// Optionally enable exception handling (off by default since it generates extra code needed to propagate exceptions)
			if (CompileEnvironment.bEnableExceptions)
			{
				Result += " -fexceptions";
			}
			else
			{
				Result += " -fno-exceptions";
			}

			if (CompileEnvironment.bEnableObjCExceptions)
			{
				Result += " -fobjc-exceptions";
			}
			else
			{
				Result += " -fno-objc-exceptions";
			}

			string SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if(SanitizerMode != null && SanitizerMode == "YES")
			{
				Result += " -fsanitize=address";
			}
			
			string UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if(UndefSanitizerMode != null && UndefSanitizerMode == "YES")
			{
				Result += " -fsanitize=undefined -fno-sanitize=bounds,enum,return,float-divide-by-zero";
			}

			Result += GetRTTIFlag(CompileEnvironment);
			Result += " -fvisibility=hidden"; // hides the linker warnings with PhysX

			// 			if (CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Shipping)
			// 			{
			// 				Result += " -flto";
			// 			}

			Result += " -Wall -Werror";
			Result += " -Wdelete-non-virtual-dtor";

			if (CompileEnvironment.bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// fix for Xcode 8.3 enabling nonportable include checks, but p4 has some invalid cases in it
			if (Settings.Value.IOSSDKVersionFloat >= 10.3)
			{
				Result += " -Wno-nonportable-include-path";
			}

			if (IsBitcodeCompilingEnabled(CompileEnvironment.Configuration))
			{
				Result += " -fembed-bitcode";
			}

			Result += " -c";

			// What architecture(s) to build for
			Result += GetArchitectureArgument(CompileEnvironment.Configuration, CompileEnvironment.Architecture);

			if (CompileEnvironment.Architecture == "-simulator")
			{
				Result += " -isysroot " + Settings.Value.BaseSDKDirSim + "/" + Settings.Value.SimulatorPlatformName + Settings.Value.IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += " -isysroot " + Settings.Value.BaseSDKDir + "/" +  Settings.Value.DevicePlatformName + Settings.Value.IOSSDKVersion + ".sdk";
			}

			Result += " -m" + GetXcodeMinVersionParam() + "=" + ProjectSettings.RuntimeVersion;
			
			bool bStaticAnalysis = false;
			string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
			if(StaticAnalysisMode != null && StaticAnalysisMode != "")
			{
				bStaticAnalysis = true;
			}

			// Optimize non- debug builds.
			if (CompileEnvironment.bOptimizeCode && !bStaticAnalysis)
			{
				if (CompileEnvironment.bOptimizeForSize)
				{
					Result += " -Oz";
				}
				else
				{
					Result += " -O3";
				}
			}
			else
			{
				Result += " -O0";
			}

			if (!CompileEnvironment.bUseInlining)
			{
				Result += " -fno-inline-functions";
			}

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -gdwarf-2";
			}

			// Add additional frameworks so that their headers can be found
			foreach (UEBuildFramework Framework in CompileEnvironment.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					Result += " -F\"" + GetExtractedFrameworkDir(Framework) + "\"";
				}
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x objective-c++-header";
			Result += " -std=c++14";
			Result += " -stdlib=libc++";
			return Result;
		}

		// Conditionally enable (default disabled) generation of information about every class with virtual functions for use by the C++ runtime type identification features 
		// (`dynamic_cast' and `typeid'). If you don't use those parts of the language, you can save some space by using -fno-rtti. 
		// Note that exception handling uses the same information, but it will generate it as needed. 
		static string GetRTTIFlag(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			if (CompileEnvironment.bUseRTTI)
			{
				Result = " -frtti";
			}
			else
			{
				Result = " -fno-rtti";
			}

			return Result;
		}

		static FileReference GetFrameworkZip(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetLocalFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			return FileReference.Combine(Framework.OwningModule.ModuleDirectory, Framework.FrameworkZipPath);
		}

		static DirectoryReference GetExtractedFrameworkDir(UEBuildFramework Framework)
		{
			if (Framework.OwningModule == null)
			{
				throw new BuildException("GetRemoteIntermediateFrameworkZipPath: No owning module for framework {0}", Framework.FrameworkName);
			}

			return DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "UnzippedFrameworks", Framework.OwningModule.Name, Framework.FrameworkZipPath.Replace(".zip", ""));
		}

		static void CleanIntermediateDirectory(string Path)
		{
			string ResultsText;

			// Delete the local dest directory if it exists
			if (Directory.Exists(Path))
			{
				// this can deal with linked files
				RunExecutableAndWait("rm", String.Format("-rf \"{0}\"", Path), out ResultsText);
			}

			// Create the intermediate local directory
			RunExecutableAndWait("mkdir", String.Format("-p \"{0}\"", Path), out ResultsText);
		}

		bool IsBitcodeCompilingEnabled(CppConfiguration Configuration)
		{
			return Configuration == CppConfiguration.Shipping && ProjectSettings.bShipForBitcode;
		}

		public virtual string GetXcodeMinVersionParam()
		{
			return "iphoneos-version-min";
		}

		public virtual string GetArchitectureArgument(CppConfiguration Configuration, string UBTArchitecture)
		{
            // get the list of architectures to compile
            string Archs =
				UBTArchitecture == "-simulator" ? "i386" :
				String.Join(",", (Configuration == CppConfiguration.Shipping) ? ProjectSettings.ShippingArchitectures : ProjectSettings.NonShippingArchitectures);

			Log.TraceLogOnce("Compiling with these architectures: " + Archs);

			// parse the string
			string[] Tokens = Archs.Split(",".ToCharArray());


			string Result = "";

			foreach (string Token in Tokens)
			{
				Result += " -arch " + Token;
			}

            //  Remove this in 4.16 
            //  Commented this out, for now. @Pete let's conditionally check this when we re-implement this fix. 
            //  Result += " -mcpu=cortex-a9";

			return Result;
		}

		public string GetAdditionalLinkerFlags(CppConfiguration InConfiguration)
		{
			if (InConfiguration != CppConfiguration.Shipping)
			{
				return ProjectSettings.AdditionalLinkerFlags;
			}
			else
			{
				return ProjectSettings.AdditionalShippingLinkerFlags;
			}
		}

		string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
            string Result = "";

			Result += GetArchitectureArgument(LinkEnvironment.Configuration, LinkEnvironment.Architecture);

			bool bIsDevice = (LinkEnvironment.Architecture != "-simulator");
			Result += String.Format(" -isysroot {0}Platforms/{1}.platform/Developer/SDKs/{1}{2}.sdk",
				Settings.Value.XcodeDeveloperDir, bIsDevice? Settings.Value.DevicePlatformName : Settings.Value.SimulatorPlatformName, Settings.Value.IOSSDKVersion);

			if(IsBitcodeCompilingEnabled(LinkEnvironment.Configuration))
			{
				FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);

				Result += " -fembed-bitcode -Xlinker -bitcode_verify -Xlinker -bitcode_hide_symbols -Xlinker -bitcode_symbol_map ";
				Result += " -Xlinker " + Path.GetDirectoryName(OutputFile.AbsolutePath);
			}

			Result += " -dead_strip";
			Result += " -m" + GetXcodeMinVersionParam() + "=" + ProjectSettings.RuntimeVersion;
			Result += " -Wl";
			if(!IsBitcodeCompilingEnabled(LinkEnvironment.Configuration))
			{
				Result += "-no_pie";
			}
			Result += " -stdlib=libc++";
			Result += " -ObjC";
			//			Result += " -v";
			
			string SanitizerMode = Environment.GetEnvironmentVariable("ENABLE_ADDRESS_SANITIZER");
			if(SanitizerMode != null && SanitizerMode == "YES")
			{
				Result += " -rpath \"@executable_path/Frameworks/libclang_rt.asan_ios_dynamic.dylib\"";
				Result += " -fsanitize=address";
			}
			
			string UndefSanitizerMode = Environment.GetEnvironmentVariable("ENABLE_UNDEFINED_BEHAVIOR_SANITIZER");
			if(UndefSanitizerMode != null && UndefSanitizerMode == "YES")
			{
				Result += " -rpath \"@executable_path/libclang_rt.ubsan_ios_dynamic.dylib\"";
				Result += " -fsanitize=undefined";
			}

			Result += " " + GetAdditionalLinkerFlags(LinkEnvironment.Configuration);

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
                if (Framework != "ARKit" || Settings.Value.IOSSDKVersionFloat >= 11.0f)
                {
                    Result += " -framework " + Framework;
                }
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.AdditionalFrameworks)
			{
				if (Framework.OwningModule != null && Framework.FrameworkZipPath != null && Framework.FrameworkZipPath != "")
				{
					// If this framework has a zip specified, we'll need to setup the path as well
					Result += " -F\"" + GetExtractedFrameworkDir(Framework) + "\"";
				}

				Result += " -framework " + Framework.FrameworkName;
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Result += " -weak_framework " + Framework;
			}

			return Result;
		}

		static string GetArchiveArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -static";

			return Result;
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, ActionGraph ActionGraph)
		{
			string Arguments = GetCompileArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				PCHArguments += string.Format(" -include \"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename.FullName);
			}

			foreach(FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
			{
				Arguments += String.Format(" -include \"{0}\"", ForceIncludeFile.Location.FullName);
			}

			// Add include paths to the argument list.
			HashSet<DirectoryReference> AllIncludes = new HashSet<DirectoryReference>(CompileEnvironment.IncludePaths.UserIncludePaths);
			AllIncludes.UnionWith(CompileEnvironment.IncludePaths.SystemIncludePaths);
			foreach (DirectoryReference IncludePath in AllIncludes)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath.FullName);
			}

			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				string FilePCHArguments = "";
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Compile the file as a C++ PCH.
					FileArguments += GetCompileArguments_PCH();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else if (Extension == ".MM")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_MM();
					FileArguments += GetRTTIFlag(CompileEnvironment);
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
					FileArguments += GetRTTIFlag(CompileEnvironment);

					// only use PCH for .cpp files
					FilePCHArguments = PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				// Add the precompiled header
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
				{
					CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
				}

				// Upload the force included files
				foreach(FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
				{
					AddPrerequisiteSourceFile(CompileEnvironment, ForceIncludeFile, CompileAction.PrerequisiteItems);
				}

				string OutputFilePath = null;
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".gch"));

					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}
					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".o"));

					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);
					OutputFilePath = ObjectFile.AbsolutePath;
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

				string CompilerPath = Settings.Value.ToolchainDir + IOSCompiler;

				string AllArgs = FilePCHArguments + Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
/*				string SourceText = System.IO.File.ReadAllText(SourceFile.AbsolutePath);
				if (CompileEnvironment.bOptimizeForSize && (SourceFile.AbsolutePath.Contains("ElementBatcher.cpp") || SourceText.Contains("ElementBatcher.cpp") || SourceFile.AbsolutePath.Contains("AnimationRuntime.cpp") || SourceText.Contains("AnimationRuntime.cpp")
					|| SourceFile.AbsolutePath.Contains("AnimEncoding.cpp") || SourceText.Contains("AnimEncoding.cpp") || SourceFile.AbsolutePath.Contains("TextRenderComponent.cpp") || SourceText.Contains("TextRenderComponent.cpp")
					|| SourceFile.AbsolutePath.Contains("SWidget.cpp") || SourceText.Contains("SWidget.cpp") || SourceFile.AbsolutePath.Contains("SCanvas.cpp") || SourceText.Contains("SCanvas.cpp") || SourceFile.AbsolutePath.Contains("ShaderCore.cpp") || SourceText.Contains("ShaderCore.cpp")
                    || SourceFile.AbsolutePath.Contains("ParticleSystemRender.cpp") || SourceText.Contains("ParticleSystemRender.cpp")))
				{
					Log.TraceInformation("Forcing {0} to --O3!", SourceFile.AbsolutePath);

					AllArgs = AllArgs.Replace("-Oz", "-O3");
				}*/
				
				// Analyze and then compile using the shell to perform the indirection
				string StaticAnalysisMode = Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE");
				if(StaticAnalysisMode != null && StaticAnalysisMode != "" && OutputFilePath != null)
				{
					string TempArgs = "-c \"" + CompilerPath + " " + AllArgs + " --analyze -Wno-unused-command-line-argument -Xclang -analyzer-output=html -Xclang -analyzer-config -Xclang path-diagnostics-alternate=true -Xclang -analyzer-config -Xclang report-in-main-source-file=true -Xclang -analyzer-disable-checker -Xclang deadcode.DeadStores -o " + OutputFilePath.Replace(".o", ".html") + "; " + CompilerPath + " " + AllArgs + "\"";
					AllArgs = TempArgs;
					CompilerPath = "/bin/sh";
				}

				// RPC utility parameters are in terms of the Mac side
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = AllArgs; // Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.bShouldOutputStatusDescription = true;

				AddFrameworksToTrack(CompileEnvironment.AdditionalFrameworks, CompileAction);
			}
			return Result;
		}

		static void AddFrameworksToTrack(List<UEBuildFramework> Frameworks, Action DependentAction)
		{
			foreach (UEBuildFramework Framework in Frameworks)
			{
				if (Framework.OwningModule == null || Framework.FrameworkZipPath == null || Framework.FrameworkZipPath == "")
				{
					continue;	// Only care about frameworks that have a zip specified
				}

				// If we've already remembered this framework, skip
				if (RememberedAdditionalFrameworks.Contains(Framework))
				{
					continue;
				}

				// Remember any files we need to unzip
				RememberedAdditionalFrameworks.Add(Framework);
			}
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			string LinkerPath = Settings.Value.ToolchainDir +
				(LinkEnvironment.bIsBuildingLibrary ? IOSArchiver : IOSLinker);

			// Create an action that invokes the linker.
			Action LinkAction = ActionGraph.Add(ActionType.Link);

			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();

			// build this up over the rest of the function
			string LinkCommandArguments = LinkEnvironment.bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment);

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (DirectoryReference LibraryPath in LinkEnvironment.LibraryPaths)
				{
					LinkCommandArguments += string.Format(" -L\"{0}\"", LibraryPath.FullName);
				}

				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
				{
					// for absolute library paths, convert to remote filename
					if (!String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
					{
						// add it to the prerequisites to make sure it's built first (this should be the case of non-system libraries)
						FileItem LibFile = FileItem.GetItemByPath(AdditionalLibrary);
						LinkAction.PrerequisiteItems.Add(LibFile);

						// and add to the commandline
						LinkCommandArguments += string.Format(" \"{0}\"", Path.GetFullPath(AdditionalLibrary));
					}
					else
					{
						LinkCommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
					}
				}
			}

			// Handle additional framework assets that might need to be shadowed
			AddFrameworksToTrack(LinkEnvironment.AdditionalFrameworks, LinkAction);

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);

			// Add arguments to generate a map file too
			if (!LinkEnvironment.bIsBuildingLibrary && LinkEnvironment.bCreateMapFile)
			{
				FileItem MapFile = FileItem.GetItemByFileReference(new FileReference(OutputFile.Location.FullName + ".map"));
				LinkCommandArguments += string.Format(" -Wl,-map,\"{0}\"", MapFile.Location.FullName);
				LinkAction.ProducedItems.Add(MapFile);
			}

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string FileNamePath = string.Format("\"{0}\"", InputFile.AbsolutePath);
				InputFileNames.Add(FileNamePath);
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			if (LinkEnvironment.bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkCommandArguments += " " + Filename;
				}
				// @todo rocket lib: the -filelist command should take a response file (see else condition), except that it just says it can't
				// find the file that's in there. Rocket.lib may overflow the commandline by putting all files on the commandline, so this 
				// may be needed:
				// LinkCommandArguments += string.Format(" -filelist \"{0}\"", ConvertPath(ResponsePath));
			}
			else
			{
				bool bIsUE4Game = LinkEnvironment.OutputFilePath.FullName.Contains("UE4Game");
				FileReference ResponsePath = FileReference.Combine(((!bIsUE4Game && ProjectFile != null) ? ProjectFile.Directory : UnrealBuildTool.EngineDirectory), "Intermediate", "Build", LinkEnvironment.Platform.ToString(), "LinkFileList_" + LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".tmp");
				FileItem.CreateIntermediateTextFile(ResponsePath, InputFileNames);
				LinkCommandArguments += string.Format(" @\"{0}\"", ResponsePath.FullName);
			}

			// Add the output file to the command-line.
			LinkCommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkCommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = string.Format("{0}", OutputFile.AbsolutePath);

			LinkAction.CommandPath = "sh";
			if(LinkEnvironment.Configuration == CppConfiguration.Shipping && Path.GetExtension(OutputFile.AbsolutePath) != ".a")
			{
				// When building a shipping package, symbols are stripped from the exe as the last build step. This is a problem
				// when re-packaging and no source files change because the linker skips symbol generation and dsymutil will 
				// recreate a new .dsym file from a symboless exe file. It's just sad. To make things happy we need to delete 
				// the output file to force the linker to recreate it with symbols again.
				string	linkCommandArguments = "-c '";

				linkCommandArguments += string.Format("rm -f \"{0}\";", OutputFile.AbsolutePath);
				linkCommandArguments += string.Format("rm -f \"{0}\\*.bcsymbolmap\";", Path.GetDirectoryName(OutputFile.AbsolutePath));
				linkCommandArguments += LinkerPath + " " + LinkCommandArguments + ";";

				linkCommandArguments += "'";

				LinkAction.CommandArguments = linkCommandArguments;
			}
			else
			{
				// This is not a shipping build so no need to delete the output file since symbols will not have been stripped from it.
				LinkAction.CommandArguments = string.Format("-c '{0} {1}'", LinkerPath, LinkCommandArguments);
			}

			return OutputFile;
		}

		static string GetAppBundleName(FileReference Executable)
		{
			// Get the app bundle name
			string AppBundleName = Executable.GetFileNameWithoutExtension();

			// Strip off any platform suffix
			int SuffixIdx = AppBundleName.IndexOf('-');
			if(SuffixIdx != -1)
			{
				AppBundleName = AppBundleName.Substring(0, SuffixIdx);
			}

			// Append the .app suffix
			return AppBundleName + ".app";
		}

		public static FileReference GetAssetCatalogFile(CppPlatform Platform, FileReference Executable)
		{
			// Get the output file
            if (Platform == CppPlatform.IOS)
            {
                return FileReference.Combine(Executable.Directory, "Payload", GetAppBundleName(Executable), "Assets.car");
            }
			else
			{
				return FileReference.Combine(Executable.Directory, "AssetCatalog", "Assets.car");
			}
		}

		public static string GetAssetCatalogArgs(CppPlatform Platform, string InputDir, string OutputDir)
		{
			StringBuilder Arguments = new StringBuilder("actool");
            Arguments.Append(" --output-format human-readable-text");
			Arguments.Append(" --notices");
			Arguments.Append(" --warnings");
            Arguments.AppendFormat(" --output-partial-info-plist '{0}/assetcatalog_generated_info.plist'", InputDir);
            Arguments.Append(" --app-icon AppIcon");
			if(Platform == CppPlatform.TVOS)
			{
				Arguments.Append(" --launch-image LaunchImage");
				Arguments.Append(" --filter-for-device-model AppleTV5,3");
				Arguments.Append(" --filter-for-device-os-version 9.2");
				Arguments.Append(" --target-device tv");
				Arguments.Append(" --minimum-deployment-target 9.2");
				Arguments.Append(" --platform appletvos");
			}
			else
			{
				Arguments.Append(" --product-type com.apple.product-type.application");
				Arguments.Append(" --target-device iphone");
				Arguments.Append(" --target-device ipad");
				Arguments.Append(" --minimum-deployment-target 9.0");
				Arguments.Append(" --platform iphoneos");
			}
            Arguments.Append(" --enable-on-demand-resources YES");
            Arguments.AppendFormat(" --compile '{0}'", OutputDir);
			Arguments.AppendFormat(" '{0}/Assets.xcassets'", InputDir);
			return Arguments.ToString();
		}

        /// <summary>
        /// Generates debug info for a given executable
        /// </summary>
        /// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="ActionGraph"></param>
        public FileItem GenerateDebugInfo(FileItem Executable, ActionGraph ActionGraph)
		{
            // Make a file item for the source and destination files
			string FullDestPathRoot = Path.Combine(Path.GetDirectoryName(Executable.AbsolutePath), Path.GetFileName(Executable.AbsolutePath) + ".dSYM");

            FileItem OutputFile = FileItem.GetItemByPath(FullDestPathRoot);
            FileItem ZipOutputFile = FileItem.GetItemByPath(FullDestPathRoot + ".zip");

            // Make the compile action
            Action GenDebugAction = ActionGraph.Add(ActionType.GenerateDebugInfo);

			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";
			if(ProjectSettings.bGeneratedSYMBundle)
			{
				GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; /usr/bin/dsymutil \"{0}\" -o \"{1}\"; cd \"{1}/..\"; zip -r -y -1 {2}.zip {2}'",
					Executable.AbsolutePath,
                    OutputFile.AbsolutePath,
					Path.GetFileName(FullDestPathRoot));
                GenDebugAction.ProducedItems.Add(ZipOutputFile);
                Log.TraceInformation("Zip file: " + ZipOutputFile.AbsolutePath);
            }
            else
			{
				GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; /usr/bin/dsymutil \"{0}\" -f -o \"{1}\"'",
						Executable.AbsolutePath,
						OutputFile.AbsolutePath);
			}
			GenDebugAction.PrerequisiteItems.Add(Executable);
            GenDebugAction.ProducedItems.Add(OutputFile);
            GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return (ProjectSettings.bGeneratedSYMBundle ? ZipOutputFile : OutputFile);
		}

        /// <summary>
        /// Generates pseudo pdb info for a given executable
        /// </summary>
        /// <param name="Executable">FileItem describing the executable to generate debug info for</param>
		/// <param name="ActionGraph"></param>
        public FileItem GeneratePseudoPDB(FileItem Executable, ActionGraph ActionGraph)
        {
            // Make a file item for the source and destination files
            string FullDestPathRoot = Path.Combine(Path.GetDirectoryName(Executable.AbsolutePath), Path.GetFileName(Executable.AbsolutePath) + ".udebugsymbols");
            string FulldSYMPathRoot = Path.Combine(Path.GetDirectoryName(Executable.AbsolutePath), Path.GetFileName(Executable.AbsolutePath) + ".dSYM");
            string PathToDWARF = Path.Combine(FulldSYMPathRoot, "Contents", "Resources", "DWARF", Path.GetFileName(Executable.AbsolutePath));

            FileItem dSYMFile = FileItem.GetItemByPath(FulldSYMPathRoot);

            FileItem DWARFFile = FileItem.GetItemByPath(PathToDWARF);

            FileItem OutputFile = FileItem.GetItemByPath(FullDestPathRoot);

            // Make the compile action
            Action GenDebugAction = ActionGraph.Add(ActionType.GenerateDebugInfo);
            GenDebugAction.WorkingDirectory = GetMacDevEngineRoot() + "/Binaries/Mac/";

            GenDebugAction.CommandPath = "sh";
            GenDebugAction.CommandArguments = string.Format("-c 'rm -rf \"{1}\"; dwarfdump --uuid \"{3}\" | cut -d\" \" -f2; chmod 777 ./DsymExporter; ./DsymExporter -UUID=$(dwarfdump --uuid \"{3}\" | cut -d\" \" -f2) \"{0}\" \"{2}\"'",
                    DWARFFile.AbsolutePath,
                    OutputFile.AbsolutePath,
                    Path.GetDirectoryName(OutputFile.AbsolutePath),
                    dSYMFile.AbsolutePath);
            GenDebugAction.PrerequisiteItems.Add(dSYMFile);
            GenDebugAction.ProducedItems.Add(OutputFile);
            GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
            GenDebugAction.bCanExecuteRemotely = false;

            return OutputFile;
        }

        private static void PackageStub(string BinaryPath, string GameName, string ExeName)
		{
			// create the ipa
			string IPAName = BinaryPath + "/" + ExeName + ".stub";
			// delete the old one
			if (File.Exists(IPAName))
			{
				File.Delete(IPAName);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(IPAName);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// set up the directories
			string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
			string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BinaryPath, GameName);

			// create the file
			using (ZipFile Zip = new ZipFile())
			{
				// add the entire directory
				Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

				// Update permissions to be UNIX-style
				// Modify the file attributes of any added file to unix format
				foreach (ZipEntry E in Zip.Entries)
				{
					const byte FileAttributePlatform_NTFS = 0x0A;
					const byte FileAttributePlatform_UNIX = 0x03;
					const byte FileAttributePlatform_FAT = 0x00;

					const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
					//const int UNIX_FILETYPE_SOCKET = 0xC000;
					//const int UNIX_FILETYPE_SYMLINK = 0xA000;
					//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
					const int UNIX_FILETYPE_DIRECTORY = 0x4000;
					//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
					//const int UNIX_FILETYPE_FIFO = 0x1000;

					const int UNIX_EXEC = 1;
					const int UNIX_WRITE = 2;
					const int UNIX_READ = 4;


					int MyPermissions = UNIX_READ | UNIX_WRITE;
					int OtherPermissions = UNIX_READ;

					int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
					int LowerBits = 0;

					// Try to preserve read-only if it was set
					bool bIsDirectory = E.IsDirectory;

					// Check to see if this 
					bool bIsExecutable = false;
					if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
					{
						bIsExecutable = true;
					}

					if (bIsExecutable)
					{
						// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
						// uncompressed gives a better indicator of IPA size for our distro builds
						E.CompressionLevel = CompressionLevel.None;
					}

					if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
					{
						FileAttributes OldAttributes = E.Attributes;
						//LowerBits = ((int)E.Attributes) & 0xFFFF;

						if ((OldAttributes & FileAttributes.Directory) != 0)
						{
							bIsDirectory = true;
						}

						// Permissions
						if ((OldAttributes & FileAttributes.ReadOnly) != 0)
						{
							MyPermissions &= ~UNIX_WRITE;
							OtherPermissions &= ~UNIX_WRITE;
						}
					}

					if (bIsDirectory || bIsExecutable)
					{
						MyPermissions |= UNIX_EXEC;
						OtherPermissions |= UNIX_EXEC;
					}

					// Re-jigger the external file attributes to UNIX style if they're not already that way
					if (PlatformEncodedBy != FileAttributePlatform_UNIX)
					{
						int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

						NewAttributes |= (MyPermissions << 6);
						NewAttributes |= (OtherPermissions << 3);
						NewAttributes |= (OtherPermissions << 0);

						// Now modify the properties
						E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
					}
				}

				// Save it out
				Zip.Save(IPAName);
			}
		}

		public static void PreBuildSync()
		{
			// Unzip any third party frameworks that are stored as zips
			foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
			{
				FileReference ZipSrcPath = GetFrameworkZip(Framework);
				if(!FileReference.Exists(ZipSrcPath))
				{
					throw new BuildException("Unable to find framework '{0}'", ZipSrcPath);
				}

				DirectoryReference FrameworkDstPath = GetExtractedFrameworkDir(Framework);
				Log.TraceInformation("Unzipping: {0} -> {1}", ZipSrcPath, FrameworkDstPath);

				CleanIntermediateDirectory(FrameworkDstPath.FullName);

				// Assume that there is another directory inside the zip with the same name as the zip
				DirectoryReference ZipDstPath = FrameworkDstPath.ParentDirectory;

				// If we're on the mac, just unzip using the shell
				string ResultsText;
				if(RunExecutableAndWait("unzip", String.Format("-o \"{0}\" -d \"{1}\"", ZipSrcPath, ZipDstPath), out ResultsText) != 0)
				{
					throw new BuildException("Unable to extract {0}:\n{1}", ZipSrcPath, ResultsText);
				}
			}
        }

        public static DirectoryReference GenerateAssetCatalog(FileReference ProjectFile, CppPlatform Platform, ref bool bUserImagesExist)
        {
            string EngineDir = UnrealBuildTool.EngineDirectory.ToString();
            string BuildDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : (string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? UnrealBuildTool.EngineDirectory.ToString() : UnrealBuildTool.GetRemoteIniPath()))) + "/Build/" + (Platform == CppPlatform.IOS ? "IOS" : "TVOS");
            string IntermediateDir = (((ProjectFile != null) ? ProjectFile.Directory.ToString() : UnrealBuildTool.EngineDirectory.ToString())) + "/Intermediate/" + (Platform == CppPlatform.IOS ? "IOS" : "TVOS");

            bUserImagesExist = false;

			string ResourcesDir = Path.Combine(IntermediateDir, "Resources");
            if (Platform == CppPlatform.TVOS)
            {
                string[] Directories = { "Assets.xcassets",
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Back.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Back.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Front.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Front.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Middle.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconLarge.imagestack", "Middle.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Back.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Back.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Front.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Front.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Middle.imagestacklayer"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "AppIconSmall.imagestack", "Middle.imagestacklayer", "Content.imageset"),
                                            Path.Combine("Assets.xcassets", "AppIcon.brandassets", "TopShelf.imageset"),
                                            Path.Combine("Assets.xcassets", "LaunchImage.launchimage"),
                };
                string[] Contents = { "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"assets\" : [\n\t\t{\n\t\t\t\"size\" : \"1280x768\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"AppIconLarge.imagestack\",\n\t\t\t\"role\" : \"primary-app-icon\"\n\t\t},\n\t\t{\n\t\t\t\"size\" : \"400x240\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"AppIconSmall.imagestack\",\n\t\t\t\"role\" : \"primary-app-icon\"\n\t\t},\n\t\t{\n\t\t\t\"size\" : \"1920x720\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"TopShelf.imageset\",\n\t\t\t\"role\" : \"top-shelf-image\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"layers\" : [\n\t\t{\n\t\t\t\"filename\" : \"Front.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Middle.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Back.imagestacklayer\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Back.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Front.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Large_Middle.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"layers\" : [\n\t\t{\n\t\t\t\"filename\" : \"Front.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Middle.imagestacklayer\"\n\t\t},\n\t\t{\n\t\t\t\"filename\" : \"Back.imagestacklayer\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Back.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Front.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Icon_Small_Middle.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"TopShelf.png\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                                    "{\n\t\"images\" : [\n\t\t{\n\t\t\t\"orientation\" : \"landscape\",\n\t\t\t\"idiom\" : \"tv\",\n\t\t\t\"filename\" : \"Launch.png\",\n\t\t\t\"extent\" : \"full-screen\",\n\t\t\t\"minimum-system-version\" : \"9.0\",\n\t\t\t\"scale\" : \"1x\"\n\t\t}\n\t],\n\t\"info\" : {\n\t\t\"version\" : 1,\n\t\t\"author\" : \"xcode\"\n\t}\n}",
                };
                string[] Images = { null,
                                null,
                                null,
                                null,
                                "Icon_Large_Back.png",
                                null,
                                "Icon_Large_Front.png",
                                null,
                                "Icon_Large_Middle.png",
                                null,
                                null,
                                "Icon_Small_Back.png",
                                null,
                                "Icon_Small_Front.png",
                                null,
                                "Icon_Small_Middle.png",
                                "TopShelf.png",
                                "Launch.png"
                };

                // create asset catalog for images
                for (int i = 0; i < Directories.Length; ++i)
                {
                    string Dir = Path.Combine(ResourcesDir, Directories[i]);
                    if (!Directory.Exists(Dir))
                    {
                        Directory.CreateDirectory(Dir);
                    }
                    File.WriteAllText(Path.Combine(Dir, "Contents.json"), Contents[i]);
                    if (Images[i] != null)
                    {
                        // This assumption might not be true, but we need the asset catalog process to fire anyway.
                        bUserImagesExist = true;

                        string Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resource", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "TVOS"))), "Resources", "Graphics", Images[i]);
                        if (File.Exists(Image))
                        {
                            File.Copy(Image, Path.Combine(Dir, Images[i]), true);
                            FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[i]));
                            DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
                        }
                    }
                }
            }
            else
            {
                // copy the template asset catalog to the appropriate directory
                string Dir = Path.Combine(ResourcesDir, "Assets.xcassets");
                if (!Directory.Exists(Dir))
                {
                    Directory.CreateDirectory(Dir);
                }
                // create the directories
                foreach (string directory in Directory.EnumerateDirectories(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
                {
                    Dir = directory.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
                    if (!Directory.Exists(Dir))
                    {
                        Directory.CreateDirectory(Dir);
                    }
                }
                // copy the default files
                foreach (string file in Directory.EnumerateFiles(Path.Combine(EngineDir, "Build", "IOS", "Resources", "Assets.xcassets"), "*", SearchOption.AllDirectories))
                {
                    Dir = file.Replace(Path.Combine(EngineDir, "Build", "IOS"), IntermediateDir);
                    File.Copy(file, Dir, true);
                    FileInfo DestFileInfo = new FileInfo(Dir);
                    DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
                }
                // copy the icons from the game directory if it has any
                string[][] Images = {
                    new string []{ "IPhoneIcon20@2x.png", "Icon20@2x.png", "Icon40.png" },
                    new string []{ "IPhoneIcon20@3x.png", "Icon20@3x.png", "Icon60.png" },
                    new string []{ "IPhoneIcon29@2x.png", "Icon29@2x.png", "Icon58.png" },
                    new string []{ "IPhoneIcon29@3x.png", "Icon29@3x.png", "Icon87.png" },
                    new string []{ "IPhoneIcon40@2x.png", "Icon40@2x.png", "Icon80.png" },
                    new string []{ "IPhoneIcon40@3x.png", "Icon40@3x.png", "Icon60@2x.png", "Icon120.png" },
                    new string []{ "IPhoneIcon60@2x.png", "Icon60@2x.png", "Icon40@3x.png", "Icon120.png" },
                    new string []{ "IPhoneIcon60@3x.png", "Icon60@3x.png", "Icon180.png" },
                    new string []{ "IPadIcon20.png", "Icon20.png" },
                    new string []{ "IPadIcon20@2x.png", "Icon20@2x.png", "Icon40.png" },
					new string []{ "IPadIcon29.png", "Icon29.png" },
                    new string []{ "IPadIcon29@2x.png", "Icon29@2x.png", "Icon58.png" },
                    new string []{ "IPadIcon40.png", "Icon40.png", "Icon20@2x.png" },
					new string []{ "IPadIcon40@2x.png", "Icon80.png", "Icon40@2x.png" },
					new string []{ "IPadIcon76.png", "Icon76.png" },
                    new string []{ "IPadIcon76@2x.png", "Icon76@2x.png", "Icon152.png" },
                    new string []{ "IPadIcon83.5@2x.png", "Icon83.5@2x.png", "Icon167.png" },
                    new string []{ "Icon1024.png", "Icon1024.png" },
                };
                Dir = Path.Combine(IntermediateDir, "Resources", "Assets.xcassets", "AppIcon.appiconset");

                string BuildResourcesGraphicsDir = Path.Combine(BuildDir, "Resources", "Graphics");
                for (int Index = 0; Index < Images.Length; ++Index)
                {
                    string Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][1]);
                    if (!File.Exists(Image) && Images[Index].Length > 2)
                    {
                        Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][2]);
                    }
                    if (!File.Exists(Image) && Images[Index].Length > 3)
                    {
                        Image = Path.Combine((Directory.Exists(Path.Combine(BuildDir, "Resources", "Graphics")) ? (BuildDir) : (Path.Combine(EngineDir, "Build", "IOS"))), "Resources", "Graphics", Images[Index][3]);
                    }
                    if (File.Exists(Image))
                    {
                        bUserImagesExist |= Image.StartsWith(BuildResourcesGraphicsDir);

                        File.Copy(Image, Path.Combine(Dir, Images[Index][0]), true);
                        FileInfo DestFileInfo = new FileInfo(Path.Combine(Dir, Images[Index][0]));
                        DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
                    }
                }
            }
			return new DirectoryReference(ResourcesDir);
        }

        public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, ActionGraph ActionGraph)
        {
            ICollection<FileItem> OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment, ActionGraph);

            if (BinaryLinkEnvironment.bIsBuildingLibrary)
            {
                return OutputFiles;
            }

            // For IOS/tvOS, generate the dSYM file if the config file is set to do so
			if (ProjectSettings.bGeneratedSYMFile == true || ProjectSettings.bGeneratedSYMBundle == true || BinaryLinkEnvironment.bUsePDBFiles == true)
            {
                OutputFiles.Add(GenerateDebugInfo(Executable, ActionGraph));
                if (ProjectSettings.bGenerateCrashReportSymbols || bUseMallocProfiler)
                {
                    OutputFiles.Add(GeneratePseudoPDB(Executable, ActionGraph));
                }
            }

			if(ShouldCompileAssetCatalog())
			{
				// generate the asset catalog
				bool bUserImagesExist = false;
				DirectoryReference ResourcesDir = GenerateAssetCatalog(ProjectFile, BinaryLinkEnvironment.Platform, ref bUserImagesExist);

				// Get the output location for the asset catalog
				FileItem AssetCatalogFile = FileItem.GetItemByFileReference(GetAssetCatalogFile(CppPlatform, Executable.Location));

				// Make the compile action
				Action CompileAssetAction = ActionGraph.Add(ActionType.CreateAppBundle);
				CompileAssetAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAssetAction.CommandPath = "/usr/bin/xcrun";
				CompileAssetAction.CommandArguments = GetAssetCatalogArgs(CppPlatform, ResourcesDir.FullName, Path.GetDirectoryName(AssetCatalogFile.AbsolutePath));
				CompileAssetAction.PrerequisiteItems.Add(Executable);
				CompileAssetAction.ProducedItems.Add(AssetCatalogFile);
				CompileAssetAction.DeleteItems.Add(AssetCatalogFile);
				CompileAssetAction.StatusDescription = CompileAssetAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
				CompileAssetAction.bCanExecuteRemotely = false;

				// Add it to the output files so it's always built
				OutputFiles.Add(AssetCatalogFile);
			}

			return OutputFiles;
        }

		public bool ShouldCompileAssetCatalog()
		{
            return CppPlatform == CppPlatform.TVOS || (CppPlatform == CppPlatform.IOS && Settings.Value.IOSSDKVersionFloat >= 11.0f);
		}

		public static string GetCodesignPlatformName(UnrealTargetPlatform Platform)
		{
			switch(Platform)
			{
				case UnrealTargetPlatform.TVOS:
					return "appletvos";
				case UnrealTargetPlatform.IOS:
					return "iphoneos";
				default:
					throw new BuildException("Invalid platform for GetCodesignPlatformName()");
			}
		}

		class ProcessOutput
		{
			/// <summary>
			/// Substrings that indicate a line contains an error
			/// </summary>
			protected static readonly string[] ErrorMessageTokens =
			{
				"ERROR ",
				"** BUILD FAILED **",
				"[BEROR]",
				"IPP ERROR",
				"System.Net.Sockets.SocketException"
			};

			/// <summary>
			/// Helper function to sync source files to and from the local system and a remote Mac
			/// </summary>
			//This chunk looks to be required to pipe output to VS giving information on the status of a remote build.
			public bool OutputReceivedDataEventHandlerEncounteredError = false;
			public string OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
			public void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
			{
				if ((Line != null) && (Line.Data != null))
				{
					Log.TraceInformation(Line.Data);

					foreach (string ErrorToken in ErrorMessageTokens)
					{
						if (Line.Data.Contains(ErrorToken))
						{
							OutputReceivedDataEventHandlerEncounteredError = true;
							OutputReceivedDataEventHandlerEncounteredErrorMessage += Line.Data;
							break;
						}
					}
				}
			}
		}

		private static void GenerateCrashlyticsData(string ExecutableDirectory, string ExecutableName, string ProjectDir, string ProjectName)
        {
			Log.TraceInformation("Generating and uploading Crashlytics Data");
            string FabricPath = UnrealBuildTool.EngineDirectory + "/Intermediate/UnzippedFrameworks/ThirdPartyFrameworks/Fabric.embeddedframework";
            if (Directory.Exists(FabricPath) && Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
            {
				string PlistFile = ProjectDir + "/Intermediate/IOS/" + ProjectName + "-Info.plist";
                Process FabricProcess = new Process();
                FabricProcess.StartInfo.WorkingDirectory = ExecutableDirectory;
                FabricProcess.StartInfo.FileName = "/bin/sh";
				FabricProcess.StartInfo.Arguments = string.Format("-c 'chmod 777 \"{0}/Fabric.framework/upload-symbols\"; \"{0}/Fabric.framework/upload-symbols\" -a 7a4cebd0324af21696e5e321802c5e26ba541cad -p ios {1}'",
                    FabricPath,
					ExecutableDirectory + "/" + ExecutableName + ".dSYM.zip");

				ProcessOutput Output = new ProcessOutput();

                FabricProcess.OutputDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);
                FabricProcess.ErrorDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);

                Utils.RunLocalProcess(FabricProcess);
                if (Output.OutputReceivedDataEventHandlerEncounteredError)
                {
                    throw new Exception(Output.OutputReceivedDataEventHandlerEncounteredErrorMessage);
                }
            }
        }

        public static void PostBuildSync(UEBuildTarget Target)
		{
			if (Target.Rules == null)
			{
				Log.TraceWarning("Unable to PostBuildSync, Target has no Rules object");
				return;
			}
			if(Target.Rules.bDisableLinking)
			{
				return;
			}

			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);

			string AppName = Target.TargetType == TargetType.Game ? Target.TargetName : Target.AppName;

			string RemoteShadowDirectoryMac = Path.GetDirectoryName(Target.OutputPath.FullName);
			string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, AppName);

			// strip the debug info from the executable if needed
			if (Target.Rules.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
			{
				Process StripProcess = new Process();
				StripProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
				StripProcess.StartInfo.FileName = new IOSToolChainSettings().ToolchainDir + "strip";
				StripProcess.StartInfo.Arguments = "\"" + Target.OutputPath + "\"";

				ProcessOutput Output = new ProcessOutput();
				StripProcess.OutputDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);
				StripProcess.ErrorDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);

				Output.OutputReceivedDataEventHandlerEncounteredError = false;
				Output.OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
				Utils.RunLocalProcess(StripProcess);
				if (Output.OutputReceivedDataEventHandlerEncounteredError)
				{
					throw new Exception(Output.OutputReceivedDataEventHandlerEncounteredErrorMessage);
				}
			}

            // ensure the plist, entitlements, and provision files are properly copied
            UEDeployIOS DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS() : new UEDeployTVOS());
            DeployHandler.PrepTargetForDeployment(new UEBuildDeployTarget(Target));

			// copy the executable
			if (!File.Exists(FinalRemoteExecutablePath))
			{
				Directory.CreateDirectory(String.Format("{0}/Payload/{1}.app", RemoteShadowDirectoryMac, AppName));
			}
			File.Copy(Target.OutputPath.FullName, FinalRemoteExecutablePath, true);

			if (!Target.Rules.IOSPlatform.bSkipCrashlytics)
			{
				GenerateCrashlyticsData(RemoteShadowDirectoryMac, Path.GetFileName(Target.OutputPath.FullName), Target.ProjectDirectory.FullName, AppName);
			}

			if (Target.Rules.bCreateStubIPA)
			{
				// generate the dummy project so signing works
				DirectoryReference XcodeWorkspaceDir;
				if (AppName == "UE4Game" || AppName == "UE4Client" || Target.ProjectFile == null || Target.ProjectFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
				{
					UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-platforms=" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk" });
					XcodeWorkspaceDir = DirectoryReference.Combine(UnrealBuildTool.RootDirectory, String.Format("UE4_{0}.xcworkspace", (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS")));
				}
				else
				{
					UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(Target.ProjectFile), new string[] { "-platforms=" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"), "-NoIntellIsense", (Target.Platform == UnrealTargetPlatform.IOS ? "-iosdeployonly" : "-tvosdeployonly"), "-ignorejunk", String.Format("-project={0}", Target.ProjectFile), "-game" });
					XcodeWorkspaceDir = DirectoryReference.Combine(Target.ProjectDirectory, String.Format("{0}_{1}.xcworkspace", AppName, (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS")));
				}

				// Make sure it exists
				if (!DirectoryReference.Exists(XcodeWorkspaceDir))
				{
					throw new BuildException("Unable to create stub IPA; Xcode workspace not found at {0}", XcodeWorkspaceDir);
				}

				// ensure the plist, entitlements, and provision files are properly copied
				DeployHandler = (Target.Platform == UnrealTargetPlatform.IOS ? new UEDeployIOS() : new UEDeployTVOS());
				DeployHandler.PrepTargetForDeployment(new UEBuildDeployTarget(Target));

				FileReference SignProjectScript = FileReference.Combine(Target.ProjectIntermediateDirectory, "SignProject.sh");
				using(StreamWriter Writer = new StreamWriter(SignProjectScript.FullName))
				{
					// Boilerplate
					Writer.WriteLine("#!/bin/sh");
					Writer.WriteLine("set -e");
					Writer.WriteLine("set -x");

					// Copy the mobile provision into the system store
					if(Target.Rules.IOSPlatform.ImportProvision != null)
					{
						Writer.WriteLine("cp -f {0} ~/Library/MobileDevice/Provisioning\\ Profiles/", Utils.EscapeShellArgument(Target.Rules.IOSPlatform.ImportProvision));
					}

					// Path to the temporary keychain. When -ImportCertificate is specified, we will temporarily add this to the list of keychains to search, and remove it later.
					FileReference TempKeychain = null;

					// Get the signing certificate to use
					string SigningCertificate;
					if(Target.Rules.IOSPlatform.ImportCertificate == null)
					{
						// Take it from the standard settings
						IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(Target.ProjectFile);
						SigningCertificate = ProvisioningData.SigningCertificate;

						// Set the identity on the command line
						if(!ProjectSettings.bAutomaticSigning)
						{
							Writer.WriteLine("CODE_SIGN_IDENTITY='{0}'", String.IsNullOrEmpty(SigningCertificate)? "IPhoneDeveloper" : SigningCertificate);
						}
					}
					else
					{
						// Read the name from the certificate
						X509Certificate2 Certificate;
						try
						{
							Certificate = new X509Certificate2(Target.Rules.IOSPlatform.ImportCertificate, Target.Rules.IOSPlatform.ImportCertificatePassword ?? "");
						}
						catch(Exception Ex)
						{
							throw new BuildException(Ex, "Unable to read certificate '{0}': {1}", Target.Rules.IOSPlatform.ImportCertificate, Ex.Message);
						}
						SigningCertificate = Certificate.GetNameInfo(X509NameType.SimpleName, false);

						// Set the path to the temporary keychain
						TempKeychain = FileReference.Combine(Target.ProjectIntermediateDirectory, "TempKeychain.keychain");//(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile), "Library", "Keychains/UE4TempKeychain.keychain";

						// Install a certificate given on the command line to a temporary keychain
						Writer.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
						Writer.WriteLine("security create-keychain -p \"A\" \"{0}\"", TempKeychain);
						Writer.WriteLine("security list-keychains -s \"{0}\"", TempKeychain);
						Writer.WriteLine("security list-keychains");
						Writer.WriteLine("security set-keychain-settings -t 3600 -l  \"{0}\"", TempKeychain);
						Writer.WriteLine("security -v unlock-keychain -p \"A\" \"{0}\"", TempKeychain);
						Writer.WriteLine("security import {0} -P {1} -k \"{2}\" -T /usr/bin/codesign -T /usr/bin/security -t agg", Utils.EscapeShellArgument(Target.Rules.IOSPlatform.ImportCertificate), Utils.EscapeShellArgument(Target.Rules.IOSPlatform.ImportCertificatePassword), TempKeychain);
						Writer.WriteLine("security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k \"A\" -D '{0}' -t private {1}", SigningCertificate, TempKeychain);

						// Set parameters to make sure it uses the correct identity and keychain
						Writer.WriteLine("CERT_IDENTITY='{0}'", SigningCertificate);
						Writer.WriteLine("CODE_SIGN_IDENTITY='{0}'", SigningCertificate);
						Writer.WriteLine("CODE_SIGN_KEYCHAIN='{0}'", TempKeychain);
					}

					FileReference MobileProvisionFile;
					string MobileProvisionUUID;
					string TeamUUID;
					if(Target.Rules.IOSPlatform.ImportProvision == null)
					{
						IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProvisioningData(ProjectSettings);
						MobileProvisionFile = ProvisioningData.MobileProvisionFile;
						MobileProvisionUUID = ProvisioningData.MobileProvisionUUID;
						TeamUUID = ProvisioningData.TeamUUID;
					}
					else
					{
						MobileProvisionFile = new FileReference(Target.Rules.IOSPlatform.ImportProvision);

						MobileProvisionContents MobileProvision = MobileProvisionContents.Read(MobileProvisionFile);
						MobileProvisionUUID = MobileProvision.GetUniqueId();
						MobileProvision.TryGetTeamUniqueId(out TeamUUID);
					}

					string ConfigName = Target.Configuration.ToString();
					if (Target.Rules.Type != TargetType.Game && Target.Rules.Type != TargetType.Program)
					{
						ConfigName += " " + Target.Rules.Type.ToString();
					}

					string SchemeName;
					if (AppName == "UE4Game" || AppName == "UE4Client")
					{
						SchemeName = "UE4";
					}
					else
					{
						SchemeName = AppName;
					}

					// code sign the project
					Console.WriteLine("Provisioning: {0}, {1}, {2}", MobileProvisionFile, MobileProvisionFile.GetFileName(), MobileProvisionUUID);
					string CmdLine = new IOSToolChainSettings().XcodeDeveloperDir + "usr/bin/xcodebuild" +
									" -workspace \"" + XcodeWorkspaceDir + "\"" +
									" -configuration \"" + ConfigName + "\"" +
									" -scheme '" + SchemeName + "'" +
									" -sdk " + GetCodesignPlatformName(Target.Platform) +
									" -destination generic/platform=" + (Target.Platform == UnrealTargetPlatform.IOS ? "iOS" : "tvOS") + 
									(!string.IsNullOrEmpty(TeamUUID) ? " DEVELOPMENT_TEAM=" + TeamUUID : "");
					CmdLine += String.Format(" CODE_SIGN_IDENTITY='{0}'", SigningCertificate);
					if (!ProjectSettings.bAutomaticSigning)
					{
						CmdLine += (!string.IsNullOrEmpty(MobileProvisionUUID) ? (" PROVISIONING_PROFILE_SPECIFIER=" + MobileProvisionUUID) : "");
					}
					Writer.WriteLine("/usr/bin/xcrun {0}", CmdLine);

					// Remove the temporary keychain from the search list
					if(TempKeychain != null)
					{
						Writer.WriteLine("security delete-keychain \"{0}\" || true", TempKeychain);
					}
				}

				Log.TraceInformation("Executing {0}", SignProjectScript);

				Process SignProcess = new Process();
				SignProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
				SignProcess.StartInfo.FileName = "/bin/sh";
				SignProcess.StartInfo.Arguments = SignProjectScript.FullName;

				ProcessOutput Output = new ProcessOutput();

				SignProcess.OutputDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);
				SignProcess.ErrorDataReceived += new DataReceivedEventHandler(Output.OutputReceivedDataEventHandler);

				Output.OutputReceivedDataEventHandlerEncounteredError = false;
				Output.OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
				Utils.RunLocalProcess(SignProcess);

				// delete the temp project
				DirectoryReference.Delete(XcodeWorkspaceDir, true);

				if (Output.OutputReceivedDataEventHandlerEncounteredError)
				{
					throw new Exception(Output.OutputReceivedDataEventHandlerEncounteredErrorMessage);
				}

				// Package the stub
				PackageStub(RemoteShadowDirectoryMac, AppName, Target.OutputPath.GetFileNameWithoutExtension());
			}

			{
				// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
				String LocalFrameworkAssets = Path.GetFullPath(Target.ProjectDirectory + "/Intermediate/" + (Target.Platform == UnrealTargetPlatform.IOS ? "IOS" : "TVOS") + "/FrameworkAssets");

				// Clean the local dest directory if it exists
				CleanIntermediateDirectory(LocalFrameworkAssets);

				foreach (UEBuildFramework Framework in RememberedAdditionalFrameworks)
				{
					if (Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "")
					{
						continue;		// Only care if we need to copy bundle assets
					}

					string UnpackedZipPath = GetExtractedFrameworkDir(Framework).FullName;

					// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
					string LocalSource = UnpackedZipPath + "/" + Framework.CopyBundledAssets;
					string BundleName = Framework.CopyBundledAssets.Substring(Framework.CopyBundledAssets.LastIndexOf('/') + 1);
					string LocalDest = LocalFrameworkAssets + "/" + BundleName;

					Log.TraceInformation("Copying bundled asset... LocalSource: {0}, LocalDest: {1}", LocalSource, LocalDest);

					string ResultsText;
                    RunExecutableAndWait("cp", String.Format("-R -L \"{0}\" \"{1}\"", LocalSource, LocalDest), out ResultsText);
                }
            }
		}

		public static int RunExecutableAndWait(string ExeName, string ArgumentList, out string StdOutResults)
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo(ExeName, ArgumentList);
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start(PSI);

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			StripSymbolsWithXcode(SourceFile, TargetFile, Settings.Value.ToolchainDir);
		}
	};
}
