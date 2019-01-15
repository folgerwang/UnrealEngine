// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Text;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	abstract class AppleToolChainSettings
	{
		/// <summary>
		/// Which developer directory to root from? If this is "xcode-select", UBT will query for the currently selected Xcode
		/// </summary>
		public string XcodeDeveloperDir = "xcode-select";

		public AppleToolChainSettings(bool bVerbose)
		{
			SelectXcode(ref XcodeDeveloperDir, bVerbose);
		}

		private static void SelectXcode(ref string DeveloperDir, bool bVerbose)
		{
			string Reason = "hardcoded";

			if (DeveloperDir == "xcode-select")
			{
				Reason = "xcode-select";

				// on the Mac, run xcode-select directly
				DeveloperDir = Utils.RunLocalProcessAndReturnStdOut("xcode-select", "--print-path");

				// make sure we get a full path
				if (Directory.Exists(DeveloperDir) == false)
				{
					throw new BuildException("Selected Xcode ('{0}') doesn't exist, cannot continue.", DeveloperDir);
				}

				if (DeveloperDir.EndsWith("/") == false)
				{
					// we expect this to end with a slash
					DeveloperDir += "/";
				}
			}

			if (bVerbose && !DeveloperDir.StartsWith("/Applications/Xcode.app"))
			{
				Log.TraceInformationOnce("Compiling with non-standard Xcode ({0}): {1}", Reason, DeveloperDir);
			}

		}

		protected void SelectSDK(string BaseSDKDir, string OSPrefix, ref string PlatformSDKVersion, bool bVerbose)
		{
			if (PlatformSDKVersion == "latest")
			{
				PlatformSDKVersion = "";
				try
				{
					// on the Mac, we can just get the directory name
					string[] SubDirs = System.IO.Directory.GetDirectories(BaseSDKDir);

					// loop over the subdirs and parse out the version
					int MaxSDKVersionMajor = 0;
					int MaxSDKVersionMinor = 0;
					string MaxSDKVersionString = null;
					foreach (string SubDir in SubDirs)
					{
						string SubDirName = Path.GetFileNameWithoutExtension(SubDir);
						if (SubDirName.StartsWith(OSPrefix))
						{
							// get the SDK version from the directory name
							string SDKString = SubDirName.Replace(OSPrefix, "");
							int Major = 0;
							int Minor = 0;

							// parse it into whole and fractional parts (since 10.10 > 10.9 in versions, but not in math)
							try
							{
								string[] Tokens = SDKString.Split(".".ToCharArray());
								if (Tokens.Length == 2)
								{
									Major = int.Parse(Tokens[0]);
									Minor = int.Parse(Tokens[1]);
								}
							}
							catch (Exception)
							{
								// weirdly formatted SDKs
								continue;
							}

							// update largest SDK version number
							if (Major > MaxSDKVersionMajor || (Major == MaxSDKVersionMajor && Minor > MaxSDKVersionMinor))
							{
								MaxSDKVersionString = SDKString;
								MaxSDKVersionMajor = Major;
								MaxSDKVersionMinor = Minor;
							}
						}
					}

					// use the largest version
					if (MaxSDKVersionString != null)
					{
						PlatformSDKVersion = MaxSDKVersionString;
					}
				}
				catch (Exception Ex)
				{
					// on any exception, just use the backup version
					Log.TraceInformation("Triggered an exception while looking for SDK directory in Xcode.app");
					Log.TraceInformation("{0}", Ex.ToString());
				}
			}

			// make sure we have a valid SDK directory
			if (Utils.IsRunningOnMono && !Directory.Exists(Path.Combine(BaseSDKDir, OSPrefix + PlatformSDKVersion + ".sdk")))
			{
				throw new BuildException("Invalid SDK {0}{1}.sdk, not found in {2}", OSPrefix, PlatformSDKVersion, BaseSDKDir);
			}

			if (bVerbose && !ProjectFileGenerator.bGenerateProjectFiles)
			{
				Log.TraceInformation("Compiling with {0} SDK {1}", OSPrefix, PlatformSDKVersion);
			}
		}
	}

	abstract class AppleToolChain : UEToolChain
	{
		protected FileReference ProjectFile;

		public AppleToolChain(CppPlatform InCppPlatform, FileReference InProjectFile)
			: base(InCppPlatform)
		{
			ProjectFile = InProjectFile;
		}

		protected DirectoryReference GetMacDevSrcRoot()
		{
			return UnrealBuildTool.EngineSourceDirectory;
		}

		protected void StripSymbolsWithXcode(FileReference SourceFile, FileReference TargetFile, string ToolchainDir)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = Path.Combine(ToolchainDir, "strip");
			StartInfo.Arguments = String.Format("\"{0}\" -S", TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
		}
		
		protected string GetDsymutilPath()
		{
			FileReference DsymutilLocation = new FileReference("/usr/bin/dsymutil");

			DirectoryReference AutoSdkDir;
			if (UEBuildPlatformSDK.TryGetHostPlatformAutoSDKDir(out AutoSdkDir))
			{
				FileReference AutoSdkDsymutilLocation = FileReference.Combine(AutoSdkDir, "Mac", "LLVM", "bin", "dsymutil");
				if (FileReference.Exists(AutoSdkDsymutilLocation))
				{
					DsymutilLocation = AutoSdkDsymutilLocation;
				}
			}

			return DsymutilLocation.FullName;
		}
	};
}
