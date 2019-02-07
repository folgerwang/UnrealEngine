// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class HTML5ToolChain : UEToolChain
	{
		// ini configurations
		static bool enableSIMD = false;
		static bool enableMultithreading = true;
		static bool useLLVMwasmBackend = false; // experimental
		public static string libExt = ".bc";  // experimental - LLVMWasmBackend
		static bool bEnableTracing = false; // Debug option
		static bool bMultithreading_UseOffscreenCanvas = true; // else use Offscreen_Framebuffer

		// verbose feedback
		delegate void VerbosePrint(CppConfiguration Configuration, bool bOptimizeForSize);	// proto
		static VerbosePrint PrintOnce = new VerbosePrint(PrintOnceOn);						// fn ptr
		static void PrintOnceOff(CppConfiguration Configuration, bool bOptimizeForSize) {}	// noop
		static void PrintOnceOn(CppConfiguration Configuration, bool bOptimizeForSize)
		{
			if (Configuration == CppConfiguration.Debug)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O0 faster compile time");
			else if (bOptimizeForSize)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -Oz favor size over speed");
			else if (Configuration == CppConfiguration.Development)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O2 aggressive size and speed optimization");
			else if (Configuration == CppConfiguration.Shipping)
				Log.TraceInformation("HTML5ToolChain: " + Configuration + " -O3 favor speed over size");
			PrintOnce = new VerbosePrint(PrintOnceOff); // clear
		}

		public HTML5ToolChain(FileReference InProjectFile)
			: base(CppPlatform.HTML5)
		{
			if (!HTML5SDKInfo.IsSDKInstalled())
			{
				throw new BuildException("HTML5 SDK is not installed; cannot use toolchain.");
			}

			// ini configs
			// - normal ConfigCache w/ UnrealBuildTool.ProjectFile takes all game config ini files
			//   (including project config ini files)
			// - but, during packaging, if -remoteini is used -- need to use UnrealBuildTool.GetRemoteIniPath()
			//   (note: ConfigCache can take null ProjectFile)
			string EngineIniPath = UnrealBuildTool.GetRemoteIniPath();
			DirectoryReference ProjectDir = !String.IsNullOrEmpty(EngineIniPath) ? new DirectoryReference(EngineIniPath)
												: DirectoryReference.FromFile(InProjectFile);
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDir, UnrealTargetPlatform.HTML5);

//			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableSIMD", out enableSIMD);
			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableMultithreading", out enableMultithreading);
			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "OffscreenCanvas", out bMultithreading_UseOffscreenCanvas);
			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "LLVMWasmBackend", out useLLVMwasmBackend);
			Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableTracing", out bEnableTracing);

			if (useLLVMwasmBackend)
			{
				libExt = ".a";  // experimental - LLVMWasmBackend
			}

			// TODO: remove this "fix" when emscripten supports WASM with SIMD
				enableSIMD = false;

			Log.TraceInformation("HTML5ToolChain: EnableSIMD = "         + enableSIMD           );
			Log.TraceInformation("HTML5ToolChain: EnableMultithreading " + enableMultithreading );
			Log.TraceInformation("HTML5ToolChain: OffscreenCanvas "      + bMultithreading_UseOffscreenCanvas );
			Log.TraceInformation("HTML5ToolChain: LLVMWasmBackend "      + useLLVMwasmBackend   );
			Log.TraceInformation("HTML5ToolChain: EnableTracing = "      + bEnableTracing       );

			PrintOnce = new VerbosePrint(PrintOnceOn); // reset

			Log.TraceInformation("Setting Emscripten SDK: located in " + HTML5SDKInfo.EMSCRIPTEN_ROOT);
			string TempDir = HTML5SDKInfo.SetupEmscriptenTemp();
			HTML5SDKInfo.SetUpEmscriptenConfigFile();

			if (Environment.GetEnvironmentVariable("EMSDK") == null) // If EMSDK is present, Emscripten is already configured by the developer
			{
				// If not using preset emsdk, configure our generated .emscripten config, instead of autogenerating one in the user's home directory.
				Environment.SetEnvironmentVariable("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN);
				Environment.SetEnvironmentVariable("EM_CACHE", HTML5SDKInfo.EMSCRIPTEN_CACHE);
				Environment.SetEnvironmentVariable("EMCC_TEMP_DIR", TempDir);
			}

			Log.TraceInformation("*** Emscripten Config File: " + Environment.GetEnvironmentVariable("EM_CONFIG"));
		}

		string GetSharedArguments_Global(CppConfiguration Configuration, bool bOptimizeForSize, string Architecture, bool bEnableShadowVariableWarnings, bool bShadowVariableWarningsAsErrors, bool bEnableUndefinedIdentifierWarnings, bool bUndefinedIdentifierWarningsAsErrors, bool bUseInlining)
		{
			string Result = " ";
//			string Result = " -Werror";

			Result += " -fdiagnostics-format=msvc";
			Result += " -fno-exceptions";

			Result += " -Wdelete-non-virtual-dtor";
			Result += " -Wno-switch"; // many unhandled cases
			Result += " -Wno-tautological-constant-out-of-range-compare"; // comparisons from TCHAR being a char
			Result += " -Wno-tautological-compare"; // comparison of unsigned expression < 0 is always false" (constant comparisons, which are possible with template arguments)
			Result += " -Wno-tautological-undefined-compare"; // pointer cannot be null in well-defined C++ code; comparison may be assumed to always evaluate
			Result += " -Wno-inconsistent-missing-override"; // as of 1.35.0, overriding a member function but not marked as 'override' triggers warnings
			Result += " -Wno-undefined-var-template"; // 1.36.11
			Result += " -Wno-invalid-offsetof"; // using offsetof on non-POD types
			Result += " -Wno-gnu-string-literal-operator-template"; // allow static FNames

			if (bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" ;//+ (bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow");
			}

			if (bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" ;//+ (bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// --------------------------------------------------------------------------------

			if (Configuration == CppConfiguration.Debug)
			{															// WARNING: UEBuildTarget.cs :: GetCppConfiguration()
				Result += " -O0"; // faster compile time				//          DebugGame is forced to Development
			}															// i.e. this will never get hit...

			else if (bOptimizeForSize)
			{															// Engine/Source/Programs/UnrealBuildTool/HTML5/UEBuildHTML5.cs
				Result += " -Oz"; // favor size over speed				// bCompileForSize=true; // set false, to build -O2 or -O3
			}															// SOURCE BUILD ONLY

			else if (Configuration == CppConfiguration.Development)
			{
				Result += " -O2"; // aggressive size and speed optimization
			}

			else if (Configuration == CppConfiguration.Shipping)
			{
				Result += " -O3"; // favor speed over size
			}

			if (!bUseInlining)
			{
				Result += " -fno-inline-functions";
			}

			PrintOnce(Configuration, bOptimizeForSize);

			// --------------------------------------------------------------------------------

			// JavaScript option overrides (see src/settings.js)
			if (enableSIMD)
			{
//				Result += " -msse2 -s SIMD=1";
			}

			if (enableMultithreading)
			{
				Result += " -msse2 -s USE_PTHREADS=1";
				Result += " -DEXPERIMENTAL_OPENGL_RHITHREAD=" + (bMultithreading_UseOffscreenCanvas ? "1" : "0");

				// NOTE: use "emscripten native" video, keyboard, mouse
			}
			else
			{
				// SDL2 is not supported for multi-threading WASM builds
				// WARNING: SDL2 may be removed in a future UE4 release
				// can comment out to use "emscripten native" single threaded
	//			Result += " -DHTML5_USE_SDL2";
			}

			if (useLLVMwasmBackend)  // experimental - LLVMWasmBackend
			{
				Result += " -s WASM_OBJECT_FILES=1";
				Environment.SetEnvironmentVariable("EMCC_WASM_BACKEND", "1");
			}

			// --------------------------------------------------------------------------------

			// emscripten ports
// WARNING: seems emscripten ports cannot be currently used
// there might be UE4 changes needed that are found in Engine/Source/ThirdParty/...

//			Result += " -s USE_ZLIB=1";
//			Result += " -s USE_LIBPNG=1";
//			Result += " -s USE_VORBIS=1";
//			Result += " -s USE_OGG=1";
//			Result += " -s USE_FREETYPE=1";	// TAG = 'release_1'
//			Result += " -s USE_HARFBUZZ=1";	// TAG = '1.2.4				note: path is https://github.com/harfbuzz/harfbuzz/archive/1.2.4.zip
//			Result += " -s USE_ICU=1";		// TAG = 'release-53-1'

			// SDL_Audio needs to be linked in [no matter if -DHTML5_USE_SDL2 is used or not]
// TODO: remove AudioMixerSDL from Engine/Source/Runtime/Launch/Launch.Build.cs and replace with emscripten native functions
//			Result += " -s USE_SDL=2";

			// --------------------------------------------------------------------------------

			// Expect that Emscripten SDK has been properly set up ahead in time (with emsdk and prebundled toolchains this is always the case)
			// This speeds up builds a tiny bit.
			Environment.SetEnvironmentVariable("EMCC_SKIP_SANITY_CHECK", "1");

			// THESE ARE TEST/DEBUGGING
	//		Environment.SetEnvironmentVariable("EMCC_DEBUG", "1");
//			Environment.SetEnvironmentVariable("EMCC_CORES", "8");
//			Environment.SetEnvironmentVariable("EMCC_OPTIMIZE_NORMALLY", "1");

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				// Packaging on Linux needs this - or else system clang will be attempted to be picked up instead of UE4's included emsdk
				Environment.SetEnvironmentVariable(HTML5SDKInfo.PLATFORM_USER_HOME, HTML5SDKInfo.HTML5Intermediatory);
			}
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32)
			{
				// Packaging on Window needs this - zap any existing HOME environment variables to prevent any accidental pick ups
				Environment.SetEnvironmentVariable("HOME", "");
			}
			return Result;
		}

		string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
			string Result = GetSharedArguments_Global(CompileEnvironment.Configuration, CompileEnvironment.bOptimizeForSize, CompileEnvironment.Architecture, CompileEnvironment.bEnableShadowVariableWarnings, CompileEnvironment.bShadowVariableWarningsAsErrors, CompileEnvironment.bEnableUndefinedIdentifierWarnings, CompileEnvironment.bUndefinedIdentifierWarningsAsErrors, CompileEnvironment.bUseInlining);

			return Result;
		}

		static string GetCLArguments_CPP(CppCompileEnvironment CompileEnvironment)
		{
			string Result = " -std=c++14";

			return Result;
		}

		static string GetCLArguments_C(string Architecture)
		{
			string Result = "";

			return Result;
		}

		string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = GetSharedArguments_Global(LinkEnvironment.Configuration, LinkEnvironment.bOptimizeForSize, LinkEnvironment.Architecture, false, false, false, false, false);

			/* N.B. When editing link flags in this function, UnrealBuildTool does not seem to automatically pick them up and do an incremental
			 *	relink only of UE4Game.js (at least when building blueprints projects). Therefore after editing, delete the old build
			 *	outputs to force UE4 to relink:
			 *
			 *    > rm Engine/Binaries/HTML5/UE4Game.js*
			 */

			// enable verbose mode
			Result += " -v";


			// --------------------------------------------------
			// do we want debug info?

			if (LinkEnvironment.Configuration == CppConfiguration.Debug || LinkEnvironment.bCreateDebugInfo)
			{
				// As a lightweight alternative, just retain function names in output.
				Result += " --profiling-funcs";

				// dump headers: http://stackoverflow.com/questions/42308/tool-to-track-include-dependencies
//				Result += " -H";
			}
			else if (LinkEnvironment.Configuration == CppConfiguration.Development)
			{
				// Development builds always have their function names intact.
				Result += " --profiling-funcs";
			}

			// Emit a .symbols map file of the minified function names. (on -g2 builds this has no effect)
			Result += " --emit-symbol-map";

//			if (LinkEnvironment.Configuration != CppConfiguration.Debug)
//			{
//				if (LinkEnvironment.bOptimizeForSize) Result += " -s OUTLINING_LIMIT=40000";
//				else Result += " -s OUTLINING_LIMIT=110000";
//			}

			if (LinkEnvironment.Configuration == CppConfiguration.Debug || LinkEnvironment.Configuration == CppConfiguration.Development)
			{
				// check for alignment/etc checking
//				Result += " -s SAFE_HEAP=1";
				//Result += " -s CHECK_HEAP_ALIGN=1";
				//Result += " -s SAFE_DYNCALLS=1";

				// enable assertions in non-Shipping/Release builds
				Result += " -s ASSERTIONS=1";
				Result += " -s GL_ASSERTIONS=1";
//				Result += " -s ASSERTIONS=2";
//				Result += " -s GL_ASSERTIONS=2";
				Result += " -s GL_DEBUG=1";

				// In non-shipping builds, don't run ctol evaller, it can take a bit of extra time.
				Result += " -s EVAL_CTORS=0";

				// --------------------------------------------------

				// UE4Game.js.symbols is redundant if -g2 is passed (i.e. --emit-symbol-map gets ignored)
				Result += " -g1";

//				// add source map loading to code
//				string source_map = Path.Combine(HTML5SDKInfo.EMSCRIPTEN_ROOT, "src", "emscripten-source-map.min.js");
//				source_map = source_map.Replace("\\", "/").Replace(" ","\\ "); // use "unix path" and escape spaces
//				Result += " --pre-js " + source_map;

				// link in libcxxabi demangling
				Result += " -s DEMANGLE_SUPPORT=1";

				// --------------------------------------------------

				if (enableMultithreading)
				{
					Result += " --threadprofiler";
				}
			}

			if (bEnableTracing)
			{
				Result += " --tracing";
			}



			// --------------------------------------------------
			// emscripten memory

			if (enableMultithreading)
			{
				Result += " -s ALLOW_MEMORY_GROWTH=0";
				Result += " -s TOTAL_MEMORY=512MB";

// NOTE: browsers needs to temporarly have some flags set:
//  https://github.com/kripken/emscripten/wiki/Pthreads-with-WebAssembly
//  https://kripken.github.io/emscripten-site/docs/porting/pthreads.html
				Result += " -s PTHREAD_POOL_SIZE=4 -s PTHREAD_HINT_NUM_CORES=2";
			}
			else
			{
				Result += " -s ALLOW_MEMORY_GROWTH=1";
			}


			// --------------------------------------------------
			// WebGL

			// NOTE: UE-51094 UE-51267 -- always USE_WEBGL2, webgl1 only feature can be switched on the fly via url paramater "?webgl1"
//			if (targetWebGL2)
			{
				Result += " -s USE_WEBGL2=1";
				if ( enableMultithreading )
				{
					if ( bMultithreading_UseOffscreenCanvas )
					{
						Result += " -s OFFSCREENCANVAS_SUPPORT=1";
					}
					else
					{
						Result += " -s OFFSCREEN_FRAMEBUFFER=1";
					}
					Result += " -s PROXY_TO_PTHREAD=1";
				}

				// Also enable WebGL 1 emulation in WebGL 2 contexts. This adds backwards compatibility related features to WebGL 2,
				// such as:
				//  - keep supporting GL_EXT_shader_texture_lod extension in GLSLES 1.00 shaders
				//  - support for WebGL1 unsized internal texture formats
				//  - mask the GL_HALF_FLOAT_OES != GL_HALF_FLOAT mixup
				Result += " -s WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION=1";
//				Result += " -s FULL_ES3=1";
			}
//			else
//			{
//				Result += " -s FULL_ES2=1";
//			}

			// The HTML page template precreates the WebGL context, so instruct the runtime to hook into that if available.
			Result += " -s GL_PREINITIALIZED_CONTEXT=1";


			// --------------------------------------------------
			// wasm

			Result += " -s BINARYEN_TRAP_MODE=\\'clamp\\'";
			Result += " -s WASM=1";


			// --------------------------------------------------
			// house keeping

			// export console command handler. Export main func too because default exports ( e.g Main ) are overridden if we use custom exported functions.
			Result += " -s EXPORTED_FUNCTIONS=\"['_main', '_on_fatal']\"";
			Result += " -s EXTRA_EXPORTED_RUNTIME_METHODS=\"['Pointer_stringify', 'writeAsciiToMemory', 'stackTrace']\"";

			Result += " -s DISABLE_EXCEPTION_CATCHING=1";
			Result += " -s ERROR_ON_UNDEFINED_SYMBOLS=1";
			Result += " -s NO_EXIT_RUNTIME=1";


			// --------------------------------------------------
			// emscripten filesystem

			Result += " -s CASE_INSENSITIVE_FS=1";
			Result += " -s FORCE_FILESYSTEM=1";

//			if (enableMultithreading)
//			{
// was recommended to NOT use either of these...
//				Result += " -s ASYNCIFY=1"; // alllow BLOCKING calls (i.e. sleep)
//				Result += " -s EMTERPRETIFY_ASYNC=1"; // alllow BLOCKING calls (i.e. sleep)
//			}

			// TODO: ASMFS


			// --------------------------------------------------
			return Result;
		}

		static string GetLibArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			return Result;
		}

		public void AddIncludePath(ref string Arguments, DirectoryReference IncludePath)
		{
			if(IncludePath.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory));
			}
			else
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, List<Action> Actions)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);

			CPPOutput Result = new CPPOutput();

			// Add include paths to the argument list.
			foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
			{
				AddIncludePath(ref Arguments, IncludePath);
			}
			foreach (DirectoryReference IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				AddIncludePath(ref Arguments, IncludePath);
			}


			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D{0}", Definition);
			}

			if (bEnableTracing)
			{
				Arguments += string.Format(" -D__EMSCRIPTEN_TRACING__");
			}

			// Force include all the requested headers
			foreach(FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
			{
				Arguments += String.Format(" -include \"{0}\"", ForceIncludeFile.Location);
			}

			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";
				CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);
//				CompileAction.bPrintDebugInfo = true;

				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);

				// Add the source file path to the command-line.
				string FileArguments = string.Format(" \"{0}\"", SourceFile.AbsolutePath);
				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + libExt));
				CompileAction.ProducedItems.Add(ObjectFile);
				FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					FileArguments += GetCLArguments_C(CompileEnvironment.Architecture);
				}
				else
				{
					FileArguments += GetCLArguments_CPP(CompileEnvironment);
				}

				// Generate the included header dependency list
				if(CompileEnvironment.bGenerateDependenciesFile)
				{
					FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".d"));
					FileArguments += string.Format(" -MD -MF\"{0}\"", DependencyListFile.AbsolutePath.Replace('\\', '/'));
					CompileAction.DependencyListFile = DependencyListFile;
					CompileAction.ProducedItems.Add(DependencyListFile);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				CompileAction.CommandPath = HTML5SDKInfo.Python();

				CompileAction.CommandArguments = HTML5SDKInfo.EmscriptenCompiler() + " " + Arguments + FileArguments + CompileEnvironment.AdditionalArguments;

				//System.Console.WriteLine(CompileAction.CommandArguments);
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);

				// Don't farm out creation of precomputed headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;

				// this is the final output of the compile step (a .abc file)
				Result.ObjectFiles.Add(ObjectFile);

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = true;

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					CompileEnvironment.bAllowRemotelyCompiledPCHs;

				Actions.Add(CompileAction);
			}

			return Result;
		}

		public override CPPOutput CompileRCFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, List<Action> Actions)
		{
			CPPOutput Result = new CPPOutput();

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, List<Action> Actions)
		{
			FileItem OutputFile;

			// Make the final javascript file
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.CommandDescription = "Link";
//			LinkAction.bPrintDebugInfo = true;

			// ResponseFile lines.
			List<string> ReponseLines = new List<string>();

			LinkAction.bCanExecuteRemotely = false;
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
			LinkAction.CommandPath = HTML5SDKInfo.Python();
			LinkAction.CommandArguments = HTML5SDKInfo.EmscriptenCompiler();
//			bool bIsBuildingLibrary = LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly;
//			ReponseLines.Add(
//					bIsBuildingLibrary ?
//					GetLibArguments(LinkEnvironment) :
//					GetLinkArguments(LinkEnvironment)
//				);
			ReponseLines.Add(GetLinkArguments(LinkEnvironment));

			// Add the input files to a response file, and pass the response file on the command-line.
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				//System.Console.WriteLine("File  {0} ", InputFile.AbsolutePath);
				ReponseLines.Add(string.Format(" \"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Make sure ThirdParty libs are at the end.
				List<string> ThirdParty = (from Lib in LinkEnvironment.AdditionalLibraries
											where Lib.Contains("ThirdParty")
											select Lib).ToList();

				LinkEnvironment.AdditionalLibraries.RemoveAll(Element => Element.Contains("ThirdParty"));
				LinkEnvironment.AdditionalLibraries.AddRange(ThirdParty);

				foreach (string InputFile in LinkEnvironment.AdditionalLibraries)
				{
					FileItem Item = FileItem.GetItemByPath(InputFile);

					if (Item.AbsolutePath.Contains(".lib"))
						continue;

					if (Item.ToString().EndsWith(".js"))
						ReponseLines.Add(string.Format(" --js-library \"{0}\"", Item.AbsolutePath));


					// WARNING: With --pre-js and --post-js, the order in which these directives are passed to
					// the compiler is very critical, because that dictates the order in which they are appended.
					//
					// Set environment variable [ EMCC_DEBUG=1 ] to see the linker order used in packaging.
					//     See GetSharedArguments_Global() above to set this environment variable

					else if (Item.ToString().EndsWith(".jspre"))
						ReponseLines.Add(string.Format(" --pre-js \"{0}\"", Item.AbsolutePath));

					else if (Item.ToString().EndsWith(".jspost"))
						ReponseLines.Add(string.Format(" --post-js \"{0}\"", Item.AbsolutePath));


					else
						ReponseLines.Add(string.Format(" \"{0}\"", Item.AbsolutePath));

					LinkAction.PrerequisiteItems.Add(Item);
				}
			}
			// make the file we will create


			OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			ReponseLines.Add(string.Format(" -o \"{0}\"", OutputFile.AbsolutePath));

			FileItem OutputLink = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath.FullName.Replace(".js", libExt).Replace(".html", libExt));
			LinkAction.ProducedItems.Add(OutputLink);
			if(!useLLVMwasmBackend)
			{
				ReponseLines.Add(string.Format(" --save-bc \"{0}\"", OutputLink.AbsolutePath));
			}

			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);

			FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponseFileName, ReponseLines);

			LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFileName);
			LinkAction.PrerequisiteItems.Add(ResponseFileItem);
			Actions.Add(LinkAction);

			Log.TraceInformation("NOTE: about to link for HTML5 -- this takes at least 7 minutes (and up to 20 minutes on older machines) to complete.");
			Log.TraceInformation("      we are workig with the Emscripten makers to speed this up.");

			return OutputFile;
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			// we need to include the generated .mem and .symbols file.
			if (Binary.Type != UEBuildBinaryType.StaticLibrary)
			{
				BuildProducts.Add(Binary.OutputFilePath.ChangeExtension("wasm"), BuildProductType.RequiredResource);
				if ( enableMultithreading )
				{
					BuildProducts.Add(Binary.OutputFilePath + ".mem", BuildProductType.RequiredResource);
				}
				BuildProducts.Add(Binary.OutputFilePath + ".symbols", BuildProductType.RequiredResource);
			}
		}
	};
}
