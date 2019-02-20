// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Option flags for the Linux toolchain
	/// </summary>
	[Flags]
	enum LinuxToolChainOptions
	{
		/// <summary>
		/// No custom options
		/// </summary>
		None = 0,

		/// <summary>
		/// Enable address sanitzier
		/// </summary>
		EnableAddressSanitizer = 0x1,

		/// <summary>
		/// Enable thread sanitizer
		/// </summary>
		EnableThreadSanitizer = 0x2,

		/// <summary>
		/// Enable undefined behavior sanitizer
		/// </summary>
		EnableUndefinedBehaviorSanitizer = 0x4,
	}

	class LinuxToolChain : UEToolChain
	{
		/** Flavor of the current build (target triplet)*/
		string Architecture;

		/** Cache to avoid making multiple checks for lld availability/usability */
		bool bUseLld = false;

		/** Whether the compiler is set up to produce PIE executables by default */
		bool bSuppressPIE = false;

		/** Whether or not to preserve the portable symbol file produced by dump_syms */
		bool bPreservePSYM = false;

		/** Platform SDK to use */
		protected LinuxPlatformSDK PlatformSDK;

		/** Toolchain information to print during the build. */
		protected string ToolchainInfo;

		/// <summary>
		/// Whether to compile with ASan enabled
		/// </summary>
		LinuxToolChainOptions Options;

		public LinuxToolChain(string InArchitecture, LinuxPlatformSDK InSDK, bool InPreservePSYM = false, LinuxToolChainOptions InOptions = LinuxToolChainOptions.None)
			: this(CppPlatform.Linux, InArchitecture, InSDK, InPreservePSYM, InOptions)
		{
			MultiArchRoot = PlatformSDK.GetSDKLocation();
			BaseLinuxPath = PlatformSDK.GetBaseLinuxPathForArchitecture(InArchitecture);

			bool bCanUseSystemCompiler = PlatformSDK.CanUseSystemCompiler();
			bool bHasValidCompiler = false;

			// First validate the BaseLinuxPath toolchain.

			if (!bCanUseSystemCompiler)
			{
				// don't register if BaseLinuxPath is not specified and cannot use the system compiler
				if (String.IsNullOrEmpty(BaseLinuxPath))
				{
					throw new BuildException("LINUX_ROOT environment variable is not set; cannot instantiate Linux toolchain");
				}
			}

			// these are supplied by the engine and do not change depending on the circumstances
			DumpSymsPath = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Binaries", "Linux", "dump_syms");
			BreakpadEncoderPath = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Binaries", "Linux", "BreakpadSymbolEncoder");

			if (!String.IsNullOrEmpty(BaseLinuxPath))
			{
				if (String.IsNullOrEmpty(MultiArchRoot)) 
				{
					MultiArchRoot = BaseLinuxPath;
					Log.TraceInformation("Using LINUX_ROOT (deprecated, consider LINUX_MULTIARCH_ROOT)");
				}

				BaseLinuxPath = BaseLinuxPath.Replace("\"", "").Replace('\\', '/');

				// set up the path to our toolchain
				GCCPath = "";
				// we rely on the fact that appending ".exe" is optional when invoking a binary on Windows
				ClangPath = Path.Combine(BaseLinuxPath, @"bin", "clang++");
				ArPath = Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, "ar")));
				LlvmArPath = Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}", "llvm-ar")));
				RanlibPath = Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, "ranlib")));
				StripPath = Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, "strip")));
				ObjcopyPath = Path.Combine(Path.Combine(BaseLinuxPath, String.Format("bin/{0}-{1}", Architecture, "objcopy")));

				// When cross-compiling on Windows, use old FixDeps. It is slow, but it does not have timing issues
				bUseFixdeps = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32);

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Environment.SetEnvironmentVariable("LC_ALL", "C");
				}

				bIsCrossCompiling = true;

				bHasValidCompiler = DetermineCompilerVersion();
			}

			// Now validate the system toolchain.

			if (bCanUseSystemCompiler && !bHasValidCompiler)
			{
				BaseLinuxPath = "";
				MultiArchRoot = "";

				ToolchainInfo = "system toolchain";

				// use native linux toolchain
				ClangPath = LinuxCommon.WhichClang();
				GCCPath = LinuxCommon.WhichGcc();
				ArPath = LinuxCommon.Which("ar");
				LlvmArPath = LinuxCommon.Which("llvm-ar");
				RanlibPath = LinuxCommon.Which("ranlib");
				StripPath = LinuxCommon.Which("strip");
				ObjcopyPath = LinuxCommon.Which("objcopy");

				// if clang is available, zero out gcc (@todo: support runtime switching?)
				if (!String.IsNullOrEmpty(ClangPath))
				{
					GCCPath = null;
				}

				// When compiling on Linux, use a faster way to relink circularly dependent libraries.
				// Race condition between actions linking to the .so and action overwriting it is avoided thanks to inodes
				bUseFixdeps = false;

				bIsCrossCompiling = false;

				bHasValidCompiler = DetermineCompilerVersion();
			}
			else
			{
				ToolchainInfo = String.Format("toolchain located at '{0}'", BaseLinuxPath);
			}

			if (!bHasValidCompiler)
			{
				throw new BuildException("Could not determine version of the compiler, not registering Linux toolchain.");
			}

			CheckDefaultCompilerSettings();

			// refuse to use compilers that we know won't work
			// disable that only if you are a dev and you know what you are doing
			if (!UsingClang())
			{
				throw new BuildException("Unable to build: no compatible clang version found. Please run Setup.sh");
			}
			// prevent unknown clangs since the build is likely to fail on too old or too new compilers
			else if ((CompilerVersionMajor * 10 + CompilerVersionMinor) > 70 || (CompilerVersionMajor * 10 + CompilerVersionMinor) < 60)
			{
				throw new BuildException(
					string.Format("This version of the Unreal Engine can only be compiled with clang 7.0 and 6.0. clang {0} may not build it - please use a different version.",
						CompilerVersionString)
					);
			}

			// trust lld only for clang 5.x and above (FIXME: also find if present on the system?)
			// NOTE: with early version you can run into errors like "failed to compute relocation:" and others
			bUseLld = (CompilerVersionMajor >= 5);
		}

		public LinuxToolChain(CppPlatform InCppPlatform, string InArchitecture, LinuxPlatformSDK InSDK, bool InPreservePSYM = false, LinuxToolChainOptions InOptions = LinuxToolChainOptions.None)
			: base(InCppPlatform)
		{
			Architecture = InArchitecture;
			PlatformSDK = InSDK;
			Options = InOptions;
			bPreservePSYM = InPreservePSYM;
		}

		protected virtual bool CrossCompiling()
		{
			return bIsCrossCompiling;
		}


		protected virtual bool UsingClang()
		{
			return !String.IsNullOrEmpty(ClangPath);
		}

		/// <summary>
		/// Splits compiler version string into numerical components, leaving unchanged if not known
		/// </summary>
		private void DetermineCompilerMajMinPatchFromVersionString()
		{
			string[] Parts = CompilerVersionString.Split('.');
			if (Parts.Length >= 1)
			{
				CompilerVersionMajor = Convert.ToInt32(Parts[0]);
			}
			if (Parts.Length >= 2)
			{
				CompilerVersionMinor = Convert.ToInt32(Parts[1]);
			}
			if (Parts.Length >= 3)
			{
				CompilerVersionPatch = Convert.ToInt32(Parts[2]);
			}
		}

		internal string GetDumpEncodeDebugCommand(LinkEnvironment LinkEnvironment, FileItem OutputFile)
		{
			bool bUseCmdExe = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32;
			string DumpCommand = bUseCmdExe ? "\"{0}\" \"{1}\" \"{2}\" 2>NUL\n" : "\"{0}\" -c -o \"{2}\" \"{1}\"\n";
			FileItem EncodedBinarySymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".sym"));
			FileItem SymbolsFile  = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, OutputFile.Location.GetFileName() + ".psym"));
			FileItem StrippedFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, OutputFile.Location.GetFileName() + "_nodebug"));
			FileItem DebugFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".debug"));

			if (bPreservePSYM)
			{
				SymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".psym"));
			}

			string Out = "";

			// dump_syms
			Out += string.Format(DumpCommand,
				DumpSymsPath,
				OutputFile.AbsolutePath,
				SymbolsFile.AbsolutePath
			);

			// encode breakpad symbols
			Out += string.Format("\"{0}\" \"{1}\" \"{2}\"\n",
				BreakpadEncoderPath,
				SymbolsFile.AbsolutePath,
				EncodedBinarySymbolsFile.AbsolutePath
			);

			if (LinkEnvironment.bCreateDebugInfo)
			{
				if (bUseCmdExe)
				{
					// Bad hack where objcopy.exe cannot handle files larger then 2GB. Its fine when building on Linux
					Out += string.Format("for /F \"tokens=*\" %%F in (\"{0}\") DO set size=%%~zF\n",
						OutputFile.AbsolutePath
					);

					// If we are less then 2GB create the debugging info
					Out += "if %size% LSS 2147483648 (\n";
				}

				// objcopy stripped file
				Out += string.Format("\"{0}\" --strip-all \"{1}\" \"{2}\"\n",
					GetObjcopyPath(LinkEnvironment.Architecture),
					OutputFile.AbsolutePath,
					StrippedFile.AbsolutePath
				);

				// objcopy debug file
				Out += string.Format("\"{0}\" --only-keep-debug \"{1}\" \"{2}\"\n",
					GetObjcopyPath(LinkEnvironment.Architecture),
					OutputFile.AbsolutePath,
					DebugFile.AbsolutePath
				);

				// objcopy link debug file to final so
				Out += string.Format("\"{0}\" --add-gnu-debuglink=\"{1}\" \"{2}\" \"{3}.temp\"\n",
					GetObjcopyPath(LinkEnvironment.Architecture),
					DebugFile.AbsolutePath,
					StrippedFile.AbsolutePath,
					OutputFile.AbsolutePath
				);

				if (bUseCmdExe)
				{
					// Only move the temp final elf file once its done being linked by objcopy
					Out += string.Format("move /Y \"{0}.temp\" \"{1}\"\n",
						OutputFile.AbsolutePath,
						OutputFile.AbsolutePath
					);

					// If our file is greater then 4GB we'll have to create a debug file anyway
					Out += string.Format(") ELSE (\necho DummyDebug >> {0}\n)\n",
						DebugFile.AbsolutePath
					);
				}
				else
				{
					// Only move the temp final elf file once its done being linked by objcopy
					Out += string.Format("mv \"{0}.temp\" \"{1}\"\n",
						OutputFile.AbsolutePath,
						OutputFile.AbsolutePath
					);

					// Change the debug file to normal permissions. It was taking on the +x rights from the output file
					Out += string.Format("chmod 644 \"{0}\"\n",
						DebugFile.AbsolutePath
					);
				}
			}
			else
			{
				// strip the final elf file if we are not producing debug info
				Out += string.Format("\"{0}\" \"{1}\"\n",
					GetStripPath(LinkEnvironment.Architecture),
					OutputFile.AbsolutePath
				);
			}

			return Out;
		}

		/// <summary>
		/// Queries compiler for the version
		/// </summary>
		protected bool DetermineCompilerVersion()
		{
			CompilerVersionString = null;
			CompilerVersionMajor = -1;
			CompilerVersionMinor = -1;
			CompilerVersionPatch = -1;

			using (Process Proc = new Process())
			{
				Proc.StartInfo.UseShellExecute = false;
				Proc.StartInfo.CreateNoWindow = true;
				Proc.StartInfo.RedirectStandardOutput = true;
				Proc.StartInfo.RedirectStandardError = true;

				if (!String.IsNullOrEmpty(GCCPath))
				{
					Proc.StartInfo.FileName = GCCPath;
					Proc.StartInfo.Arguments = " -dumpversion";

					Proc.Start();
					Proc.WaitForExit();

					if (Proc.ExitCode == 0)
					{
						// read just the first string
						CompilerVersionString = Proc.StandardOutput.ReadLine();
						DetermineCompilerMajMinPatchFromVersionString();
					}
				}
				else if (!String.IsNullOrEmpty(ClangPath))
				{
					Proc.StartInfo.FileName = ClangPath;
					Proc.StartInfo.Arguments = " --version";

					Proc.Start();
					Proc.WaitForExit();

					if (Proc.ExitCode == 0)
					{
						// read just the first string
						string VersionString = Proc.StandardOutput.ReadLine();

						Regex VersionPattern = new Regex("version \\d+(\\.\\d+)+");
						Match VersionMatch = VersionPattern.Match(VersionString);

						// version match will be like "version 3.3", so remove the "version"
						if (VersionMatch.Value.StartsWith("version "))
						{
							CompilerVersionString = VersionMatch.Value.Replace("version ", "");

							DetermineCompilerMajMinPatchFromVersionString();
						}
					}
				}
				else
				{
					// icl?
				}
			}

			return !String.IsNullOrEmpty(CompilerVersionString);
		}

		/// <summary>
		/// Checks default compiler settings
		/// </summary>
		private void CheckDefaultCompilerSettings()
		{
			using (Process Proc = new Process())
			{
				Proc.StartInfo.UseShellExecute = false;
				Proc.StartInfo.CreateNoWindow = true;
				Proc.StartInfo.RedirectStandardOutput = true;
				Proc.StartInfo.RedirectStandardError = true;
				Proc.StartInfo.RedirectStandardInput = true;

				if (!String.IsNullOrEmpty(ClangPath) && File.Exists(ClangPath))
				{
					Proc.StartInfo.FileName = ClangPath;
					Proc.StartInfo.Arguments = " -E -dM -";

					Proc.Start();
					Proc.StandardInput.Close();

					for (; ; )
					{
						string CompilerDefine = Proc.StandardOutput.ReadLine();
						if (string.IsNullOrEmpty(CompilerDefine))
						{
							Proc.WaitForExit();
							break;
						}

						if (CompilerDefine.Contains("__PIE__") || CompilerDefine.Contains("__pie__"))
						{
							bSuppressPIE = true;
						}
					}
				}
				else
				{
					// other compilers aren't implemented atm
				}
			}
		}

		/// <summary>
		/// Checks if compiler version matches the requirements
		/// </summary>
		private static bool CompilerVersionGreaterOrEqual(int Major, int Minor, int Patch)
		{
			return CompilerVersionMajor > Major ||
				(CompilerVersionMajor == Major && CompilerVersionMinor > Minor) ||
				(CompilerVersionMajor == Major && CompilerVersionMinor == Minor && CompilerVersionPatch >= Patch);
		}

		/// <summary>
		/// Architecture-specific compiler switches
		/// </summary>
		static string ArchitectureSpecificSwitches(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("arm") || Architecture.StartsWith("aarch64"))
			{
				Result += " -fsigned-char";
			}

			return Result;
		}

		protected virtual string ArchitectureSpecificDefines(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64"))
			{
				Result += " -D_LINUX64";
			}

			return Result;
		}

		/// <summary>
		/// Gets architecture-specific ar paths
		/// </summary>
		protected virtual string GetArPath(string Architecture)
		{
			return ArPath;
		}

		/// <summary>
		/// Gets architecture-specific ranlib paths
		/// </summary>
		protected virtual string GetRanlibPath(string Architecture)
		{
			return RanlibPath;
		}

		/// <summary>
		/// Gets architecture-specific strip path
		/// </summary>
		protected virtual string GetStripPath(string Architecture)
		{
			return StripPath;
		}

		/// <summary>
		/// Gets architecture-specific objcopy path
		/// </summary>
		protected virtual string GetObjcopyPath(string Architecture)
		{
			return ObjcopyPath;
		}

		private static bool ShouldUseLibcxx(string Architecture)
		{
			// set UE4_LINUX_USE_LIBCXX to either 0 or 1. If unset, defaults to 1.
			string UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE4_LINUX_USE_LIBCXX");
			if (string.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
			{
				// at the moment ARM32 libc++ remains missing
				return Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64") || Architecture.StartsWith("i686");
			}
			return false;
		}

		protected virtual string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			// build up the commandline common to C and C++
			Result += " -c";
			Result += " -pipe";

			if (ShouldUseLibcxx(CompileEnvironment.Architecture))
			{
				Result += " -nostdinc++";
				Result += " -I" + "ThirdParty/Linux/LibCxx/include/";
				Result += " -I" + "ThirdParty/Linux/LibCxx/include/c++/v1";
			}

			// ASan
			if (Options.HasFlag(LinuxToolChainOptions.EnableAddressSanitizer))
			{
				Result += " -fsanitize=address";
			}

			// TSan
			if (Options.HasFlag(LinuxToolChainOptions.EnableThreadSanitizer))
			{
				Result += " -fsanitize=thread";
			}

			// UBSan
			if (Options.HasFlag(LinuxToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Result += " -fsanitize=undefined";
			}

			Result += " -Wall -Werror";

			if (!CompileEnvironment.Architecture.StartsWith("x86_64") && !CompileEnvironment.Architecture.StartsWith("i686"))
			{
				Result += " -funwind-tables";               // generate unwind tables as they are needed for backtrace (on x86(64) they are generated implicitly)
			}

			Result += " -Wsequence-point";              // additional warning not normally included in Wall: warns if order of operations is ambigious
			//Result += " -Wunreachable-code";            // additional warning not normally included in Wall: warns if there is code that will never be executed - not helpful due to bIsGCC and similar
			//Result += " -Wshadow";                      // additional warning not normally included in Wall: warns if there variable/typedef shadows some other variable - not helpful because we have gobs of code that shadows variables
			Result += " -Wdelete-non-virtual-dtor";

			Result += ArchitectureSpecificSwitches(CompileEnvironment.Architecture);

			Result += " -fno-math-errno";               // do not assume that math ops have side effects

			Result += GetRTTIFlag(CompileEnvironment);	// flag for run-time type info

			if (CompileEnvironment.bHideSymbolsByDefault)
			{
				Result += " -fvisibility=hidden";
				Result += " -fvisibility-inlines-hidden";
			}

			if (String.IsNullOrEmpty(ClangPath))
			{
				// GCC only option
				Result += " -fno-strict-aliasing";
				Result += " -Wno-sign-compare"; // needed to suppress: comparison between signed and unsigned integer expressions
				Result += " -Wno-enum-compare"; // Stats2.h triggers this (alignof(int64) <= DATA_ALIGN)
				Result += " -Wno-return-type"; // Variant.h triggers this
				Result += " -Wno-unused-local-typedefs";
				Result += " -Wno-multichar";
				Result += " -Wno-unused-but-set-variable";
				Result += " -Wno-strict-overflow"; // Array.h:518
			}
			else
			{
				// Clang only options
				if (CrossCompiling())
				{
					if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32)
					{
						Result += " -fdiagnostics-format=msvc";     // make diagnostics compatible with MSVC when cross-compiling
					}
					else if (Log.ColorConsoleOutput)
					{
						Result += " -fcolor-diagnostics";
					}
				}
				Result += " -Wno-unused-private-field";     // MultichannelTcpSocket.h triggers this, possibly more
				// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
				Result += " -Wno-tautological-compare";

				// this switch is understood by clang 3.5.0, but not clang-3.5 as packaged by Ubuntu 14.04 atm
				if (CompilerVersionGreaterOrEqual(3, 5, 0))
				{
					Result += " -Wno-undefined-bool-conversion";	// hides checking if 'this' pointer is null
				}

				if (CompilerVersionGreaterOrEqual(3, 6, 0))
				{
					Result += " -Wno-unused-local-typedef";	// clang is being overly strict here? PhysX headers trigger this.
					Result += " -Wno-inconsistent-missing-override";	// these have to be suppressed for UE 4.8, should be fixed later.
				}

				if (CompilerVersionGreaterOrEqual(3, 9, 0))
				{
					Result += " -Wno-undefined-var-template"; // not really a good warning to disable
				}

				if (CompilerVersionGreaterOrEqual(5, 0, 0))
				{
					Result += " -Wno-unused-lambda-capture";  // suppressed because capturing of compile-time constants is seemingly inconsistent. And MSVC doesn't do that.
				}
			}

			Result += " -Wno-unused-variable";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			Result += " -Wno-unknown-pragmas";			// Slate triggers this (with its optimize on/off pragmas)
			// needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-invalid-offsetof";
			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			// Whether we actually can enable that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (CompileEnvironment.bPGOOptimize)
			{
				//
				// Clang emits a warning for each compiled function that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable this warning. It's far too verbose.
				//
				Result += " -Wno-backend-plugin";

				Log.TraceInformationOnce("Enabling Profile Guided Optimization (PGO). Linking will take a while.");
				Result += string.Format(" -fprofile-instr-use=\"{0}\"", Path.Combine(CompileEnvironment.PGODirectory, CompileEnvironment.PGOFilenamePrefix));
			}
			else if (CompileEnvironment.bPGOProfile)
			{
				Log.TraceInformationOnce("Enabling Profile Guided Instrumentation (PGI). Linking will take a while.");
				Result += " -fprofile-generate";
			}

			// Unlike on other platforms, allow LTO be specified independently of PGO
			// Whether we actually can enable that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (CompileEnvironment.bAllowLTCG)
			{
				Result += " -flto";
			}

			if (CompileEnvironment.bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			//Result += " -DOPERATOR_NEW_INLINE=FORCENOINLINE";

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Configuration == CppConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
				Result += " -fomit-frame-pointer";
			}
			// switches to help debugging
			else if (CompileEnvironment.Configuration == CppConfiguration.Debug)
			{
				Result += " -fno-inline";                   // disable inlining for better debuggability (e.g. callstacks, "skip file" in gdb)
				Result += " -fno-omit-frame-pointer";       // force not omitting fp
				Result += " -fstack-protector";             // detect stack smashing
				//Result += " -fsanitize=address";            // detect address based errors (support properly and link to libasan)
			}

			// debug info
			// bCreateDebugInfo is normally set for all configurations, including Shipping - this is needed to enable callstack in Shipping builds (proper resolution: UEPLAT-205, separate files with debug info)
			if (CompileEnvironment.bCreateDebugInfo)
			{
				// libdwarf (from elftoolchain 0.6.1) doesn't support DWARF4. If we need to go back to depending on elftoolchain revert this back to dwarf-3
				Result += " -gdwarf-4";

				// Make debug info LLDB friendly
				Result += " -glldb";
			}

			// optimization level
			if (!CompileEnvironment.bOptimizeCode)
			{
				Result += " -O0";
			}
			else
			{
				// Don't over optimise if using AddressSanitizer or you'll get false positive errors due to erroneous optimisation of necessary AddressSanitizer instrumentation.
				if (Options.HasFlag(LinuxToolChainOptions.EnableAddressSanitizer))
				{
					Result += " -O1 -g -fno-optimize-sibling-calls -fno-omit-frame-pointer";
				}
				else if (Options.HasFlag(LinuxToolChainOptions.EnableThreadSanitizer))
				{
					Result += " -O1 -g";
				}
				else
				{
					Result += " -O2";	// warning: as of now (2014-09-28), clang 3.5.0 miscompiles PlatformerGame with -O3 (bitfields?)
				}
			}

			if (!CompileEnvironment.bUseInlining)
			{
				Result += " -fno-inline-functions";
			}

			if (CompileEnvironment.bIsBuildingDLL)
			{
				Result += " -fPIC";
				// Use local-dynamic TLS model. This generates less efficient runtime code for __thread variables, but avoids problems of running into
				// glibc/ld.so limit (DTV_SURPLUS) for number of dlopen()'ed DSOs with static TLS (see e.g. https://www.cygwin.com/ml/libc-help/2013-11/msg00033.html)
				Result += " -ftls-model=local-dynamic";
			}

			if (CompileEnvironment.bEnableExceptions)
			{
				Result += " -fexceptions";
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=0";
			}
			else
			{
				Result += " -fno-exceptions";               // no exceptions
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=1";
			}

			if (bSuppressPIE && !CompileEnvironment.bIsBuildingDLL)
			{
				Result += " -fno-PIE";
			}

			if (PlatformSDK.bVerboseCompiler)
			{
				Result += " -v";                            // for better error diagnosis
			}

			Result += ArchitectureSpecificDefines(CompileEnvironment.Architecture);
			if (CrossCompiling())
			{
				if (UsingClang() && !string.IsNullOrEmpty(CompileEnvironment.Architecture))
				{
					Result += String.Format(" -target {0}", CompileEnvironment.Architecture);        // Set target triple
				}
				Result += String.Format(" --sysroot=\"{0}\"", BaseLinuxPath);
			}

			return Result;
		}

		/// <summary>
		/// Sanitizes a definition argument if needed.
		/// </summary>
		/// <param name="definition">A string in the format "foo=bar".</param>
		/// <returns></returns>
		internal static string EscapeArgument(string definition)
		{
			string[] splitData = definition.Split('=');
			string myKey = splitData.ElementAtOrDefault(0);
			string myValue = splitData.ElementAtOrDefault(1);

			if (string.IsNullOrEmpty(myKey)) { return ""; }
			if (!string.IsNullOrEmpty(myValue))
			{
				if (!myValue.StartsWith("\"") && (myValue.Contains(" ") || myValue.Contains("$")))
				{
					myValue = myValue.Trim('\"');		// trim any leading or trailing quotes
					myValue = "\"" + myValue + "\"";	// ensure wrap string with double quotes
				}

				// replace double quotes to escaped double quotes if exists
				myValue = myValue.Replace("\"", "\\\"");
			}

			return myValue == null
				? string.Format("{0}", myKey)
				: string.Format("{0}={1}", myKey, myValue);
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x c++";
			Result += " -std=c++14";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
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

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++14";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x c++-header";
			Result += " -std=c++14";
			return Result;
		}

		protected virtual string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			if (UsingLld(LinkEnvironment.Architecture) && !LinkEnvironment.bIsBuildingDLL)
			{
				Result += (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) ? " -fuse-ld=lld.exe" : " -fuse-ld=lld";
			}

			// debugging symbols
			// Applying to all configurations @FIXME: temporary hack for FN to enable callstack in Shipping builds (proper resolution: UEPLAT-205)
			Result += " -rdynamic";   // needed for backtrace_symbols()...

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Result += " -shared";
			}
			else
			{
				// ignore unresolved symbols in shared libs
				Result += string.Format(" -Wl,--unresolved-symbols=ignore-in-shared-libs");
			}

			if (Options.HasFlag(LinuxToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(LinuxToolChainOptions.EnableThreadSanitizer) || Options.HasFlag(LinuxToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Result += " -g";
				if (Options.HasFlag(LinuxToolChainOptions.EnableAddressSanitizer))
				{
					Result += " -fsanitize=address";
				}
				else if (Options.HasFlag(LinuxToolChainOptions.EnableThreadSanitizer))
				{
					Result += " -fsanitize=thread";
				}
				else if (Options.HasFlag(LinuxToolChainOptions.EnableUndefinedBehaviorSanitizer))
				{
					Result += " -fsanitize=undefined";
				}
			}

			// RPATH for third party libs
			Result += " -Wl,-rpath=${ORIGIN}";
			Result += " -Wl,-rpath-link=${ORIGIN}";
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/Linux";
			Result += " -Wl,-rpath=${ORIGIN}/..";	// for modules that are in sub-folders of the main Engine/Binary/Linux folder
			// FIXME: really ugly temp solution. Modules need to be able to specify this
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Steamworks/Steamv139/x86_64-unknown-linux-gnu";
			if (LinkEnvironment.Architecture.StartsWith("x86_64"))
			{
				Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Qualcomm/Linux";
			}
			else
			{
				// x86_64 is now using updated ICU that doesn't need extra .so
				Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Linux/" + LinkEnvironment.Architecture;
			}
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/OpenVR/OpenVRv1_0_16/linux64";

			// @FIXME: Workaround for generating RPATHs for launching on devices UE-54136
			Result += " -Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/PhysX3/Linux/x86_64-unknown-linux-gnu";

			// Some OS ship ld with new ELF dynamic tags, which use DT_RUNPATH vs DT_RPATH. Since DT_RUNPATH do not propagate to dlopen()ed DSOs,
			// this breaks the editor on such systems. See https://kenai.com/projects/maxine/lists/users/archive/2011-01/message/12 for details
			Result += " -Wl,--disable-new-dtags";

			// This severely improves runtime linker performance. Without using FixDeps the impact on link time is not as big.
			Result += " -Wl,--as-needed";

			// Additionally speeds up editor startup by 1-2s
			Result += " -Wl,--hash-style=gnu";

			// This apparently can help LLDB speed up symbol lookups
			Result += " -Wl,--build-id";
			if (bSuppressPIE && !LinkEnvironment.bIsBuildingDLL)
			{
				Result += " -Wl,-nopie";
			}

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			// Whether we actually can enable that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (LinkEnvironment.bPGOOptimize)
			{
				//
				// Clang emits a warning for each compiled function that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable this warning. It's far too verbose.
				//
				Result += " -Wno-backend-plugin";

				Log.TraceInformationOnce("Enabling Profile Guided Optimization (PGO). Linking will take a while.");
				Result += string.Format(" -fprofile-instr-use=\"{0}\"", Path.Combine(LinkEnvironment.PGODirectory, LinkEnvironment.PGOFilenamePrefix));
			}
			else if (LinkEnvironment.bPGOProfile)
			{
				Log.TraceInformationOnce("Enabling Profile Guided Instrumentation (PGI). Linking will take a while.");
				Result += " -fprofile-generate";
			}

			// whether we actually can do that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (LinkEnvironment.bAllowLTCG)
			{
				Result += " -flto";
			}

			if (CrossCompiling())
			{
				if (UsingClang())
				{
					Result += String.Format(" -target {0}", LinkEnvironment.Architecture);        // Set target triple
				}
				string SysRootPath = BaseLinuxPath.TrimEnd(new char[] { '\\', '/' });
				Result += String.Format(" \"--sysroot={0}\"", SysRootPath);

				// Linking with the toolchain on linux appears to not search usr/
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Result += String.Format(" -B{0}/usr/lib/", SysRootPath);
					Result += String.Format(" -B{0}/usr/lib64/", SysRootPath);
					Result += String.Format(" -L{0}/usr/lib/", SysRootPath);
					Result += String.Format(" -L{0}/usr/lib64/", SysRootPath);
				}
			}

			return Result;
		}

		string GetArchiveArguments(LinkEnvironment LinkEnvironment)
		{
			return " rcs";
		}

		// cache the location of NDK tools
		protected bool bIsCrossCompiling;
		protected string BaseLinuxPath;
		protected string ClangPath;
		protected string GCCPath;
		protected string ArPath;
		protected string LlvmArPath;
		protected string RanlibPath;
		protected string StripPath;
		protected string ObjcopyPath;
		protected string DumpSymsPath;
		protected string BreakpadEncoderPath;
		protected string MultiArchRoot;

		/// <summary>
		/// Version string of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static string CompilerVersionString;
		/// <summary>
		/// Major version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionMajor = -1;
		/// <summary>
		/// Minor version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionMinor = -1;
		/// <summary>
		/// Patch version of the current compiler, whether clang or gcc or whatever
		/// </summary>
		static int CompilerVersionPatch = -1;

		/// <summary>
		/// Whether to use old, slower way to relink circularly dependent libraries.
		/// It makes sense to use it when cross-compiling on Windows due to race conditions between actions reading and modifying the libs.
		/// </summary>
		private bool bUseFixdeps = false;

		/// <summary>
		/// Track which scripts need to be deleted before appending to
		/// </summary>
		private bool bHasWipedFixDepsScript = false;

		/// <summary>
		/// Holds all the binaries for a particular target (except maybe the executable itself).
		/// </summary>
		private static List<FileItem> AllBinaries = new List<FileItem>();

		/// <summary>
		/// Tracks that information about used C++ library is only printed once
		/// </summary>
		private bool bHasPrintedBuildDetails = false;

		/// <summary>
		/// Checks if we actually can use LTO/PGO with this set of tools
		/// </summary>
		private bool CanUseAdvancedLinkerFeatures(string Architecture)
		{
			return UsingLld(Architecture) && !String.IsNullOrEmpty(LlvmArPath);
		}

		/// <summary>
		/// Returns a helpful string for the user
		/// </summary>
		protected string ExplainWhyCannotUseAdvancedLinkerFeatures(string Architecture)
		{
			string Explanation = "Cannot use LTO/PGO on this toolchain:";
			int NumProblems = 0;
			if (!UsingLld(Architecture))
			{
				Explanation += " not using lld";
				++NumProblems;
			}
			if (String.IsNullOrEmpty(LlvmArPath))
			{
				if (NumProblems > 0)
				{
					Explanation += " and";
				}
				Explanation += " llvm-ar was not found";
			}
			return Explanation;
		}

		protected void PrintBuildDetails(CppCompileEnvironment CompileEnvironment)
		{
			Log.TraceInformation("------- Build details --------");
			Log.TraceInformation("Using {0}.", ToolchainInfo);
			Log.TraceInformation("Using {0} ({1}) version '{2}' (string), {3} (major), {4} (minor), {5} (patch)",
				String.IsNullOrEmpty(ClangPath) ? "gcc" : "clang",
				String.IsNullOrEmpty(ClangPath) ? GCCPath : ClangPath,
				CompilerVersionString, CompilerVersionMajor, CompilerVersionMinor, CompilerVersionPatch);

			if (UsingClang())
			{
				// inform the user which C++ library the engine is going to be compiled against - important for compatibility with third party code that uses STL
				Log.TraceInformation("Using {0} standard C++ library.", ShouldUseLibcxx(CompileEnvironment.Architecture) ? "bundled libc++" : "compiler default (most likely libstdc++)");
				Log.TraceInformation("Using {0}", UsingLld(CompileEnvironment.Architecture) ? "lld linker" : "default linker (ld)");
				Log.TraceInformation("Using {0}", !String.IsNullOrEmpty(LlvmArPath) ? String.Format("llvm-ar : {0}", LlvmArPath) : String.Format("ar and ranlib: {0}, {1}", GetArPath(CompileEnvironment.Architecture), GetRanlibPath(CompileEnvironment.Architecture)));
			}

			// Also print other once-per-build information
			if (bUseFixdeps)
			{
				Log.TraceInformation("Using old way to relink circularly dependent libraries (with a FixDeps step).");
			}
			else
			{
				Log.TraceInformation("Using fast way to relink  circularly dependent libraries (no FixDeps).");
			}

			if (CompileEnvironment.bPGOOptimize)
			{
				Log.TraceInformation("Using PGO (profile guided optimization).");
				Log.TraceInformation("  Directory for PGO data files='{0}'", CompileEnvironment.PGODirectory);
				Log.TraceInformation("  Prefix for PGO data files='{0}'", CompileEnvironment.PGOFilenamePrefix);
			}

			if (CompileEnvironment.bPGOProfile)
			{
				Log.TraceInformation("Using PGI (profile guided instrumentation).");
			}

			if (CompileEnvironment.bAllowLTCG)
			{
				Log.TraceInformation("Using LTO (link-time optimization).");
			}

			if (bSuppressPIE)
			{
				Log.TraceInformation("Compiler is set up to generate position independent executables by default, but we're suppressing it.");
			}
			Log.TraceInformation("------------------------------");
		}

		protected bool CheckSDKVersionFromFile(string VersionPath, out string ErrorMessage)
		{
			if (File.Exists(VersionPath))
			{
				StreamReader SDKVersionFile = new StreamReader(VersionPath);
				string SDKVersionString = SDKVersionFile.ReadLine();
				SDKVersionFile.Close();

				if (SDKVersionString != null)
				{
					return PlatformSDK.CheckSDKCompatible(SDKVersionString, out ErrorMessage);
				}
			}

			ErrorMessage = "Cannot use an old toolchain (missing " + PlatformSDK.SDKVersionFileName() + " file, assuming version earlier than v11)";
			return false;
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, List<Action> Actions)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			var BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(CompileEnvironment.Platform);

			if (!bHasPrintedBuildDetails)
			{
				PrintBuildDetails(CompileEnvironment);

				string LinuxDependenciesPath = Path.Combine(UnrealBuildTool.EngineSourceThirdPartyDirectory.FullName, "Linux", PlatformSDK.HaveLinuxDependenciesFile());
				if (!File.Exists(LinuxDependenciesPath))
				{
					throw new BuildException("Please make sure that Engine/Source/ThirdParty/Linux is complete (re - run Setup script if using a github build)");
				}

				if (!String.IsNullOrEmpty(MultiArchRoot))
				{
					string ErrorMessage;
					if (!CheckSDKVersionFromFile(Path.Combine(MultiArchRoot, PlatformSDK.SDKVersionFileName()), out ErrorMessage))
					{
						throw new BuildException(ErrorMessage);
					}
				}

				bHasPrintedBuildDetails = true;
			}

			if ((CompileEnvironment.bAllowLTCG || CompileEnvironment.bPGOOptimize || CompileEnvironment.bPGOProfile) && !CanUseAdvancedLinkerFeatures(CompileEnvironment.Architecture))
			{
				throw new BuildException(ExplainWhyCannotUseAdvancedLinkerFeatures(CompileEnvironment.Architecture));
			}

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				PCHArguments += string.Format(" -include \"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename.FullName.Replace('\\', '/'));
			}

			// Add include paths to the argument list.
			foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath.FullName.Replace('\\', '/'));
			}
			foreach (DirectoryReference IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath.FullName.Replace('\\', '/'));
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D \"{0}\"", EscapeArgument(Definition));
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);

				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				// Add C or C++ specific compiler arguments.
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					FileArguments += GetCompileArguments_PCH();
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
					// Compile the file as Objective-C code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					FileArguments += GetCompileArguments_CPP();

					// only use PCH for .cpp files
					FileArguments += PCHArguments;
				}

				foreach (FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
				{
					FileArguments += String.Format(" -include \"{0}\"", ForceIncludeFile.Location.FullName.Replace('\\', '/'));
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".gch"));

					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath.Replace('\\', '/'));
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

					FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath.Replace('\\', '/'));
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath.Replace('\\', '/'));

				// Generate the included header dependency list
				if(CompileEnvironment.bGenerateDependenciesFile)
				{
					FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".d"));
					FileArguments += string.Format(" -MD -MF\"{0}\"", DependencyListFile.AbsolutePath.Replace('\\', '/'));
					CompileAction.DependencyListFile = DependencyListFile;
					CompileAction.ProducedItems.Add(DependencyListFile);
				}

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				if (!UsingClang())
				{
					CompileAction.CommandPath = new FileReference(GCCPath);
				}
				else
				{
					CompileAction.CommandPath = new FileReference(ClangPath);
				}

				string AllArguments = (Arguments + FileArguments + CompileEnvironment.AdditionalArguments);
				// all response lines should have / instead of \, but we cannot just bulk-replace it here since some \ are used to escape quotes, e.g. Definitions.Add("FOO=TEXT(\"Bar\")");

				Debug.Assert(CompileAction.ProducedItems.Count > 0);

				FileReference CompilerResponseFileName = CompileAction.ProducedItems[0].Location + ".rsp";
				FileItem CompilerResponseFileItem = FileItem.CreateIntermediateTextFile(CompilerResponseFileName, AllArguments);

				CompileAction.CommandArguments = string.Format(" @\"{0}\"", CompilerResponseFileName);
				CompileAction.PrerequisiteItems.Add(CompilerResponseFileItem);
				CompileAction.CommandDescription = "Compile";
				CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
				CompileAction.bIsGCCCompiler = true;

				// Don't farm out creation of pre-compiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					CompileEnvironment.bAllowRemotelyCompiledPCHs;

				Actions.Add(CompileAction);
			}

			return Result;
		}

		bool UsingLld(string Architecture)
		{
			return bUseLld && Architecture.StartsWith("x86_64");
		}

		/// <summary>
		/// Creates an action to archive all the .o files into single .a file
		/// </summary>
		public FileItem CreateArchiveAndIndex(LinkEnvironment LinkEnvironment, List<Action> Actions)
		{
			// Create an archive action
			Action ArchiveAction = new Action(ActionType.Link);
			ArchiveAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
			ArchiveAction.CommandPath = BuildHostPlatform.Current.Shell;

			if (BuildHostPlatform.Current.ShellType == ShellType.Sh)
			{
				ArchiveAction.CommandArguments = "-c '";
			}
			else
			{
				ArchiveAction.CommandArguments = "/c \"";
			}

			// this will produce a final library
			ArchiveAction.bProducesImportLibrary = true;

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			ArchiveAction.ProducedItems.Add(OutputFile);
			ArchiveAction.CommandDescription = "Archive";
			ArchiveAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			ArchiveAction.CommandArguments += string.Format("\"{0}\" {1} \"{2}\"", GetArPath(LinkEnvironment.Architecture), GetArchiveArguments(LinkEnvironment), OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string InputAbsolutePath = InputFile.AbsolutePath.Replace("\\", "/");
				InputFileNames.Add(string.Format("\"{0}\"", InputAbsolutePath));
				ArchiveAction.PrerequisiteItems.Add(InputFile);
			}

			// this won't stomp linker's response (which is not used when compiling static libraries)
			FileReference ResponsePath = GetResponseFileName(LinkEnvironment, OutputFile);
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponsePath, InputFileNames);
				ArchiveAction.PrerequisiteItems.Add(ResponseFileItem);
			}
			ArchiveAction.CommandArguments += string.Format(" @\"{0}\"", ResponsePath.FullName);

			// add ranlib if not using llvm-ar
			if (String.IsNullOrEmpty(LlvmArPath))
			{
				ArchiveAction.CommandArguments += string.Format(" && \"{0}\" \"{1}\"", GetRanlibPath(LinkEnvironment.Architecture), OutputFile.AbsolutePath);
			}

			// Add the additional arguments specified by the environment.
			ArchiveAction.CommandArguments += LinkEnvironment.AdditionalArguments;
			ArchiveAction.CommandArguments = ArchiveAction.CommandArguments.Replace("\\", "/");

			if (BuildHostPlatform.Current.ShellType == ShellType.Sh)
			{
				ArchiveAction.CommandArguments += "'";
			}
			else
			{
				ArchiveAction.CommandArguments += "\"";
			}

			// Only execute linking on the local PC.
			ArchiveAction.bCanExecuteRemotely = false;
			Actions.Add(ArchiveAction);

			return OutputFile;
		}

		public FileItem FixDependencies(LinkEnvironment LinkEnvironment, FileItem Executable, List<Action> Actions)
		{
			if (bUseFixdeps)
			{
				if (!LinkEnvironment.bIsCrossReferenced)
				{
					return null;
				}

				Log.TraceVerbose("Adding postlink step");

				bool bUseCmdExe = BuildHostPlatform.Current.ShellType == ShellType.Cmd;
				FileReference ShellBinary = BuildHostPlatform.Current.Shell;
				string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x
				string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

				FileItem FixDepsScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory, ScriptName));

				Action PostLinkAction = new Action(ActionType.Link);
				PostLinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				PostLinkAction.CommandPath = ShellBinary;
				PostLinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(Executable.AbsolutePath));
				PostLinkAction.CommandDescription = "FixDeps";
				PostLinkAction.bCanExecuteRemotely = false;
				PostLinkAction.CommandArguments = ExecuteSwitch;

				PostLinkAction.CommandArguments += bUseCmdExe ? " \"" : " -c '";

				FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));

				// Make sure we don't run this script until the all executables and shared libraries
				// have been built.
				PostLinkAction.PrerequisiteItems.Add(Executable);
				foreach (FileItem Dependency in AllBinaries)
				{
					PostLinkAction.PrerequisiteItems.Add(Dependency);
				}

				PostLinkAction.CommandArguments += ShellBinary + ExecuteSwitch + " \"" + FixDepsScript.AbsolutePath + "\" && ";

				// output file should not be empty or it will be rebuilt next time
				string Touch = bUseCmdExe ? "echo \"Dummy\" >> \"{0}\" && copy /b \"{0}\" +,," : "echo \"Dummy\" >> \"{0}\"";

				PostLinkAction.CommandArguments += String.Format(Touch, OutputFile.AbsolutePath);
				PostLinkAction.CommandArguments += bUseCmdExe ? "\"" : "'";

				System.Console.WriteLine("{0} {1}", PostLinkAction.CommandPath, PostLinkAction.CommandArguments);

				PostLinkAction.ProducedItems.Add(OutputFile);
				Actions.Add(PostLinkAction);
				return OutputFile;
			}
			else
			{
				return null;
			}
		}

		// allow sub-platforms to modify the name of the output file
		protected virtual FileItem GetLinkOutputFile(LinkEnvironment LinkEnvironment)
		{
			return FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
		}


		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, List<Action> Actions)
		{
			Debug.Assert(!bBuildImportLibraryOnly);

			if ((LinkEnvironment.bAllowLTCG || LinkEnvironment.bPGOOptimize || LinkEnvironment.bPGOProfile) && !CanUseAdvancedLinkerFeatures(LinkEnvironment.Architecture))
			{
				throw new BuildException(ExplainWhyCannotUseAdvancedLinkerFeatures(LinkEnvironment.Architecture));
			}

			List<string> RPaths = new List<string>();

			if (LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly)
			{
				return CreateArchiveAndIndex(LinkEnvironment, Actions);
			}

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;

			string LinkCommandString;
			if (String.IsNullOrEmpty(ClangPath))
			{
				LinkCommandString = "\"" + GCCPath + "\"";
			}
			else
			{
				LinkCommandString = "\"" + ClangPath + "\"";
			}

			// Get link arguments.
			LinkCommandString += GetLinkArguments(LinkEnvironment);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = LinkEnvironment.bIsBuildingDLL;

			// Add the output file as a production of the link action.
			FileItem OutputFile = GetLinkOutputFile(LinkEnvironment);
			LinkAction.ProducedItems.Add(OutputFile);
			// LTO/PGO can take a lot of time, make it clear for the user
			if (LinkEnvironment.bPGOProfile)
			{
				LinkAction.CommandDescription = "Link-PGI";
			}
			else if (LinkEnvironment.bPGOOptimize)
			{
				LinkAction.CommandDescription = "Link-PGO";
			}
			else if (LinkEnvironment.bAllowLTCG)
			{
				LinkAction.CommandDescription = "Link-LTO";
			}
			else
			{
				LinkAction.CommandDescription = "Link";
			}
			// because the logic choosing between lld and ld is somewhat messy atm (lld fails to link .DSO due to bugs), make the name of the linker clear
			LinkAction.CommandDescription += (LinkCommandString.Contains("-fuse-ld=lld")) ? " (lld)" : " (ld)";
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Add the output file to the command-line.
			LinkCommandString += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> ResponseLines = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				ResponseLines.Add(string.Format("\"{0}\"", InputFile.AbsolutePath.Replace("\\", "/")));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				ResponseLines.Add(string.Format(" -soname=\"{0}\"", OutputFile.Location.GetFileName()));
			}

			// Start with the configured LibraryPaths and also add paths to any libraries that
			// we depend on (libraries that we've build ourselves).
			List<DirectoryReference> AllLibraryPaths = LinkEnvironment.LibraryPaths;
			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				string PathToLib = Path.GetDirectoryName(AdditionalLibrary);
				if (!String.IsNullOrEmpty(PathToLib))
				{
					// make path absolute, because FixDependencies script may be executed in a different directory
					DirectoryReference AbsolutePathToLib = new DirectoryReference(PathToLib);
					if (!AllLibraryPaths.Contains(AbsolutePathToLib))
					{
						AllLibraryPaths.Add(AbsolutePathToLib);
					}
				}

				if ((AdditionalLibrary.Contains("Plugins") || AdditionalLibrary.Contains("Binaries/ThirdParty") || AdditionalLibrary.Contains("Binaries\\ThirdParty")) && Path.GetDirectoryName(AdditionalLibrary) != Path.GetDirectoryName(OutputFile.AbsolutePath))
				{
					string RelativePath = new FileReference(AdditionalLibrary).Directory.MakeRelativeTo(OutputFile.Location.Directory);
					// On Windows, MakeRelativeTo can silently fail if the engine and the project are located on different drives
					if (CrossCompiling() && RelativePath.StartsWith(UnrealBuildTool.RootDirectory.FullName))
					{
						// do not replace directly, but take care to avoid potential double slashes or missed slashes
						string PathFromRootDir = RelativePath.Replace(UnrealBuildTool.RootDirectory.FullName, "");
						// Path.Combine doesn't combine these properly
						RelativePath = ((PathFromRootDir.StartsWith("\\") || PathFromRootDir.StartsWith("/")) ? "..\\..\\.." : "..\\..\\..\\") + PathFromRootDir;
					}

					if (!RPaths.Contains(RelativePath))
					{
						RPaths.Add(RelativePath);
						ResponseLines.Add(string.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
					}
				}
			}

			foreach(string RuntimeLibaryPath in LinkEnvironment.RuntimeLibraryPaths)
			{
				string RelativePath = RuntimeLibaryPath;
				if(!RelativePath.StartsWith("$"))
				{
					string RelativeRootPath = new DirectoryReference(RuntimeLibaryPath).MakeRelativeTo(UnrealBuildTool.RootDirectory);
					// We're assuming that the binary will be placed according to our ProjectName/Binaries/Platform scheme
					RelativePath = Path.Combine("..", "..", "..", RelativeRootPath);
				}
				if (!RPaths.Contains(RelativePath))
				{
					RPaths.Add(RelativePath);
					ResponseLines.Add(string.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
				}
			}

			ResponseLines.Add(string.Format(" -rpath-link=\"{0}\"", Path.GetDirectoryName(OutputFile.AbsolutePath)));

			// Add the library paths to the argument list.
			foreach (DirectoryReference LibraryPath in AllLibraryPaths)
			{
				// use absolute paths because of FixDependencies script again
				ResponseLines.Add(string.Format(" -L\"{0}\"", LibraryPath.FullName.Replace('\\', '/')));
			}

			List<string> EngineAndGameLibrariesLinkFlags = new List<string>();
			List<FileItem> EngineAndGameLibrariesFiles = new List<FileItem>();

			// Pre-2.25 ld has symbol resolution problems when .so are mixed with .a in a single --start-group/--end-group
			// when linking with --as-needed.
			// Move external libraries to a separate --start-group/--end-group to fix it (and also make groups smaller and faster to link).
			// See https://github.com/EpicGames/UnrealEngine/pull/2778 and https://github.com/EpicGames/UnrealEngine/pull/2793 for discussion
			string ExternalLibraries = "";

			// add libraries in a library group
			ResponseLines.Add(string.Format(" --start-group"));

			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
				{
					// library was passed just like "jemalloc", turn it into -ljemalloc
					ExternalLibraries += string.Format(" -l{0}", AdditionalLibrary);
				}
				else if (Path.GetExtension(AdditionalLibrary) == ".a")
				{
					// static library passed in, pass it along but make path absolute, because FixDependencies script may be executed in a different directory
					string AbsoluteAdditionalLibrary = Path.GetFullPath(AdditionalLibrary);
					if (AbsoluteAdditionalLibrary.Contains(" "))
					{
						AbsoluteAdditionalLibrary = string.Format("\"{0}\"", AbsoluteAdditionalLibrary);
					}
					AbsoluteAdditionalLibrary = AbsoluteAdditionalLibrary.Replace('\\', '/');

					// libcrypto/libssl contain number of functions that are being used in different DSOs. FIXME: generalize?
					if (LinkEnvironment.bIsBuildingDLL && (AbsoluteAdditionalLibrary.Contains("libcrypto") || AbsoluteAdditionalLibrary.Contains("libssl")))
					{
						ResponseLines.Add(" --whole-archive " + AbsoluteAdditionalLibrary + " --no-whole-archive");
					}
					else
					{
						ResponseLines.Add(" " + AbsoluteAdditionalLibrary);
					}

					LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
				}
				else
				{
					// Skip over full-pathed library dependencies when building DLLs to avoid circular
					// dependencies.
					FileItem LibraryDependency = FileItem.GetItemByPath(AdditionalLibrary);

					string LibName = Path.GetFileNameWithoutExtension(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						// Remove lib prefix
						LibName = LibName.Remove(0, 3);
					}
					string LibLinkFlag = string.Format(" -l{0}", LibName);

					if (LinkEnvironment.bIsBuildingDLL && LinkEnvironment.bIsCrossReferenced)
					{
						// We are building a cross referenced DLL so we can't actually include
						// dependencies at this point. Instead we add it to the list of
						// libraries to be used in the FixDependencies step.
						EngineAndGameLibrariesLinkFlags.Add(LibLinkFlag);
						EngineAndGameLibrariesFiles.Add(LibraryDependency);
						// it is important to add this exactly to the same place where the missing libraries would have been, it will be replaced later
						if (!ExternalLibraries.Contains("--allow-shlib-undefined"))
						{
							ExternalLibraries += string.Format(" -Wl,--allow-shlib-undefined");
						}
					}
					else
					{
						LinkAction.PrerequisiteItems.Add(LibraryDependency);
						ExternalLibraries += LibLinkFlag;
					}
				}
			}
			ResponseLines.Add(" --end-group");

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
			FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponseFileName, ResponseLines);

			LinkCommandString += string.Format(" -Wl,@\"{0}\"", ResponseFileName);
			LinkAction.PrerequisiteItems.Add(ResponseFileItem);

			LinkCommandString += " -Wl,--start-group";
			LinkCommandString += ExternalLibraries;
			LinkCommandString += " -Wl,--end-group";

			LinkCommandString += " -lrt"; // needed for clock_gettime()
			LinkCommandString += " -lm"; // math

			if (ShouldUseLibcxx(LinkEnvironment.Architecture))
			{
				// libc++ and its abi lib
				LinkCommandString += " -nodefaultlibs";
				LinkCommandString += " -L" + "ThirdParty/Linux/LibCxx/lib/Linux/" + LinkEnvironment.Architecture + "/";
				LinkCommandString += " " + "ThirdParty/Linux/LibCxx/lib/Linux/" + LinkEnvironment.Architecture + "/libc++.a";
				LinkCommandString += " " + "ThirdParty/Linux/LibCxx/lib/Linux/" + LinkEnvironment.Architecture + "/libc++abi.a";
				LinkCommandString += " -lm";
				LinkCommandString += " -lc";
				LinkCommandString += " -lgcc_s";
				LinkCommandString += " -lgcc";
			}

			// these can be helpful for understanding the order of libraries or library search directories
			if (PlatformSDK.bVerboseLinker)
			{
				LinkCommandString += " -Wl,--verbose";
				LinkCommandString += " -Wl,--trace";
				LinkCommandString += " -v";
			}

			// Add the additional arguments specified by the environment.
			LinkCommandString += LinkEnvironment.AdditionalArguments;
			LinkCommandString = LinkCommandString.Replace("\\\\", "/");
			LinkCommandString = LinkCommandString.Replace("\\", "/");

			bool bUseCmdExe = BuildHostPlatform.Current.ShellType == ShellType.Cmd;
			FileReference ShellBinary = BuildHostPlatform.Current.Shell;
			string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x

			// Linux has issues with scripts and parameter expansion from curely brakets
			if (!bUseCmdExe)
			{
				LinkCommandString = LinkCommandString.Replace("{", "'{");
				LinkCommandString = LinkCommandString.Replace("}", "}'");
				LinkCommandString = LinkCommandString.Replace("$'{", "'${");	// fixing $'{ORIGIN}' to be '${ORIGIN}'
			}

			string LinkScriptName = string.Format((bUseCmdExe ? "Link-{0}.link.bat" : "Link-{0}.link.sh"), OutputFile.Location.GetFileName());
			string LinkScriptFullPath = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, LinkScriptName);
			Log.TraceVerbose("Creating link script: {0}", LinkScriptFullPath);
			Directory.CreateDirectory(Path.GetDirectoryName(LinkScriptFullPath));
			using (StreamWriter LinkWriter = File.CreateText(LinkScriptFullPath))
			{
				if (bUseCmdExe)
				{
					LinkWriter.Write("@echo off\n");
					LinkWriter.Write("rem Automatically generated by UnrealBuildTool\n");
					LinkWriter.Write("rem *DO NOT EDIT*\n\n");
					LinkWriter.Write("set Retries=0\n");
					LinkWriter.Write(":linkloop\n");
					LinkWriter.Write("if %Retries% GEQ 10 goto failedtorelink\n");
					LinkWriter.Write(LinkCommandString + "\n");
					LinkWriter.Write("if %errorlevel% neq 0 goto sleepandretry\n");
					LinkWriter.Write(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile) + "\n");
					LinkWriter.Write("exit 0\n");
					LinkWriter.Write(":sleepandretry\n");
					LinkWriter.Write("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL\n");     // timeout complains about lack of redirection
					LinkWriter.Write("set /a Retries+=1\n");
					LinkWriter.Write("goto linkloop\n");
					LinkWriter.Write(":failedtorelink\n");
					LinkWriter.Write("echo Failed to link {0} after %Retries% retries\n", OutputFile.AbsolutePath);
					LinkWriter.Write("exit 1\n");
				}
				else
				{
					LinkWriter.Write("#!/bin/sh\n");
					LinkWriter.Write("# Automatically generated by UnrealBuildTool\n");
					LinkWriter.Write("# *DO NOT EDIT*\n\n");
					LinkWriter.Write("set -o errexit\n");
					LinkWriter.Write(LinkCommandString + "\n");
					LinkWriter.Write(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile) + "\n");
				}
			};

			LinkAction.CommandPath = ShellBinary;

			// This must maintain the quotes around the LinkScriptFullPath
			LinkAction.CommandArguments = ExecuteSwitch + " \"" + LinkScriptFullPath + "\"";

			// prepare a linker script
			FileReference LinkerScriptPath = FileReference.Combine(LinkEnvironment.LocalShadowDirectory, "remove-sym.ldscript");
			if (!DirectoryReference.Exists(LinkEnvironment.LocalShadowDirectory))
			{
				DirectoryReference.CreateDirectory(LinkEnvironment.LocalShadowDirectory);
			}
			if (FileReference.Exists(LinkerScriptPath))
			{
				FileReference.Delete(LinkerScriptPath);
			}

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;
			Actions.Add(LinkAction);

			// Prepare a script that will run later, once all shared libraries and the executable
			// are created. This script will be called by action created in FixDependencies()
			if (LinkEnvironment.bIsCrossReferenced && LinkEnvironment.bIsBuildingDLL)
			{
				if (bUseFixdeps)
				{
					string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

					string FixDepsScriptPath = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, ScriptName);
					if (!bHasWipedFixDepsScript)
					{
						bHasWipedFixDepsScript = true;
						Log.TraceVerbose("Creating script: {0}", FixDepsScriptPath);
						Directory.CreateDirectory(Path.GetDirectoryName(FixDepsScriptPath));
						using (StreamWriter Writer = File.CreateText(FixDepsScriptPath))
						{
						if (bUseCmdExe)
						{
							Writer.Write("@echo off\n");
							Writer.Write("rem Automatically generated by UnrealBuildTool\n");
							Writer.Write("rem *DO NOT EDIT*\n\n");
						}
						else
						{
							Writer.Write("#!/bin/sh\n");
							Writer.Write("# Automatically generated by UnrealBuildTool\n");
							Writer.Write("# *DO NOT EDIT*\n\n");
							Writer.Write("set -o errexit\n");
						}
						}
					}

					StreamWriter FixDepsScript = File.AppendText(FixDepsScriptPath);

					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibrariesLinkFlags)
					{
						EngineAndGameLibrariesString += Library;
					}

					FixDepsScript.Write(string.Format("echo Fixing {0}\n", Path.GetFileName(OutputFile.AbsolutePath)));
					if (!bUseCmdExe)
					{
						FixDepsScript.Write(string.Format("TIMESTAMP=`stat --format %y \"{0}\"`\n", OutputFile.AbsolutePath));
					}
					string FixDepsLine = LinkCommandString;
					string Replace = "-Wl,--allow-shlib-undefined";

					FixDepsLine = FixDepsLine.Replace(Replace, EngineAndGameLibrariesString);
					string OutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
					FixDepsLine = FixDepsLine.Replace(OutputFileForwardSlashes, OutputFileForwardSlashes + ".fixed");
					FixDepsLine = FixDepsLine.Replace("$", "\\$");
					FixDepsScript.Write(FixDepsLine + "\n");
					if (bUseCmdExe)
					{
						FixDepsScript.Write(string.Format("move /Y \"{0}.fixed\" \"{0}\"\n", OutputFile.AbsolutePath));
					}
					else
					{
						FixDepsScript.Write(string.Format("mv \"{0}.fixed\" \"{0}\"\n", OutputFile.AbsolutePath));
						FixDepsScript.Write(string.Format("touch -d \"$TIMESTAMP\" \"{0}\"\n\n", OutputFile.AbsolutePath));
					}
					FixDepsScript.Close();
				}
				else
				{
					// Create the action to relink the library. This actions does not overwrite the source file so it can be executed in parallel
					Action RelinkAction = new Action(ActionType.Link);
					RelinkAction.WorkingDirectory = LinkAction.WorkingDirectory;
					RelinkAction.StatusDescription = LinkAction.StatusDescription;
					RelinkAction.CommandDescription = "Relink";
					RelinkAction.bCanExecuteRemotely = false;
					RelinkAction.ProducedItems.Clear();
					RelinkAction.PrerequisiteItems = new List<FileItem>(LinkAction.PrerequisiteItems);
					foreach (FileItem Dependency in EngineAndGameLibrariesFiles)
					{
						RelinkAction.PrerequisiteItems.Add(Dependency);
					}
					RelinkAction.PrerequisiteItems.Add(OutputFile); // also depend on the first link action's output

					string LinkOutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
					string RelinkedFileForwardSlashes = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, OutputFile.Location.GetFileName()) + ".relinked";

					// cannot use the real product because we need to maintain the timestamp on it
					FileReference RelinkActionDummyProductRef = FileReference.Combine(LinkEnvironment.LocalShadowDirectory, LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".relinked_action_ran");
					RelinkAction.ProducedItems.Add(FileItem.GetItemByFileReference(RelinkActionDummyProductRef));

					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibrariesLinkFlags)
					{
						EngineAndGameLibrariesString += Library;
					}

					// create the relinking step
					string RelinkScriptName = string.Format((bUseCmdExe ? "Relink-{0}.bat" : "Relink-{0}.sh"), OutputFile.Location.GetFileName());
					string RelinkScriptFullPath = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, RelinkScriptName);

					Log.TraceVerbose("Creating script: {0}", RelinkScriptFullPath);
					Directory.CreateDirectory(Path.GetDirectoryName(RelinkScriptFullPath));
					using (StreamWriter RelinkWriter = File.CreateText(RelinkScriptFullPath))
					{
					string RelinkInvocation = LinkCommandString;
					string Replace = "-Wl,--allow-shlib-undefined";
					RelinkInvocation = RelinkInvocation.Replace(Replace, EngineAndGameLibrariesString);

					// should be the same as RelinkedFileRef
					RelinkInvocation = RelinkInvocation.Replace(LinkOutputFileForwardSlashes, RelinkedFileForwardSlashes);
					RelinkInvocation = RelinkInvocation.Replace("$", "\\$");

					if (bUseCmdExe)
					{
						RelinkWriter.Write("@echo off\n");
						RelinkWriter.Write("rem Automatically generated by UnrealBuildTool\n");
						RelinkWriter.Write("rem *DO NOT EDIT*\n\n");
						RelinkWriter.Write("set Retries=0\n");
						RelinkWriter.Write(":relinkloop\n");
						RelinkWriter.Write("if %Retries% GEQ 10 goto failedtorelink\n");
						RelinkWriter.Write(RelinkInvocation + "\n");
						RelinkWriter.Write("if %errorlevel% neq 0 goto sleepandretry\n");
						RelinkWriter.Write("copy /B \"{0}\" \"{1}.temp\" >NUL 2>NUL\n", RelinkedFileForwardSlashes, OutputFile.AbsolutePath);
						RelinkWriter.Write("if %errorlevel% neq 0 goto sleepandretry\n");
						RelinkWriter.Write("move /Y \"{0}.temp\" \"{1}\" >NUL 2>NUL\n", OutputFile.AbsolutePath, OutputFile.AbsolutePath);
						RelinkWriter.Write("if %errorlevel% neq 0 goto sleepandretry\n");
						RelinkWriter.Write(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile) + "\n");
						RelinkWriter.Write(string.Format("echo \"Dummy\" >> \"{0}\" && copy /b \"{0}\" +,,\n", RelinkActionDummyProductRef.FullName));
						RelinkWriter.Write("echo Relinked {0} successfully after %Retries% retries\n", OutputFile.AbsolutePath);
						RelinkWriter.Write("exit 0\n");
						RelinkWriter.Write(":sleepandretry\n");
						RelinkWriter.Write("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL\n");     // timeout complains about lack of redirection
						RelinkWriter.Write("set /a Retries+=1\n");
						RelinkWriter.Write("goto relinkloop\n");
						RelinkWriter.Write(":failedtorelink\n");
						RelinkWriter.Write("echo Failed to relink {0} after %Retries% retries\n", OutputFile.AbsolutePath);
						RelinkWriter.Write("exit 1\n");
					}
					else
					{
						RelinkWriter.Write("#!/bin/sh\n");
						RelinkWriter.Write("# Automatically generated by UnrealBuildTool\n");
						RelinkWriter.Write("# *DO NOT EDIT*\n\n");
						RelinkWriter.Write("set -o errexit\n");
						RelinkWriter.Write(RelinkInvocation + "\n");
						RelinkWriter.Write(string.Format("TIMESTAMP=`stat --format %y \"{0}\"`\n", OutputFile.AbsolutePath));
						RelinkWriter.Write("cp \"{0}\" \"{1}.temp\"\n", RelinkedFileForwardSlashes, OutputFile.AbsolutePath);
						RelinkWriter.Write("mv \"{0}.temp\" \"{1}\"\n", OutputFile.AbsolutePath, OutputFile.AbsolutePath);
						RelinkWriter.Write(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile) + "\n");
						RelinkWriter.Write(string.Format("touch -d \"$TIMESTAMP\" \"{0}\"\n\n", OutputFile.AbsolutePath));
						RelinkWriter.Write(string.Format("echo \"Dummy\" >> \"{0}\"", RelinkActionDummyProductRef.FullName));
					}
					}

					RelinkAction.CommandPath = ShellBinary;
					RelinkAction.CommandArguments = ExecuteSwitch + " \"" + RelinkScriptFullPath + "\"";
					Actions.Add(RelinkAction);
				}
			}
			return OutputFile;
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			if (bUseFixdeps)
			{
				foreach (UEBuildBinary Binary in Binaries)
				{
					AllBinaries.Add(FileItem.GetItemByFileReference(Binary.OutputFilePath));
				}
			}
		}

		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, List<Action> Actions)
		{
			ICollection<FileItem> OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment, Actions);

			if (bUseFixdeps)
			{
				if (BinaryLinkEnvironment.bIsBuildingDLL || BinaryLinkEnvironment.bIsBuildingLibrary)
				{
					return OutputFiles;
				}

				FileItem FixDepsOutputFile = FixDependencies(BinaryLinkEnvironment, Executable, Actions);
				if (FixDepsOutputFile != null)
				{
					OutputFiles.Add(FixDepsOutputFile);
				}
			}
			else
			{
				// make build product of cross-referenced DSOs to be *.relinked_action_ran, so the relinking steps are executed
				if (BinaryLinkEnvironment.bIsBuildingDLL && BinaryLinkEnvironment.bIsCrossReferenced)
				{
					FileReference RelinkedMapRef = FileReference.Combine(BinaryLinkEnvironment.LocalShadowDirectory, BinaryLinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".relinked_action_ran");
					OutputFiles.Add(FileItem.GetItemByFileReference(RelinkedMapRef));
				}
			}
			return OutputFiles;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = GetStripPath(Architecture);
			StartInfo.Arguments = "--strip-debug \"" + TargetFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	}
}
