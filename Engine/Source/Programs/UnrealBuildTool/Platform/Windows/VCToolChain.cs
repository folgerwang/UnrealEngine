// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class VCToolChain : UEToolChain
	{
		/// <summary>
		/// The target being built
		/// </summary>
		protected ReadOnlyTargetRules Target;

		/// <summary>
		/// The Visual C++ environment
		/// </summary>
		protected VCEnvironment EnvVars;

		public VCToolChain(CppPlatform Platform, ReadOnlyTargetRules Target)
			: base(Platform)
		{
			this.Target = Target;
			this.EnvVars = Target.WindowsPlatform.Environment;

			Log.TraceLog("Compiler: {0}", EnvVars.CompilerPath);
			Log.TraceLog("Linker: {0}", EnvVars.LinkerPath);
			Log.TraceLog("Library Manager: {0}", EnvVars.LibraryManagerPath);
			Log.TraceLog("Resource Compiler: {0}", EnvVars.ResourceCompilerPath);

			if (Target.WindowsPlatform.ObjSrcMapFile != null)
			{
				try
				{
					File.Delete(Target.WindowsPlatform.ObjSrcMapFile);
				}
				catch
				{
				}
			}
		}

		/// <summary>
		/// Returns the version info for the toolchain. This will be output before building.
		/// </summary>
		/// <returns>String describing the current toolchain</returns>
		public override string GetVersionInfo()
		{
			if(EnvVars.Compiler == EnvVars.ToolChain)
			{
				return String.Format("Using {0} {1} toolchain ({2}) and Windows {3} SDK ({4}).", WindowsPlatform.GetCompilerName(EnvVars.Compiler), EnvVars.ToolChainVersion, EnvVars.ToolChainDir, EnvVars.WindowsSdkVersion, EnvVars.WindowsSdkDir);
			}
			else
			{
				return String.Format("Using {0} {1} compiler ({2}) with {3} {4} runtime ({5}) and Windows {6} SDK ({7}).", WindowsPlatform.GetCompilerName(EnvVars.Compiler), EnvVars.CompilerVersion, EnvVars.CompilerDir, WindowsPlatform.GetCompilerName(EnvVars.ToolChain), EnvVars.ToolChainVersion, EnvVars.ToolChainDir, EnvVars.WindowsSdkVersion, EnvVars.WindowsSdkDir);
			}
		}

		static void AddDefinition(List<string> Arguments, string Definition)
		{
			// Split the definition into name and value
			int ValueIdx = Definition.IndexOf('=');
			if (ValueIdx == -1)
			{
				AddDefinition(Arguments, Definition, null);
			}
			else
			{
				AddDefinition(Arguments, Definition.Substring(0, ValueIdx), Definition.Substring(ValueIdx + 1));
			}
		}

		static void AddDefinition(List<string> Arguments, string Variable, string Value)
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now
			if (Value != null && !Value.StartsWith("\"") && (Value.Contains(" ") || Value.Contains("$")))
			{
				Value = "\"" + Value + "\"";
			}

			if (Value != null)
			{
				Arguments.Add("/D" + Variable + "=" + Value);
			}
			else
			{
				Arguments.Add("/D" + Variable);
			}
		}


		void AddIncludePath(List<string> Arguments, DirectoryReference IncludePath)
		{
			// If the value has a space in it and isn't wrapped in quotes, do that now. Make sure it doesn't include a trailing slash, because that will escape the closing quote.
			string IncludePathString;
			if(IncludePath.IsUnderDirectory(UnrealBuildTool.RootDirectory) && Target.WindowsPlatform.Compiler != WindowsCompiler.Clang)
			{
				IncludePathString = IncludePath.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory);
			}
			else
			{
				IncludePathString = IncludePath.FullName;
			}

			if(IncludePathString.Contains(" "))
			{
				IncludePathString = "\"" + IncludePathString + "\"";
			}

			Arguments.Add("/I " + IncludePathString);
		}

		void AddSystemIncludePath(List<string> Arguments, DirectoryReference IncludePath)
		{
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				// Clang has special treatment for system headers; only system include directories are searched when include directives use angle brackets,
				// and warnings are disabled to allow compiler toolchains to be upgraded separately.
				Arguments.Add(String.Format("/imsvc \"{0}\"", IncludePath));
			}
			else
			{
				AddIncludePath(Arguments, IncludePath);
			}
		}


		void AppendCLArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// NOTE: Uncommenting this line will print includes as they are encountered by the preprocessor.  This can help with diagnosing include order problems.
			//Arguments.Add("/showIncludes");

			// Suppress generation of object code for unreferenced inline functions. Enabling this option is more standards compliant, and causes a big reduction
			// in object file sizes (and link times) due to the amount of stuff we inline.
			Arguments.Add("/Zc:inline");

			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				Arguments.Add("-fms-compatibility-version=19.1");
			}

			// @todo clang: Clang on Windows doesn't respect "#pragma warning (error: ####)", and we're not passing "/WX", so warnings are not
			// treated as errors when compiling on Windows using Clang right now.

			// NOTE re: clang: the arguments for clang-cl can be found at http://llvm.org/viewvc/llvm-project/cfe/trunk/include/clang/Driver/CLCompatOptions.td?view=markup
			// This will show the cl.exe options that map to clang.exe ones, which ones are ignored and which ones are unsupported.

			if (Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.VisualCpp)
			{
				Arguments.Add("/analyze");

				// Don't cause analyze warnings to be errors
				Arguments.Add("/analyze:WX-");

				// Report functions that use a LOT of stack space.  You can lower this value if you
				// want more aggressive checking for functions that use a lot of stack memory.
				Arguments.Add("/analyze:stacksize81940");

				// Don't bother generating code, only analyze code (may report fewer warnings though.)
				//Arguments.Add("/analyze:only");
			}

			// Prevents the compiler from displaying its logo for each invocation.
			Arguments.Add("/nologo");

			// Enable intrinsic functions.
			Arguments.Add("/Oi");


			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				// Tell the Clang compiler whether we want to generate 32-bit code or 64-bit code
				if (CompileEnvironment.Platform == CppPlatform.Win64)
				{
					Arguments.Add("--target=x86_64-pc-windows-msvc");
				}
				else
				{
					Arguments.Add("--target=i686-pc-windows-msvc");
				}
			}

			// Compile into an .obj file, and skip linking.
			Arguments.Add("/c");

			// Put symbols into different sections so the linker can remove them.
			if(Target.WindowsPlatform.bOptimizeGlobalData)
			{
				Arguments.Add("/Gw");
			}

			// Separate functions for linker.
			Arguments.Add("/Gy");

			// Allow 750% of the default memory allocation limit when using the static analyzer, and 850% at other times.
			if(Target.WindowsPlatform.PCHMemoryAllocationFactor == 0)
			{
				if (Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.VisualCpp)
				{
					Arguments.Add("/Zm750");
				}
				else
				{
					Arguments.Add("/Zm850");
				}
			}
			else
			{
				if(Target.WindowsPlatform.PCHMemoryAllocationFactor > 0)
				{
					Arguments.Add(String.Format("/Zm{0}", Target.WindowsPlatform.PCHMemoryAllocationFactor));
				}
			}

			// Disable "The file contains a character that cannot be represented in the current code page" warning for non-US windows.
			Arguments.Add("/wd4819");

			// Disable Microsoft extensions on VS2017+ for improved standards compliance.
			if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2017 && Target.WindowsPlatform.bStrictConformanceMode)
			{
				Arguments.Add("/permissive-");
				Arguments.Add("/Zc:strictStrings-"); // Have to disable strict const char* semantics due to Windows headers not being compliant.
			}

			// @todo UWP: UE4 is non-compliant when it comes to use of %s and %S
			// Previously %s meant "the current character set" and %S meant "the other one".
			// Now %s means multibyte and %S means wide. %Ts means "natural width".
			// Reverting this behaviour until the UE4 source catches up.
			if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015_DEPRECATED || Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				AddDefinition(Arguments, "_CRT_STDIO_LEGACY_WIDE_SPECIFIERS=1");
			}

			// @todo UWP: Silence the hash_map deprecation errors for now. This should be replaced with unordered_map for the real fix.
			if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015_DEPRECATED || Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				AddDefinition(Arguments, "_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS=1");
			}

			// Ignore secure CRT warnings on Clang
			if(Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				AddDefinition(Arguments, "_CRT_SECURE_NO_WARNINGS");
			}

			// If compiling as a DLL, set the relevant defines
			if (CompileEnvironment.bIsBuildingDLL)
			{
				AddDefinition(Arguments, "_WINDLL");
			}

			// Maintain the old std::aligned_storage behavior from VS from v15.8 onwards, in case of prebuilt third party libraries are reliant on it
			AddDefinition(Arguments, "_DISABLE_EXTENDED_ALIGNED_STORAGE");

            // Fix Incredibuild errors with helpers using heterogeneous character sets
            if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015_DEPRECATED)
            {
                Arguments.Add("/source-charset:utf-8");
				Arguments.Add("/execution-charset:utf-8");
            }

			//
			//	Debug
			//
			if (CompileEnvironment.Configuration == CppConfiguration.Debug)
			{
				// Disable compiler optimization.
				Arguments.Add("/Od");

				// Favor code size (especially useful for embedded platforms).
				Arguments.Add("/Os");

				// Allow inline method expansion unless E&C support is requested
				if (!CompileEnvironment.bSupportEditAndContinue && CompileEnvironment.bUseInlining)
				{
					Arguments.Add("/Ob2");
				}

				if ((CompileEnvironment.Platform == CppPlatform.Win32) ||
					(CompileEnvironment.Platform == CppPlatform.Win64))
				{
					Arguments.Add("/RTCs");
				}
			}
			//
			//	Development and LTCG
			//
			else
			{
				if(!CompileEnvironment.bOptimizeCode)
				{
					// Disable compiler optimization.
					Arguments.Add("/Od");
				}
				else
				{
					// Maximum optimizations.
					Arguments.Add("/Ox");

					// Favor code speed.
					Arguments.Add("/Ot");

					// Coalesce duplicate strings
					Arguments.Add("/GF");

					// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
					if (CompileEnvironment.bOmitFramePointers == false
					&& ((CompileEnvironment.Platform == CppPlatform.Win32) ||
						(CompileEnvironment.Platform == CppPlatform.Win64)))
					{
						Arguments.Add("/Oy-");
					}
				}

				// Allow inline method expansion
				Arguments.Add("/Ob2");

				//
				// LTCG
				//
				if (CompileEnvironment.bAllowLTCG)
				{
					// Enable link-time code generation.
					Arguments.Add("/GL");
				}
			}

			//
			//	PC
			//
			if ((CompileEnvironment.Platform == CppPlatform.Win32) ||
				(CompileEnvironment.Platform == CppPlatform.Win64))
			{
				if (CompileEnvironment.bUseAVX)
				{
					// Allow the compiler to generate AVX instructions.
					Arguments.Add("/arch:AVX");
				}
				// SSE options are not allowed when using the 64 bit toolchain
				// (enables SSE2 automatically)
				else if (CompileEnvironment.Platform != CppPlatform.Win64)
				{
					// Allow the compiler to generate SSE2 instructions.
					Arguments.Add("/arch:SSE2");
				}

				if (Target.WindowsPlatform.Compiler != WindowsCompiler.Intel)
				{
					// Prompt the user before reporting internal errors to Microsoft.
					Arguments.Add("/errorReport:prompt");
				}

				// Enable C++ exceptions when building with the editor or when building UHT.
				if (CompileEnvironment.bEnableExceptions)
				{
					// Enable C++ exception handling, but not C exceptions.
					Arguments.Add("/EHsc");
				}
				else
				{
					// This is required to disable exception handling in VC platform headers.
					CompileEnvironment.Definitions.Add("_HAS_EXCEPTIONS=0");
				}
			}
			else if (CompileEnvironment.Platform == CppPlatform.HTML5)
			{
				Arguments.Add("/EHsc");
			}

			// If enabled, create debug information.
			if (CompileEnvironment.bCreateDebugInfo)
			{
				// Store debug info in .pdb files.
				// @todo clang: PDB files are emited from Clang but do not fully work with Visual Studio yet (breakpoints won't hit due to "symbol read error")
				// @todo clang (update): as of clang 3.9 breakpoints work with PDBs, and the callstack is correct, so could be used for crash dumps. However debugging is still impossible due to the large amount of unreadable variables and unpredictable breakpoint stepping behaviour
				if (CompileEnvironment.bUsePDBFiles || CompileEnvironment.bSupportEditAndContinue)
				{
					// Create debug info suitable for E&C if wanted.
					if (CompileEnvironment.bSupportEditAndContinue)
					{
						Arguments.Add("/ZI");
					}
					// Regular PDB debug information.
					else
					{
						Arguments.Add("/Zi");
					}
					// We need to add this so VS won't lock the PDB file and prevent synchronous updates. This forces serialization through MSPDBSRV.exe.
					// See http://msdn.microsoft.com/en-us/library/dn502518.aspx for deeper discussion of /FS switch.
					if (CompileEnvironment.bUseIncrementalLinking)
					{
						Arguments.Add("/FS");
					}
				}
				// Store C7-format debug info in the .obj files, which is faster.
				else
				{
					Arguments.Add("/Z7");
				}
			}

			// Specify the appropriate runtime library based on the platform and config.
			if (CompileEnvironment.bUseStaticCRT)
			{
				if (CompileEnvironment.bUseDebugCRT)
				{
					Arguments.Add("/MTd");
				}
				else
				{
					Arguments.Add("/MT");
				}
			}
			else
			{
				if (CompileEnvironment.bUseDebugCRT)
				{
					Arguments.Add("/MDd");
				}
				else
				{
					Arguments.Add("/MD");
				}
			}

			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang)
			{
				// Allow large object files to avoid hitting the 2^16 section limit when running with -StressTestUnity.
				// Note: not needed for clang, it implicitly upgrades COFF files to bigobj format when necessary.
				Arguments.Add("/bigobj");
			}

			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Intel || Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				// FMath::Sqrt calls get inlined and when reciprical is taken, turned into an rsqrtss instruction,
				// which is *too* imprecise for, e.g., TestVectorNormalize_Sqrt in UnrealMathTest.cpp
				// TODO: Observed in clang 7.0, presumably the same in Intel C++ Compiler?
				Arguments.Add("/fp:precise");
			}
			else
			{
				// Relaxes floating point precision semantics to allow more optimization.
				Arguments.Add("/fp:fast");
			}

			if (CompileEnvironment.bOptimizeCode)
			{
				// Allow optimized code to be debugged more easily.  This makes PDBs a bit larger, but doesn't noticeably affect
				// compile times.  The executable code is not affected at all by this switch, only the debugging information.
				Arguments.Add("/Zo");
			}

			if (CompileEnvironment.Platform == CppPlatform.Win64)
			{
				// Pack struct members on 8-byte boundaries.
				Arguments.Add("/Zp8");
			}
			else
			{
				// Pack struct members on 4-byte boundaries.
				Arguments.Add("/Zp4");
			}

			//@todo: Disable warnings for VS2015. These should be reenabled as we clear the reasons for them out of the engine source and the VS2015 toolchain evolves.
			if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015_DEPRECATED)
			{
				// Disable shadow variable warnings
				if (CompileEnvironment.bEnableShadowVariableWarnings == false)
				{
					Arguments.Add("/wd4456"); // 4456 - declaration of 'LocalVariable' hides previous local declaration
					Arguments.Add("/wd4458"); // 4458 - declaration of 'parameter' hides class member
					Arguments.Add("/wd4459"); // 4459 - declaration of 'LocalVariable' hides global declaration
				}

				Arguments.Add("/wd4463"); // 4463 - overflow; assigning 1 to bit-field that can only hold values from -1 to 0

				Arguments.Add("/wd4838"); // 4838: conversion from 'type1' to 'type2' requires a narrowing conversion
			}

			if(CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				if (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors)
				{
					Arguments.Add("/we4668");
				}
				else
				{
					Arguments.Add("/w44668");
				}
			}
		}

		void AppendCLArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang)
			{
				// Explicitly compile the file as C++.
				Arguments.Add("/TP");
			}
			else
			{
				string FileSpecifier = "c++";
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Tell Clang to generate a PCH header
					FileSpecifier += "-header";
				}
				
				Arguments.Add(String.Format("-Xclang -x -Xclang \"{0}\"", FileSpecifier));
			}


			if (!CompileEnvironment.bEnableBufferSecurityChecks)
			{
				// This will disable buffer security checks (which are enabled by default) that the MS compiler adds around arrays on the stack,
				// Which can add some performance overhead, especially in performance intensive code
				// Only disable this if you know what you are doing, because it will be disabled for the entire module!
				Arguments.Add("/GS-");
			}

			// Configure RTTI
			if (CompileEnvironment.bUseRTTI)
			{
				// Enable C++ RTTI.
				Arguments.Add("/GR");
			}
			else
			{
				// Disable C++ RTTI.
				Arguments.Add("/GR-");
			}

			// Set warning level.
			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Intel)
			{
				// Restrictive during regular compilation.
				Arguments.Add("/W4");
			}
			else
			{
				// If we had /W4 with clang or Intel on windows we would be flooded with warnings. This will be fixed incrementally.
				Arguments.Add("/W0");
			}

			// Intel compiler options.
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Intel)
			{
				Arguments.Add("/Qstd=c++14");
			}
			else
			{
				if(CompileEnvironment.CppStandard >= CppStandardVersion.Latest)
				{
					Arguments.Add("/std:c++latest");
				}
				else if(CompileEnvironment.CppStandard >= CppStandardVersion.Cpp17)
				{
					Arguments.Add("/std:c++17");
				}
				else if(CompileEnvironment.CppStandard >= CppStandardVersion.Cpp14)
				{
					Arguments.Add("/std:c++14");
				}
			}

			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
			{
				// Disable specific warnings that cause problems with Clang
				// NOTE: These must appear after we set the MSVC warning level

				// @todo clang: Ideally we want as few warnings disabled as possible
				// 

				// Treat all warnings as errors by default
				Arguments.Add("-Werror");

				// Allow Microsoft-specific syntax to slide, even though it may be non-standard.  Needed for Windows headers.
				Arguments.Add("-Wno-microsoft");

				// @todo clang: Hack due to how we have our 'DummyPCH' wrappers setup when using unity builds.  This warning should not be disabled!!
				Arguments.Add("-Wno-msvc-include");

				if (CompileEnvironment.bEnableShadowVariableWarnings)
				{
					Arguments.Add("-Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow"));
				}

				if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
				{
					Arguments.Add(" -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef"));
				}

				// @todo clang: Kind of a shame to turn these off.  We'd like to catch unused variables, but it is tricky with how our assertion macros work.
				Arguments.Add("-Wno-inconsistent-missing-override");
				Arguments.Add("-Wno-unused-variable");
				Arguments.Add("-Wno-unused-local-typedefs");
				Arguments.Add("-Wno-unused-function");
				Arguments.Add("-Wno-unused-private-field");
				Arguments.Add("-Wno-unused-value");

				Arguments.Add("-Wno-inline-new-delete");	// @todo clang: We declare operator new as inline.  Clang doesn't seem to like that.
				Arguments.Add("-Wno-implicit-exception-spec-mismatch");

				// Sometimes we compare 'this' pointers against nullptr, which Clang warns about by default
				Arguments.Add("-Wno-undefined-bool-conversion");

				// @todo clang: Disabled warnings were copied from MacToolChain for the most part
				Arguments.Add("-Wno-deprecated-declarations");
				Arguments.Add("-Wno-deprecated-writable-strings");
				Arguments.Add("-Wno-deprecated-register");
				Arguments.Add("-Wno-switch-enum");
				Arguments.Add("-Wno-logical-op-parentheses");	// needed for external headers we shan't change
				Arguments.Add("-Wno-null-arithmetic");			// needed for external headers we shan't change
				Arguments.Add("-Wno-deprecated-declarations");	// needed for wxWidgets
				Arguments.Add("-Wno-return-type-c-linkage");	// needed for PhysX
				Arguments.Add("-Wno-ignored-attributes");		// needed for nvtesslib
				Arguments.Add("-Wno-uninitialized");
				Arguments.Add("-Wno-tautological-compare");
				Arguments.Add("-Wno-switch");
				Arguments.Add("-Wno-invalid-offsetof"); // needed to suppress warnings about using offsetof on non-POD types.

				// @todo clang: Sorry for adding more of these, but I couldn't read my output log. Most should probably be looked at
				Arguments.Add("-Wno-unused-parameter");			// Unused function parameter. A lot are named 'bUnused'...
				Arguments.Add("-Wno-ignored-qualifiers");		// const ignored when returning by value e.g. 'const int foo() { return 4; }'
				Arguments.Add("-Wno-expansion-to-defined");		// Usage of 'defined(X)' in a macro definition. Gives different results under MSVC
				Arguments.Add("-Wno-gnu-string-literal-operator-template");	// String literal operator"" in template, used by delegates
				Arguments.Add("-Wno-sign-compare");				// Signed/unsigned comparison - millions of these
				Arguments.Add("-Wno-undefined-var-template");	// Variable template instantiation required but no definition available
				Arguments.Add("-Wno-missing-field-initializers"); // Stupid warning, generated when you initialize with MyStruct A = {0};
				Arguments.Add("-Wno-unused-lambda-capture");
				Arguments.Add("-Wno-nonportable-include-path");
				Arguments.Add("-Wno-invalid-token-paste");
				Arguments.Add("-Wno-null-pointer-arithmetic");
				Arguments.Add("-Wno-constant-logical-operand"); // Triggered by || of two template-derived values inside a static_assert

			}
		}

		static void AppendCLArguments_C(List<string> Arguments)
		{
			// Explicitly compile the file as C.
			Arguments.Add("/TC");

			// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
			Arguments.Add("/W0");
		}

		void AppendLinkArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang && WindowsPlatform.bAllowClangLinker)
			{
				// This tells LLD to run in "Windows emulation" mode, meaning that it will accept MSVC Link arguments
				Arguments.Add("-flavor link");

				// @todo clang: The following static libraries aren't linking correctly with Clang:
				//		tbbmalloc.lib, zlib_64.lib, libpng_64.lib, freetype2412MT.lib, IlmImf.lib
				//		LLD: Assertion failed: result.size() == 1, file ..\tools\lld\lib\ReaderWriter\FileArchive.cpp, line 71
				//		

				// Only omit frame pointers on the PC (which is implied by /Ox) if wanted.
				if (!LinkEnvironment.bOmitFramePointers)
				{
					Arguments.Add("--disable-fp-elim");
				}
			}

			// Don't create a side-by-side manifest file for the executable.
			Arguments.Add("/MANIFEST:NO");

			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Add("/NOLOGO");

			if (LinkEnvironment.bCreateDebugInfo)
			{
				// Output debug info for the linked executable.
				Arguments.Add("/DEBUG");

				// Allow partial PDBs for faster linking
				if (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2015_DEPRECATED && LinkEnvironment.bUseFastPDBLinking)
				{
					Arguments[Arguments.Count - 1] += ":FASTLINK";
				}
			}

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Add("/errorReport:prompt");

			//
			//	PC
			//
			if ((LinkEnvironment.Platform == CppPlatform.Win32) ||
				(LinkEnvironment.Platform == CppPlatform.Win64))
			{
				// Set machine type/ architecture to be 64 bit.
				if (LinkEnvironment.Platform == CppPlatform.Win64)
				{
					Arguments.Add("/MACHINE:x64");
				}
				// 32 bit executable/ target.
				else
				{
					Arguments.Add("/MACHINE:x86");
				}

				{
					if (LinkEnvironment.bIsBuildingConsoleApplication)
					{
						Arguments.Add("/SUBSYSTEM:CONSOLE");
					}
					else
					{
						Arguments.Add("/SUBSYSTEM:WINDOWS");
					}
				}

				if (LinkEnvironment.bIsBuildingConsoleApplication && !LinkEnvironment.bIsBuildingDLL && !String.IsNullOrEmpty(LinkEnvironment.WindowsEntryPointOverride))
				{
					// Use overridden entry point
					Arguments.Add("/ENTRY:" + LinkEnvironment.WindowsEntryPointOverride);
				}

				// Allow the OS to load the EXE at different base addresses than its preferred base address.
				Arguments.Add("/FIXED:No");

				// Option is only relevant with 32 bit toolchain.
				if ((LinkEnvironment.Platform == CppPlatform.Win32) && Target.WindowsPlatform.bBuildLargeAddressAwareBinary)
				{
					// Disables the 2GB address space limit on 64-bit Windows and 32-bit Windows with /3GB specified in boot.ini
					Arguments.Add("/LARGEADDRESSAWARE");
				}

				// Explicitly declare that the executable is compatible with Data Execution Prevention.
				Arguments.Add("/NXCOMPAT");

				// Set the default stack size.
				if (LinkEnvironment.DefaultStackSizeCommit > 0)
				{
					Arguments.Add("/STACK:" + LinkEnvironment.DefaultStackSize + "," + LinkEnvironment.DefaultStackSizeCommit);
				}
				else
				{
					Arguments.Add("/STACK:" + LinkEnvironment.DefaultStackSize);
				}

				// E&C can't use /SAFESEH.  Also, /SAFESEH isn't compatible with 64-bit linking
				if (!LinkEnvironment.bSupportEditAndContinue &&
					LinkEnvironment.Platform != CppPlatform.Win64)
				{
					// Generates a table of Safe Exception Handlers.  Documentation isn't clear whether they actually mean
					// Structured Exception Handlers.
					Arguments.Add("/SAFESEH");
				}

				// Allow delay-loaded DLLs to be explicitly unloaded.
				Arguments.Add("/DELAY:UNLOAD");

				if (LinkEnvironment.bIsBuildingDLL)
				{
					Arguments.Add("/DLL");
				}
			}

			// Don't embed the full PDB path; we want to be able to move binaries elsewhere. They will always be side by side.
			Arguments.Add("/PDBALTPATH:%_PDB%");

			//
			//	Shipping & LTCG
			//
			if (LinkEnvironment.bAllowLTCG)
			{
				// Use link-time code generation.
				Arguments.Add("/LTCG");

				// This is where we add in the PGO-Lite linkorder.txt if we are using PGO-Lite
				//Result += " /ORDER:@linkorder.txt";
				//Result += " /VERBOSE";
			}

			//
			//	Shipping binary
			//
			if (LinkEnvironment.Configuration == CppConfiguration.Shipping)
			{
				// Generate an EXE checksum.
				Arguments.Add("/RELEASE");

				// Eliminate unreferenced symbols.
				Arguments.Add("/OPT:REF");

				// Remove redundant COMDATs.
				Arguments.Add("/OPT:ICF");
			}
			//
			//	Regular development binary. 
			//
			else
			{
				// Keep symbols that are unreferenced.
				Arguments.Add("/OPT:NOREF");

				// Disable identical COMDAT folding.
				Arguments.Add("/OPT:NOICF");
			}

			// Enable incremental linking if wanted.
			if (LinkEnvironment.bUseIncrementalLinking)
			{
				Arguments.Add("/INCREMENTAL");
			}
			else
			{
				Arguments.Add("/INCREMENTAL:NO");
			}

			// Disable 
			//LINK : warning LNK4199: /DELAYLOAD:nvtt_64.dll ignored; no imports found from nvtt_64.dll
			// type warning as we leverage the DelayLoad option to put third-party DLLs into a 
			// non-standard location. This requires the module(s) that use said DLL to ensure that it 
			// is loaded prior to using it.
			Arguments.Add("/ignore:4199");

			// Suppress warnings about missing PDB files for statically linked libraries.  We often don't want to distribute
			// PDB files for these libraries.
			Arguments.Add("/ignore:4099");		// warning LNK4099: PDB '<file>' was not found with '<file>'
		}

		void AppendLibArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			// Prevents the linker from displaying its logo for each invocation.
			Arguments.Add("/NOLOGO");

			// Prompt the user before reporting internal errors to Microsoft.
			Arguments.Add("/errorReport:prompt");

			//
			//	PC
			//
			if (LinkEnvironment.Platform == CppPlatform.Win32 || LinkEnvironment.Platform == CppPlatform.Win64)
			{
				// Set machine type/ architecture to be 64 bit.
				if (LinkEnvironment.Platform == CppPlatform.Win64)
				{
					Arguments.Add("/MACHINE:x64");
				}
				// 32 bit executable/ target.
				else
				{
					Arguments.Add("/MACHINE:x86");
				}

				{
					if (LinkEnvironment.bIsBuildingConsoleApplication)
					{
						Arguments.Add("/SUBSYSTEM:CONSOLE");
					}
					else
					{
						Arguments.Add("/SUBSYSTEM:WINDOWS");
					}
				}
			}

			//
			//	Shipping & LTCG
			//
			if (LinkEnvironment.Configuration == CppConfiguration.Shipping)
			{
				// Use link-time code generation.
				Arguments.Add("/LTCG");
			}
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, List<Action> Actions)
		{
			List<string> SharedArguments = new List<string>();
			AppendCLArguments_Global(CompileEnvironment, SharedArguments);

			// Add include paths to the argument list.
			foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
			{
				AddIncludePath(SharedArguments, IncludePath);
			}

			foreach (DirectoryReference IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				AddSystemIncludePath(SharedArguments, IncludePath);
			}

			foreach (DirectoryReference IncludePath in EnvVars.IncludePaths)
			{
				AddSystemIncludePath(SharedArguments, IncludePath);
			}

			if (CompileEnvironment.bPrintTimingInfo)
			{
				if(Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015_DEPRECATED || Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					SharedArguments.Add("/Bt+ /d2cgsummary");
					if(EnvVars.ToolChainVersion >= VersionNumber.Parse("14.14.26316"))
					{
						SharedArguments.Add("/d1reportTime");
					}
				}
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				string DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				AddDefinition(SharedArguments, DefinitionArgument);
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Compile";

				// ensure compiler timings are captured when we execute the action.
				if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && CompileEnvironment.bPrintTimingInfo)
				{
					CompileAction.bPrintDebugInfo = true;
				}

				List<string> FileArguments = new List<string>();
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);

				bool bEmitsObjectFile = true;
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Generate a CPP File that just includes the precompiled header.
					FileReference PCHCPPPath = CompileEnvironment.PrecompiledHeaderIncludeFilename.ChangeExtension(".cpp");
					FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
						PCHCPPPath,
						string.Format("#include \"{0}\"\r\n", CompileEnvironment.PrecompiledHeaderIncludeFilename.FullName.Replace('\\', '/'))
						);

					// Make sure the original source directory the PCH header file existed in is added as an include
					// path -- it might be a private PCH header and we need to make sure that its found!
					AddIncludePath(FileArguments, SourceFile.Location.Directory);

					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							OutputDir,
							Path.GetFileName(SourceFile.AbsolutePath) + ".pch"
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments.Add(String.Format("/Yc\"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename));
					FileArguments.Add(String.Format("/Fp\"{0}\"", PrecompiledHeaderFile.AbsolutePath));

					// If we're creating a PCH that will be used to compile source files for a library, we need
					// the compiled modules to retain a reference to PCH's module, so that debugging information
					// will be included in the library.  This is also required to avoid linker warning "LNK4206"
					// when linking an application that uses this library.
					if (CompileEnvironment.bIsBuildingLibrary)
					{
						// NOTE: The symbol name we use here is arbitrary, and all that matters is that it is
						// unique per PCH module used in our library
						string FakeUniquePCHSymbolName = CompileEnvironment.PrecompiledHeaderIncludeFilename.GetFileNameWithoutExtension();
						FileArguments.Add(String.Format("/Yl{0}", FakeUniquePCHSymbolName));
					}

					FileArguments.Add(String.Format("\"{0}\"", PCHCPPFile.AbsolutePath));

					CompileAction.StatusDescription = PCHCPPPath.GetFileName();
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);

						if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
						{
							FileArguments.Add(String.Format("/FI\"{0}\"", Path.ChangeExtension(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath, null)));
						}
						else
						{
							FileArguments.Add(String.Format("/FI\"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename.FullName));
							FileArguments.Add(String.Format("/Yu\"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename.FullName));
							FileArguments.Add(String.Format("/Fp\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath));
						}
					}

					// Add the source file path to the command-line.
					FileArguments.Add(String.Format("\"{0}\"", SourceFile.AbsolutePath));

					CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				}

				foreach(FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
				{
					FileArguments.Add(String.Format("/FI\"{0}\"", ForceIncludeFile.Location));
				}

				if (bEmitsObjectFile)
				{
					// Add the object file to the produced item list.
					string ObjectLeafFilename = Path.GetFileName(SourceFile.AbsolutePath) + ".obj";
					FileItem ObjectFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, ObjectLeafFilename));
					if (Target.WindowsPlatform.ObjSrcMapFile != null)
					{
						using (StreamWriter Writer = File.AppendText(Target.WindowsPlatform.ObjSrcMapFile))
						{
							Writer.WriteLine(string.Format("\"{0}\" -> \"{1}\"", ObjectLeafFilename, SourceFile.AbsolutePath));
						}
					}
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);
					FileArguments.Add(String.Format("/Fo\"{0}\"", ObjectFile.AbsolutePath));

					// Experimental: support for JSON output of timing data
					if(Target.WindowsPlatform.Compiler == WindowsCompiler.Clang && Target.WindowsPlatform.bClangTimeTrace)
					{
						SharedArguments.Add("-Xclang -ftime-trace");
						CompileAction.ProducedItems.Add(FileItem.GetItemByFileReference(ObjectFile.Location.ChangeExtension(".json")));
					}
				}

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					CompileEnvironment.bAllowRemotelyCompiledPCHs
					;

				// Create PDB files if we were configured to do that.
				if (CompileEnvironment.bUsePDBFiles || CompileEnvironment.bSupportEditAndContinue)
				{
					FileReference PDBLocation;
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						// All files using the same PCH are required to share the same PDB that was used when compiling the PCH
						PDBLocation = CompileEnvironment.PrecompiledHeaderFile.Location.ChangeExtension(".pdb");

						// Enable synchronous file writes, since we'll be modifying the existing PDB
						FileArguments.Add("/FS");
					}
					else if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
						// Files creating a PCH use a PDB per file.
						PDBLocation = FileReference.Combine(OutputDir, CompileEnvironment.PrecompiledHeaderIncludeFilename.GetFileName() + ".pdb");

						// Enable synchronous file writes, since we'll be modifying the existing PDB
						FileArguments.Add("/FS");
					}
					else if (!bIsPlainCFile)
					{
						// Ungrouped C++ files use a PDB per file.
						PDBLocation = FileReference.Combine(OutputDir, SourceFile.Location.GetFileName() + ".pdb");
					}
					else
					{
						// Group all plain C files that doesn't use PCH into the same PDB
						PDBLocation = FileReference.Combine(OutputDir, "MiscPlainC.pdb");
					}

					// Specify the PDB file that the compiler should write to.
					FileArguments.Add(String.Format("/Fd\"{0}\"", PDBLocation));

					// Don't add the PDB as an output file because it's modified multiple times. This will break timestamp dependency tracking.
					FileItem PDBFile = FileItem.GetItemByFileReference(PDBLocation);
					Result.DebugDataFiles.Add(PDBFile);

					// Don't allow remote execution when PDB files are enabled; we need to modify the same files. XGE works around this by generating separate
					// PDB files per agent, but this functionality is only available with the Visual C++ extension package (via the VCCompiler=true tool option).
					CompileAction.bCanExecuteRemotely = false;
				}

				// Add C or C++ specific compiler arguments.
				if (bIsPlainCFile)
				{
					AppendCLArguments_C(FileArguments);
				}
				else
				{
					AppendCLArguments_CPP(CompileEnvironment, FileArguments);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				CompileAction.CommandPath = EnvVars.CompilerPath;
				CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);

				string[] AdditionalArguments = String.IsNullOrEmpty(CompileEnvironment.AdditionalArguments)? new string[0] : new string[] { CompileEnvironment.AdditionalArguments };

				if (!ProjectFileGenerator.bGenerateProjectFiles
					&& Target.WindowsPlatform.Compiler != WindowsCompiler.Clang
					&& CompileAction.ProducedItems.Count > 0)
				{
					FileItem TargetFile = CompileAction.ProducedItems[0];
					FileReference ResponseFileName = new FileReference(TargetFile.AbsolutePath + ".response");
					FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponseFileName, SharedArguments.Concat(FileArguments).Concat(AdditionalArguments).Select(x => Utils.ExpandVariables(x)));
					CompileAction.CommandArguments = " @\"" + ResponseFileName + "\"";
					CompileAction.PrerequisiteItems.Add(ResponseFileItem);
				}
				else
				{
					CompileAction.CommandArguments = String.Join(" ", SharedArguments.Concat(FileArguments).Concat(AdditionalArguments));
				}

				if(CompileEnvironment.bGenerateDependenciesFile)
				{
					CompileAction.DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, SourceFile.Location.GetFileName() + ".txt"));
					CompileAction.CommandArguments = String.Format("{0} -- {1} {2} /showIncludes", Utils.MakePathSafeToUseWithCommandLine(CompileAction.DependencyListFile.Location), Utils.MakePathSafeToUseWithCommandLine(CompileAction.CommandPath), CompileAction.CommandArguments);
					CompileAction.CommandPath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Windows", "cl-filter", "cl-filter.exe");
					CompileAction.ProducedItems.Add(CompileAction.DependencyListFile);
				}

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					Log.TraceVerbose("Creating PCH " + CompileEnvironment.PrecompiledHeaderIncludeFilename + ": \"" + CompileAction.CommandPath + "\"" + CompileAction.CommandArguments);
				}
				else
				{
					Log.TraceVerbose("   Compiling " + CompileAction.StatusDescription + ": \"" + CompileAction.CommandPath + "\"" + CompileAction.CommandArguments);
				}

				if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
				{
					// Clang doesn't print the file names by default, so we'll do it ourselves
					CompileAction.bShouldOutputStatusDescription = true;
				}
				else
				{
					// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
					CompileAction.bShouldOutputStatusDescription = false;
				}

				// When compiling with SN-DBS, modules that contain a #import must be built locally
				if (CompileEnvironment.bBuildLocallyWithSNDBS == true)
				{
					CompileAction.bCanExecuteRemotelyWithSNDBS = false;
				}

				Actions.Add(CompileAction);
			}
			return Result;
		}

		public override CPPOutput CompileRCFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, List<Action> Actions)
		{
			CPPOutput Result = new CPPOutput();

			foreach (FileItem RCFile in InputFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.CommandDescription = "Resource";
				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				CompileAction.CommandPath = EnvVars.ResourceCompilerPath;
				CompileAction.StatusDescription = Path.GetFileName(RCFile.AbsolutePath);
				CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);

				// Resource tool can run remotely if possible
				CompileAction.bCanExecuteRemotely = true;

				List<string> Arguments = new List<string>();

				// Suppress header spew
				Arguments.Add("/nologo");

				// If we're compiling for 64-bit Windows, also add the _WIN64 definition to the resource
				// compiler so that we can switch on that in the .rc file using #ifdef.
				if (CompileEnvironment.Platform == CppPlatform.Win64)
				{
					AddDefinition(Arguments, "_WIN64");
				}

				// Language
				Arguments.Add("/l 0x409");

				// Include paths. Don't use AddIncludePath() here, since it uses the full path and exceeds the max command line length.
				foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
				{
					Arguments.Add(String.Format("/I \"{0}\"", IncludePath));
				}

				// System include paths.
				foreach (DirectoryReference SystemIncludePath in CompileEnvironment.SystemIncludePaths)
				{
					Arguments.Add(String.Format("/I \"{0}\"", SystemIncludePath));
				}
				foreach (DirectoryReference SystemIncludePath in EnvVars.IncludePaths)
				{
					Arguments.Add(String.Format("/I \"{0}\"", SystemIncludePath));
				}

				// Preprocessor definitions.
				foreach (string Definition in CompileEnvironment.Definitions)
				{
					if (!Definition.Contains("_API"))
					{
						AddDefinition(Arguments, Definition);
					}
				}

				// Figure the icon to use. We can only use a custom icon when compiling to a project-specific intemediate directory (and not for the shared editor executable, for example).
				FileReference IconFile;
				if(Target.ProjectFile != null && !CompileEnvironment.bUseSharedBuildEnvironment)
				{
					IconFile = WindowsPlatform.GetApplicationIcon(Target.ProjectFile);
				}
				else
				{
					IconFile = WindowsPlatform.GetApplicationIcon(null);
				}
				CompileAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(IconFile));

				// Setup the compile environment, setting the icon to use via a macro. This is used in Default.rc2.
				AddDefinition(Arguments, String.Format("BUILD_ICON_FILE_NAME=\"\\\"{0}\\\"\"", IconFile.FullName.Replace("\\", "\\\\")));

				// Apply the target settings for the resources
				if(!CompileEnvironment.bUseSharedBuildEnvironment)
				{
					if (!String.IsNullOrEmpty(Target.WindowsPlatform.CompanyName))
					{
						AddDefinition(Arguments, String.Format("PROJECT_COMPANY_NAME={0}", SanitizeMacroValue(Target.WindowsPlatform.CompanyName)));
					}

					if (!String.IsNullOrEmpty(Target.WindowsPlatform.CopyrightNotice))
					{
						AddDefinition(Arguments, String.Format("PROJECT_COPYRIGHT_STRING={0}", SanitizeMacroValue(Target.WindowsPlatform.CopyrightNotice)));
					}

					if (!String.IsNullOrEmpty(Target.WindowsPlatform.ProductName))
					{
						AddDefinition(Arguments, String.Format("PROJECT_PRODUCT_NAME={0}", SanitizeMacroValue(Target.WindowsPlatform.ProductName)));
					}

					if (Target.ProjectFile != null)
					{
						AddDefinition(Arguments, String.Format("PROJECT_PRODUCT_IDENTIFIER={0}", SanitizeMacroValue(Target.ProjectFile.GetFileNameWithoutExtension())));
					}
				}

				// Add the RES file to the produced item list.
				FileItem CompiledResourceFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						OutputDir,
						Path.GetFileName(RCFile.AbsolutePath) + ".res"
						)
					);
				CompileAction.ProducedItems.Add(CompiledResourceFile);
				Arguments.Add(String.Format("/fo \"{0}\"", CompiledResourceFile.AbsolutePath));
				Result.ObjectFiles.Add(CompiledResourceFile);

				// Add the RC file as a prerequisite of the action.
				Arguments.Add(String.Format(" \"{0}\"", RCFile.AbsolutePath));

				CompileAction.CommandArguments = String.Join(" ", Arguments);

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(RCFile);

				Actions.Add(CompileAction);
			}

			return Result;
		}

		/// <summary>
		/// Macros passed via the command line have their quotes stripped, and are tokenized before being re-stringized by the compiler. This conversion
		/// back and forth is normally ok, but certain characters such as single quotes must always be paired. Remove any such characters here.
		/// </summary>
		/// <param name="Value">The macro value</param>
		/// <returns>The sanitized value</returns>
		static string SanitizeMacroValue(string Value)
		{
			StringBuilder Result = new StringBuilder(Value.Length);
			for(int Idx = 0; Idx < Value.Length; Idx++)
			{
				if(Value[Idx] != '\'' && Value[Idx] != '\"')
				{
					Result.Append(Value[Idx]);
				}
			}
			return Result.ToString();
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, List<Action> Actions)
		{
			if (LinkEnvironment.bIsBuildingDotNetAssembly)
			{
				return FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			}

			bool bIsBuildingLibraryOrImportLibrary = LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly;

			// Get link arguments.
			List<string> Arguments = new List<string>();
			if (bIsBuildingLibraryOrImportLibrary)
			{
				AppendLibArguments(LinkEnvironment, Arguments);
			}
			else
			{
				AppendLinkArguments(LinkEnvironment, Arguments);
			}

			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && LinkEnvironment.bPrintTimingInfo)
			{
				Arguments.Add("/time+");
			}

			// If we're only building an import library, add the '/DEF' option that tells the LIB utility
			// to simply create a .LIB file and .EXP file, and don't bother validating imports
			if (bBuildImportLibraryOnly)
			{
				Arguments.Add("/DEF");

				// Ensure that the import library references the correct filename for the linked binary.
				Arguments.Add(String.Format("/NAME:\"{0}\"", LinkEnvironment.OutputFilePath.GetFileName()));

				// Ignore warnings about object files with no public symbols.
				Arguments.Add("/IGNORE:4221");
			}


			if (!bIsBuildingLibraryOrImportLibrary)
			{
				// Delay-load these DLLs.
				foreach (string DelayLoadDLL in LinkEnvironment.DelayLoadDLLs.Distinct())
				{
					Arguments.Add(String.Format("/DELAYLOAD:\"{0}\"", DelayLoadDLL));
				}

				// Pass the module definition file to the linker if we have one
				if (LinkEnvironment.ModuleDefinitionFile != null && LinkEnvironment.ModuleDefinitionFile.Length > 0)
				{
					Arguments.Add(String.Format("/DEF:\"{0}\"", LinkEnvironment.ModuleDefinitionFile));
				}
			}

			// Set up the library paths for linking this binary
			if(bBuildImportLibraryOnly)
			{
				// When building an import library, ignore all the libraries included via embedded #pragma lib declarations. 
				// We shouldn't need them to generate exports.
				Arguments.Add("/NODEFAULTLIB");
			}
			else if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (DirectoryReference LibraryPath in LinkEnvironment.LibraryPaths)
				{
					Arguments.Add(String.Format("/LIBPATH:\"{0}\"", LibraryPath));
				}
				foreach (DirectoryReference LibraryPath in EnvVars.LibraryPaths)
				{
					Arguments.Add(String.Format("/LIBPATH:\"{0}\"", LibraryPath));
				}

				// Add the excluded default libraries to the argument list.
				foreach (string ExcludedLibrary in LinkEnvironment.ExcludedLibraries)
				{
					Arguments.Add(String.Format("/NODEFAULTLIB:\"{0}\"", ExcludedLibrary));
				}
			}

			// Enable function level hot-patching
			if(!bBuildImportLibraryOnly && Target.WindowsPlatform.bCreateHotpatchableImage)
			{
				Arguments.Add("/FUNCTIONPADMIN");
			}

			// For targets that are cross-referenced, we don't want to write a LIB file during the link step as that
			// file will clobber the import library we went out of our way to generate during an earlier step.  This
			// file is not needed for our builds, but there is no way to prevent MSVC from generating it when
			// linking targets that have exports.  We don't want this to clobber our LIB file and invalidate the
			// existing timstamp, so instead we simply emit it with a different name
			FileReference ImportLibraryFilePath = FileReference.Combine(LinkEnvironment.IntermediateDirectory,
														 LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".lib");

			if (LinkEnvironment.bIsCrossReferenced && !bBuildImportLibraryOnly)
			{
				ImportLibraryFilePath = ImportLibraryFilePath.ChangeExtension(".suppressed" + ImportLibraryFilePath.GetExtension());
			}

			FileItem OutputFile;
			if (bBuildImportLibraryOnly)
			{
				OutputFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
			}
			else
			{
				OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			}

			List<FileItem> ProducedItems = new List<FileItem>();
			ProducedItems.Add(OutputFile);

			List<FileItem> PrerequisiteItems = new List<FileItem>();

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				PrerequisiteItems.Add(InputFile);
			}

			if (!bBuildImportLibraryOnly)
			{
				// Add input libraries as prerequisites, too!
				foreach (FileItem InputLibrary in LinkEnvironment.InputLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputLibrary.AbsolutePath));
					PrerequisiteItems.Add(InputLibrary);
				}
			}

			if (!bIsBuildingLibraryOrImportLibrary)
			{
				foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
				{
					InputFileNames.Add(string.Format("\"{0}\"", AdditionalLibrary));

					// If the library file name has a relative path attached (rather than relying on additional
					// lib directories), then we'll add it to our prerequisites list.  This will allow UBT to detect
					// when the binary needs to be relinked because a dependent external library has changed.
					//if( !String.IsNullOrEmpty( Path.GetDirectoryName( AdditionalLibrary ) ) )
					{
						PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
					}
				}
			}

			Arguments.AddRange(InputFileNames);

			// Add the output file to the command-line.
			Arguments.Add(String.Format("/OUT:\"{0}\"", OutputFile.AbsolutePath));

			// For import libraries and exports generated by cross-referenced builds, we don't track output files. VS 15.3+ doesn't touch timestamps for libs
			// and exp files with no modifications, breaking our dependency checking, but incremental linking will fall back to a full link if we delete it.
			// Since all DLLs are typically marked as cross referenced now anyway, we can just ignore this file to allow incremental linking to work.
			if(LinkEnvironment.bHasExports && !LinkEnvironment.bIsBuildingLibrary && !LinkEnvironment.bIsCrossReferenced)
			{
				FileReference ExportFilePath = ImportLibraryFilePath.ChangeExtension(".exp");
				FileItem ExportFile = FileItem.GetItemByFileReference(ExportFilePath);
				ProducedItems.Add(ExportFile);
			}

			if (!bIsBuildingLibraryOrImportLibrary)
			{
				// There is anything to export
				if (LinkEnvironment.bHasExports)
				{
					// Write the import library to the output directory for nFringe support.
					FileItem ImportLibraryFile = FileItem.GetItemByFileReference(ImportLibraryFilePath);
					Arguments.Add(String.Format("/IMPLIB:\"{0}\"", ImportLibraryFilePath));

					// Like the export file above, don't add the import library as a produced item when it's cross referenced.
					if(!LinkEnvironment.bIsCrossReferenced)
					{
					    ProducedItems.Add(ImportLibraryFile);
					}
				}

				if (LinkEnvironment.bCreateDebugInfo)
				{
					// Write the PDB file to the output directory.			
					{
						FileReference PDBFilePath = FileReference.Combine(LinkEnvironment.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
						FileItem PDBFile = FileItem.GetItemByFileReference(PDBFilePath);
						Arguments.Add(String.Format("/PDB:\"{0}\"", PDBFilePath));
						ProducedItems.Add(PDBFile);
					}

					// Write the MAP file to the output directory.			
					if (LinkEnvironment.bCreateMapFile)
					{
						FileReference MAPFilePath = FileReference.Combine(LinkEnvironment.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByFileReference(MAPFilePath);
						Arguments.Add(String.Format("/MAP:\"{0}\"", MAPFilePath));
						ProducedItems.Add(MAPFile);

						// Export a list of object file paths, so we can locate the object files referenced by the map file
						ExportObjectFilePaths(LinkEnvironment, Path.ChangeExtension(MAPFilePath.FullName, ".objpaths"), EnvVars);
					}
				}

				// Add the additional arguments specified by the environment.
				if(!String.IsNullOrEmpty(LinkEnvironment.AdditionalArguments))
				{
					Arguments.Add(LinkEnvironment.AdditionalArguments.Trim());
				}
			}

			// Add any forced references to functions
			foreach(string IncludeFunction in LinkEnvironment.IncludeFunctions)
			{
				if(CppPlatform == CppPlatform.Win32)
				{
					Arguments.Add(String.Format("/INCLUDE:_{0}", IncludeFunction)); // Assume decorated cdecl name
				}
				else
				{
					Arguments.Add(String.Format("/INCLUDE:{0}", IncludeFunction));
				}
			}

			// Create a response file for the linker, unless we're generating IntelliSense data
			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				FileItem ResponseFile = FileItem.CreateIntermediateTextFile(ResponseFileName, String.Join(Environment.NewLine, Arguments));
				PrerequisiteItems.Add(ResponseFile);
			}

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.CommandDescription = "Link";
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
			LinkAction.CommandPath = bIsBuildingLibraryOrImportLibrary ? EnvVars.LibraryManagerPath : EnvVars.LinkerPath;
			LinkAction.CommandArguments = String.Format("@\"{0}\"", ResponseFileName);
			LinkAction.ProducedItems.AddRange(ProducedItems);
			LinkAction.PrerequisiteItems.AddRange(PrerequisiteItems);
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// ensure compiler timings are captured when we execute the action.
			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && LinkEnvironment.bPrintTimingInfo)
			{
				LinkAction.bPrintDebugInfo = true;
			}

			// VS 15.3+ does not touch lib files if they do not contain any modifications, but we need to ensure the timestamps are updated to avoid repeatedly building them.
			if (bBuildImportLibraryOnly || (LinkEnvironment.bHasExports && !bIsBuildingLibraryOrImportLibrary))
			{
				LinkAction.DeleteItems.AddRange(LinkAction.ProducedItems.Where(x => x.Location.HasExtension(".lib") || x.Location.HasExtension(".exp")));
			}

			// Delete PDB files for all produced items, since incremental updates are slower than full ones.
			if (!LinkEnvironment.bUseIncrementalLinking)
			{
				LinkAction.DeleteItems.AddRange(LinkAction.ProducedItems.Where(x => x.Location.HasExtension(".pdb")));
			}

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = bBuildImportLibraryOnly || LinkEnvironment.bIsBuildingDLL;

			// Allow remote linking.  Especially in modular builds with many small DLL files, this is almost always very efficient
			LinkAction.bCanExecuteRemotely = true;
			Actions.Add(LinkAction);

			Log.TraceVerbose("     Linking: " + LinkAction.StatusDescription);
			Log.TraceVerbose("     Command: " + LinkAction.CommandArguments);

			return OutputFile;
		}

		private void ExportObjectFilePaths(LinkEnvironment LinkEnvironment, string FileName, VCEnvironment EnvVars)
		{
			// Write the list of object file directories
			HashSet<DirectoryReference> ObjectFileDirectories = new HashSet<DirectoryReference>();
			foreach(FileItem InputFile in LinkEnvironment.InputFiles)
			{
				ObjectFileDirectories.Add(InputFile.Location.Directory);
			}
			foreach(FileItem InputLibrary in LinkEnvironment.InputLibraries)
			{
				ObjectFileDirectories.Add(InputLibrary.Location.Directory);
			}
			foreach(string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				// Need to handle import libraries that are about to be built (but may not exist yet), third party libraries with relative paths in the UE4 tree, and system libraries in the system path
				FileReference AdditionalLibraryLocation = new FileReference(AdditionalLibrary);
				if(Path.IsPathRooted(AdditionalLibrary) || FileReference.Exists(AdditionalLibraryLocation))
				{
					ObjectFileDirectories.Add(AdditionalLibraryLocation.Directory);
				}
			}
			foreach(DirectoryReference LibraryPath in LinkEnvironment.LibraryPaths)
			{
				ObjectFileDirectories.Add(LibraryPath);
			}
			foreach(string LibraryPath in (Environment.GetEnvironmentVariable("LIB") ?? "").Split(new char[]{ ';' }, StringSplitOptions.RemoveEmptyEntries))
			{
				ObjectFileDirectories.Add(new DirectoryReference(LibraryPath));
			}
			foreach (DirectoryReference LibraryPath in EnvVars.LibraryPaths)
			{
				ObjectFileDirectories.Add(LibraryPath);
			}
			Directory.CreateDirectory(Path.GetDirectoryName(FileName));
			File.WriteAllLines(FileName, ObjectFileDirectories.Select(x => x.FullName).OrderBy(x => x).ToArray());
		}

		/// <summary>
		/// Gets the default include paths for the given platform.
		/// </summary>
		public static string GetVCIncludePaths(CppPlatform Platform, WindowsCompiler Compiler, string CompilerVersion)
		{
			Debug.Assert(Platform == CppPlatform.Win32 || Platform == CppPlatform.Win64);

			// Make sure we've got the environment variables set up for this target
			VCEnvironment EnvVars = VCEnvironment.Create(Compiler, Platform, CompilerVersion, null);

			// Also add any include paths from the INCLUDE environment variable.  MSVC is not necessarily running with an environment that
			// matches what UBT extracted from the vcvars*.bat using SetEnvironmentVariablesFromBatchFile().  We'll use the variables we
			// extracted to populate the project file's list of include paths
			// @todo projectfiles: Should we only do this for VC++ platforms?
			StringBuilder IncludePaths = new StringBuilder();
			foreach(DirectoryReference IncludePath in EnvVars.IncludePaths)
			{
				IncludePaths.AppendFormat("{0};", IncludePath);
			}
			return IncludePaths.ToString();
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			if (Binary.Type == UEBuildBinaryType.DynamicLinkLibrary)
			{
				BuildProducts.Add(FileReference.Combine(Binary.IntermediateDirectory, Binary.OutputFilePath.GetFileNameWithoutExtension() + ".lib"), BuildProductType.BuildResource);
			}
			if(Binary.Type == UEBuildBinaryType.Executable && Target.bCreateMapFile)
			{
				foreach(FileReference OutputFilePath in Binary.OutputFilePaths)
				{
					BuildProducts.Add(FileReference.Combine(OutputFilePath.Directory, OutputFilePath.GetFileNameWithoutExtension() + ".map"), BuildProductType.MapFile);
					BuildProducts.Add(FileReference.Combine(OutputFilePath.Directory, OutputFilePath.GetFileNameWithoutExtension() + ".objpaths"), BuildProductType.MapFile);
				}
			}
		}
    }
}
