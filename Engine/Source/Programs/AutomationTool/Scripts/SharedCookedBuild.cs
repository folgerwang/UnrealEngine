// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;
using System.Text.RegularExpressions;

public class SharedCookedBuild
{
	private static Task CopySharedCookedBuildTask = null;

	private static bool FindBestSharedCookedBuild(ref string FinalCookedBuildPath, string ProjectFullPath, UnrealTargetPlatform TargetPlatform, string CookPlatform, string SharedCookedBuildCL )
	{
		string BuildRoot = CommandUtils.P4Enabled ? CommandUtils.P4Env.Branch.Replace("/", "+") : "";
		int CurrentCLInt = CommandUtils.P4Enabled ? CommandUtils.P4Env.Changelist : 0;

		BuildVersion Version;
		if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
		{
			CurrentCLInt = Version.Changelist;
			BuildRoot = Version.BranchName;
		}
		System.GC.Collect();
		string CurrentCL = CurrentCLInt.ToString();


		FileReference ProjectFileRef = new FileReference(ProjectFullPath);
		// get network location 
		ConfigHierarchy Hierarchy = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFileRef), TargetPlatform);
		List<string> CookedBuildPaths;
		if (Hierarchy.GetArray("SharedCookedBuildSettings", "SharedCookedBuildPath", out CookedBuildPaths) == false)
		{
			CommandUtils.Log("Unable to copy shared cooked build: SharedCookedBuildPath not set in Engine.ini SharedCookedBuildSettings");
			return false;
		}

		const string MetaDataFilename = "\\Metadata\\DevelopmentAssetRegistry.bin";


		if (SharedCookedBuildCL == "usesyncedbuild")
		{
			foreach (string CookedBuildPath in CookedBuildPaths)
			{
				if (CurrentCL == "" && FinalCookedBuildPath.Contains("[CL]"))
				{
					CommandUtils.Log("Unable to copy shared cooked build: Unable to determine CL number from P4 or UGS, and is required by SharedCookedBuildPath");
					return false;
				}

				if (CurrentCL == "" && FinalCookedBuildPath.Contains("[BRANCHNAME]"))
				{
					CommandUtils.Log("Unable to copy shared cooked build: Unable to determine BRANCHNAME number from P4 or UGS, and is required by SharedCookedBuildPath");
					return false;
				}


				FinalCookedBuildPath = FinalCookedBuildPath.Replace("[CL]", CurrentCL.ToString());
				FinalCookedBuildPath = FinalCookedBuildPath.Replace("[BRANCHNAME]", BuildRoot);
				FinalCookedBuildPath = FinalCookedBuildPath.Replace("[PLATFORM]", CookPlatform);

				// make sure that the directory and metadata file exist.  otherwise this build might not be finished yet and we should skip it
				if (Directory.Exists(FinalCookedBuildPath))
				{
					if ( File.Exists( FinalCookedBuildPath + MetaDataFilename) )
					{
						return true;
					}
				}
			}
		}
		else if (SharedCookedBuildCL == "userecentbuild")
		{

			// build our CookedBUildPath into a regex which we can execute on the directories and extract info from



			string BestBuild = null;
			int BestCLNumber = 0;

			// find all the recent builds which are valid
			foreach (string CookedBuildPath in CookedBuildPaths)
			{
				int IndexOfFirstParam = CookedBuildPath.IndexOf("[");
				int CustomFolderStart = CookedBuildPath.LastIndexOf("\\", IndexOfFirstParam);

				string CookedBuildDirectory = CookedBuildPath.Substring(0, CustomFolderStart);

				string BuildNameWildcard = CookedBuildPath.Substring(CustomFolderStart);


				BuildNameWildcard += MetaDataFilename;

				FileFilter BuildSearch = new FileFilter();

				// we know the platform and the branch name;
				string BuildRule = BuildNameWildcard;
				BuildRule = BuildRule.Replace("[BRANCHNAME]", BuildRoot);
				BuildRule = BuildRule.Replace("[PLATFORM]", CookPlatform);

				string IncludeRule = BuildRule.Replace("[CL]", "*");
				string ForgetRule = BuildRule.Replace("[CL]", "*-PF-*"); // get rid of any preflights from the list... they don't count because who knows what they did...

				BuildSearch.AddRule(IncludeRule);
				BuildSearch.AddRule(ForgetRule, FileFilterType.Exclude);

				List<FileReference> ValidBuilds = BuildSearch.ApplyToDirectory(new DirectoryReference(CookedBuildDirectory), false);

				// figure out what the CL is
				string BuildNameRegex = String.Format(".*{0}", CookedBuildPath.Substring(CustomFolderStart));
				BuildNameRegex = BuildNameRegex.Replace("\\", "\\\\");
				BuildNameRegex = BuildNameRegex.Replace("[BRANCHNAME]", BuildRoot);
				BuildNameRegex = BuildNameRegex.Replace("+", "\\+");
				BuildNameRegex = BuildNameRegex.Replace("[PLATFORM]", CookPlatform);
				BuildNameRegex = BuildNameRegex.Replace("[CL]", "(?<CL>.*)");
				
				Regex ExtractCL = new Regex(BuildNameRegex);

				foreach ( FileReference ValidBuild in ValidBuilds )
				{
					string BuildPath = ValidBuild.FullName.Replace(MetaDataFilename, "");

					Match CLMatch = ExtractCL.Match(BuildPath);
					if ( CLMatch != null )
					{
						string CLNumber = CLMatch.Result("${CL}");
						int CLNumberInt = int.Parse(CLNumber);
						if ( CLNumberInt <= CurrentCLInt )
						{
							if ( CLNumberInt > BestCLNumber )
							{
								BestCLNumber = CLNumberInt;
								BestBuild = BuildPath;
							}
						}
					}
				}
				
			}

			if ( string.IsNullOrEmpty(BestBuild) )
			{
				return false;
			}

			FinalCookedBuildPath = BestBuild;
			return true;
		}


		return false;
	}

	private static bool CopySharedCookedBuildForTargetInternal(string CookedBuildPath, string CookPlatform, string LocalPath, bool bOnlyCopyAssetRegistry)
	{

		// check to see if we have already synced this build ;)
		var SyncedBuildFile = CommandUtils.CombinePaths(LocalPath, "SyncedBuild.txt");
		string BuildCL = "Invalid";
		if (File.Exists(SyncedBuildFile))
		{
			BuildCL = File.ReadAllText(SyncedBuildFile);
		}

		CommandUtils.Log("Attempting download of latest shared build from {0}", CookedBuildPath);

		string SavedBuildCL = string.Format( "{0} {1}", CookedBuildPath, bOnlyCopyAssetRegistry ? "RegistryOnly" : "" );

		if (BuildCL == SavedBuildCL)
		{
			CommandUtils.Log("Already downloaded latest shared build at CL {0}", SavedBuildCL);
			return false;
		}

		if (Directory.Exists(CookedBuildPath) == false)
		{
			CommandUtils.Log("Unable to copy shared cooked build: Unable to find shared build at location {0} check SharedCookedBuildPath in Engine.ini SharedCookedBuildSettings is correct", CookedBuildPath);
			return false;
		}
		
		// delete all the stuff
		CommandUtils.Log("Deleting previous shared build because it was out of date");
		CommandUtils.DeleteDirectory(LocalPath);
		Directory.CreateDirectory(LocalPath);



		string CookedBuildMetadataDirectory = Path.Combine(CookedBuildPath, "Metadata");
		CookedBuildMetadataDirectory = Path.GetFullPath(CookedBuildMetadataDirectory);
		string LocalBuildMetadataDirectory = Path.Combine(LocalPath, "Metadata");
		LocalBuildMetadataDirectory = Path.GetFullPath(LocalBuildMetadataDirectory);
		if (Directory.Exists(CookedBuildMetadataDirectory))
		{
			foreach (string FileName in Directory.EnumerateFiles(CookedBuildMetadataDirectory, "*.*", SearchOption.AllDirectories))
			{
				string SourceFileName = Path.GetFullPath(FileName);
				string DestFileName = SourceFileName.Replace(CookedBuildMetadataDirectory, LocalBuildMetadataDirectory);
				Directory.CreateDirectory(Path.GetDirectoryName(DestFileName));
				File.Copy(SourceFileName, DestFileName);
			}
		}

		if ( CopySharedCookedBuildTask != null )
		{
			WaitForCopy();
		}

		if (bOnlyCopyAssetRegistry == false)
		{
			CopySharedCookedBuildTask = Task.Run(() =>
				{
					// find all the files in the staged directory
					string CookedBuildStagedDirectory = Path.GetFullPath(Path.Combine(CookedBuildPath, "Staged"));
					string LocalBuildStagedDirectory = Path.GetFullPath(Path.Combine(LocalPath, "Staged"));
					if (Directory.Exists(CookedBuildStagedDirectory))
					{
						foreach (string FileName in Directory.EnumerateFiles(CookedBuildStagedDirectory, "*.*", SearchOption.AllDirectories))
						{
							string SourceFileName = Path.GetFullPath(FileName);
							string DestFileName = SourceFileName.Replace(CookedBuildStagedDirectory, LocalBuildStagedDirectory);
							Directory.CreateDirectory(Path.GetDirectoryName(DestFileName));
							File.Copy(SourceFileName, DestFileName);
						}
					}
					File.WriteAllText(SyncedBuildFile, SavedBuildCL);
				}
				);
		}
		else
		{
			File.WriteAllText(SyncedBuildFile, SavedBuildCL);
		}

		
		return true;
	}

	public static void CopySharedCookedBuildForTarget(string ProjectFullPath, UnrealTargetPlatform TargetPlatform, string CookPlatform, string BuildCl, bool bOnlyCopyAssetRegistry = false)
	{
		var LocalPath = CommandUtils.CombinePaths(Path.GetDirectoryName(ProjectFullPath), "Saved", "SharedIterativeBuild", CookPlatform);
		
		string CookedBuildPath = null;
		if ( FindBestSharedCookedBuild(ref CookedBuildPath, ProjectFullPath, TargetPlatform, CookPlatform, BuildCl) )
		{
			CopySharedCookedBuildForTargetInternal(CookedBuildPath, CookPlatform, LocalPath, bOnlyCopyAssetRegistry);
		}

		return;
	}

	public static void CopySharedCookedBuild(ProjectParams Params)
	{

		if (!Params.NoClient)
		{
			foreach (var ClientPlatform in Params.ClientTargetPlatforms)
			{
				// Use the data platform, sometimes we will copy another platform's data
				var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
				string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(false, Params.Client);
				CopySharedCookedBuildForTarget(Params.RawProjectPath.FullName, ClientPlatform.Type, PlatformToCook, Params.IterateSharedCookedBuild);
			}
		}
		if (Params.DedicatedServer)
		{
			foreach (var ServerPlatform in Params.ServerTargetPlatforms)
			{
				// Use the data platform, sometimes we will copy another platform's data
				var DataPlatformDesc = Params.GetCookedDataPlatformForServerTarget(ServerPlatform);
				string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(true, false);
				CopySharedCookedBuildForTarget(Params.RawProjectPath.FullName, ServerPlatform.Type, PlatformToCook, Params.IterateSharedCookedBuild);
			}
		}
	}

	public static void WaitForCopy()
	{
		if (CopySharedCookedBuildTask != null)
		{
			CopySharedCookedBuildTask.Wait();
		}
	}

}