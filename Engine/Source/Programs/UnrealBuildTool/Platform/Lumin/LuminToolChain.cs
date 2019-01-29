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
	class LuminToolChain : AndroidToolChain
	{
		static private Dictionary<string, string[]> AllArchNames = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { "armv7", "armeabi-v7a", "android-kk-egl-t124-a32" } },
			{ "-arm64", new string[] { "arm64", "arm64-v8a", "android-L-egl-t132-a64" } },
		};

		static private string[] LibrariesToSkip = new string[] { "nvToolsExt", "nvToolsExtStub", "oculus", "vrapi", "ovrkernel", "systemutils", "openglloader", "gpg", };

		private List<string> AdditionalGPUArches;

		protected string StripPath;
		protected string ObjCopyPath;

		private string MabuPath_ = null;

		private string MabuPath
		{
			get
			{
				if (MabuPath_ == null)
				{
					MabuPath_ = Environment.ExpandEnvironmentVariables("%MLSDK%/mabu" + (Utils.IsRunningOnMono ? "" : ".cmd"));

					MabuPath_ = MabuPath_.Replace("\"", "");

					if (!File.Exists(MabuPath_))
					{
						throw new BuildException("Could not find mabu command at '{0}'", MabuPath_);
					}
				}
				return MabuPath_;
			}
		}

		[CommandLine("-GpuArchitectures=", ListSeparator = '+')]
		public List<string> GPUArchitectureArg = new List<string>();
		public LuminToolChain(FileReference InProjectFile, bool bInUseLdGold = true, IReadOnlyList<string> InAdditionalArches = null, IReadOnlyList<string> InAdditionalGPUArches = null, bool bAllowMissingNDK = true)
			: base(CppPlatform.Lumin, InProjectFile,
				  // @todo Lumin: ld gold?
				  true, null, null, true)

		{
			AdditionalGPUArches = new List<string>();
			if (InAdditionalGPUArches != null)
			{
				AdditionalGPUArches.AddRange(InAdditionalGPUArches);
			}


			// by default tools chains don't parse arguments, but we want to be able to check the -gpuarchitectures flag defined above. This is
			// only necessary when LuminToolChain is used during UAT
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this);
			if (AdditionalGPUArches.Count == 0 && GPUArchitectureArg.Count > 0)
			{
				AdditionalGPUArches.AddRange(GPUArchitectureArg);
			}

			string MLSDKPath = Environment.GetEnvironmentVariable("MLSDK");

			// don't register if we don't have an MLSDK specified
			if (String.IsNullOrEmpty(MLSDKPath))
			{
				throw new BuildException("MLSDK is not specified; cannot use Lumin toolchain.");
			}

			MLSDKPath = MLSDKPath.Replace("\"", "");

			string MabuSpec = RunMabuAndReadOutput("-t device --print-spec");

			// parse clange version
			Regex SpecRegex = new Regex("\\s*(?:[a-z]+_lumin_clang-)(\\d)[.](\\d)\\s*");

			Match SpecMatch = SpecRegex.Match(MabuSpec);

			if (SpecMatch.Groups.Count != 3)
			{
				throw new BuildException("Could not parse clang version from mabu spec '{0}'", MabuSpec);
			}

			string ClangVersion = string.Format("{0}.{1}", SpecMatch.Groups[1].Value, SpecMatch.Groups[2].Value);

			SetClangVersion(int.Parse(SpecMatch.Groups[1].Value), int.Parse(SpecMatch.Groups[2].Value), 0);

			string MabuTools = RunMabuAndReadOutput("-t device --print-tools");

			Dictionary<string, string> ToolsDict = new Dictionary<string, string>();
			using (StringReader Reader = new StringReader(MabuTools))
			{
				string Line = null;
				while (null != (Line = Reader.ReadLine()))
				{
					string[] Split = Line.Split('=');
					if (Split.Length != 2)
					{
						throw new BuildException("Unexpected output from mabu in --print-tools: '{0}'", Line);
					}

					ToolsDict.Add(Split[0].Trim(), Split[1].Trim());
				}
			}

			// set up the path to our toolchains
			// Clang path used to be in quotes, but some part of FileReference in AndroidToolChain was choking on the quotes.  Don't put your MLSDK in a directory with spaces for now, I guess.
			ClangPath = ToolsDict["CXX"];
			ArPathArm64 = "\"" + ToolsDict["AR"] + "\"";
			// The strip command does not execute through the shell. Hence we don't quote it.
			StripPath = ToolsDict["STRIP"];
			// The objcopy command does not execute through the shell. Hence we don't quote it.
			ObjCopyPath = ToolsDict["OBJCOPY"];

			// force the compiler to be executed through a command prompt instance
			bExecuteCompilerThroughShell = true;

			// toolchain params
			ToolchainParamsArm64 = " --sysroot=\"" + Path.Combine(MLSDKPath, "lumin") + "\"";

			ToolchainLinkParamsArm = ToolchainParamsArm;
			ToolchainLinkParamsArm64 = ToolchainParamsArm64;
			ToolchainLinkParamsx86 = ToolchainParamsx86;
			ToolchainLinkParamsx64 = ToolchainParamsx64;
		}

		public bool UseVulkan()
		{
			DirectoryReference DirRef = (!string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()) : (ProjectFile != null ? ProjectFile.Directory : null));
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirRef, UnrealTargetPlatform.Lumin);
			// go by string
			bool bUseVulkan = true;
			Ini.GetBool("/Script/LuminRuntimeSettings.LuminRuntimeSettings", "bUseVulkan", out bUseVulkan);

			return bUseVulkan;
		}

		public bool UseMobileRendering()
		{
			DirectoryReference DirRef = (!string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()) ? new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()) : (ProjectFile != null ? ProjectFile.Directory : null));
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirRef, UnrealTargetPlatform.Lumin);

			// go by string
			bool bUseMobileRendering = true;
			Ini.GetBool("/Script/LuminRuntimeSettings.LuminRuntimeSettings", "bUseMobileRendering", out bUseMobileRendering);

			return bUseMobileRendering;
		}

		public override void ParseArchitectures()
		{
			Arches = new List<string>() { "-arm64" };

			GPUArchitectures = new List<string>();
			if (AdditionalGPUArches != null)
			{
				if (!UseMobileRendering() && AdditionalGPUArches.Contains("lumingl4"))
				{
					GPUArchitectures.Add("-lumingl4");
				}
				else
				{
					GPUArchitectures.Add("-lumin");
				}
			}
			if (GPUArchitectures.Count == 0)
			{
				if (UseMobileRendering() || UseVulkan())
				{
					GPUArchitectures.Add("-lumin");
				}
				else
				{
					GPUArchitectures.Add("-lumingl4");
				}
			}

			AllComboNames = (from Arch in Arches
							 from GPUArch in GPUArchitectures
							 select Arch + GPUArch).ToList();
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{

		}

		protected override void ModifySourceFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
		}

		protected override void ModifyLibraries(LinkEnvironment LinkEnvironment)
		{
		}

		protected override string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment, string Architecture)
		{
			string Params = base.GetCLArguments_Global(CompileEnvironment, Architecture);

			Params += " -Wno-undefined-var-template";
			if (GPUArchitectures.Contains("-lumingl4"))
			{
				Params += " -DPLATFORM_LUMINGL4=1";
			}
			else
			{
				Params += " -DPLATFORM_LUMINGL4=0";
			}

			// jf: added for seal, as XGE seems to not preserve case in includes properly
			Params += " -Wno-nonportable-include-path";         // not all of these are real

			return Params;
		}

		protected override string GetLinkArguments(LinkEnvironment LinkEnvironment, string Architecture)
		{
			string Result = "";

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Result += " -Wl,-shared,-Bsymbolic";
			}
			else
			{
				// ignore unresolved symbols in shared libs
				Result += string.Format(" -Wl,--unresolved-symbols=ignore-in-shared-libs");
			}
			Result += " -Wl,-z,nocopyreloc";
			Result += " -Wl,--warn-shared-textrel";
			Result += " -Wl,--fatal-warnings";
			Result += " -Wl,--no-undefined";
			Result += " -no-canonical-prefixes";
			Result += " -Wl,-z,relro";
			Result += " -Wl,-z,now";
			Result += " -Wl,--enable-new-dtags";
			Result += " -Wl,--export-dynamic";
			Result += " -Wl,-rpath=$ORIGIN";
			Result += " -fdiagnostics-format=msvc";

			if (!LinkEnvironment.bCreateDebugInfo)
			{
				Result += " -Wl,--strip-debug";
			}

			if (!LinkEnvironment.bIsBuildingDLL)
			{
				// Position independent code executable *only*.
				Result += " -pie";
			}

			Result += ToolchainParamsArm64;

			return Result;
		}

		private static void RunCommandLineProgramWithException(string WorkingDirectory, string Command, string Params, string OverrideDesc = null, bool bUseShellExecute = false)
		{
			if (OverrideDesc == null)
			{
				Log.TraceInformation("\nRunning: " + Command + " " + Params);
			}
			else if (OverrideDesc != "")
			{
				Log.TraceInformation(OverrideDesc);
				Log.TraceVerbose("\nRunning: " + Command + " " + Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			// bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		public void RunMabuWithException(string WorkingDirectory, string Params, string OverrideDesc = null)
		{
			RunCommandLineProgramWithException(WorkingDirectory, MabuPath, Params, OverrideDesc, false);
		}

		public string RunMabuAndReadOutput(string Params)
		{
			try
			{
				return Utils.RunLocalProcessAndReturnStdOut(MabuPath, Params);
			}
			catch (Exception e)
			{
				throw new BuildException("Running mabu failed: '{0}'", e.Message);
			}
		}

		protected override bool ValidateNDK(string PlatformsDir, string ApiString)
		{
			return true;
		}

		public override string GetStripPath(FileReference SourceFile)
		{
			return StripPath;
		}

		/// <summary>
		/// Creates an object file with only the symbolic debug information from an executable.
		/// </summary>
		/// <param name="SourceExeFile">The executable with debug symbol information.</param>
		/// <param name="TargetSymFile">The generated object file with debug symbols.</param>
		public void ExtractSymbols(FileReference SourceExeFile, FileReference TargetSymFile)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName =ObjCopyPath;
			StartInfo.Arguments = " --only-keep-debug \"" + SourceExeFile.FullName + "\" \"" + TargetSymFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}

		/// <summary>
		/// Creates a debugger link in an executable referencing where the debug symbols for it are located.
		/// </summary>
		/// <param name="SourceDebugFile">An object file with the debug symbols, can be an executable or the file generated with ExtractSymbols.</param>
		/// <param name="TargetExeFile">The executable to reference the split debug info into.</param>
		public void LinkSymbols(FileReference SourceDebugFile, FileReference TargetExeFile)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = ObjCopyPath;
			StartInfo.Arguments = " --add-gnu-debuglink=\"" + SourceDebugFile.FullName + "\" \"" + TargetExeFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
	};
}
