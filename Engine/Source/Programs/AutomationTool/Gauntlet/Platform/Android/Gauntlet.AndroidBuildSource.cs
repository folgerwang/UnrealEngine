// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;

namespace Gauntlet
{


	public class AndroidBuild : IBuild
	{
		public UnrealTargetConfiguration Configuration { get; protected set; }

		public string SourceApkPath;

		public Dictionary<string, string> FilesToInstall;

		public string AndroidPackageName;

		public BuildFlags Flags { get; protected set; }

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.Android; } }

		public AndroidBuild(UnrealTargetConfiguration InConfig, string InAndroidPackageName, string InApkPath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
		{
			Configuration = InConfig;
			AndroidPackageName = InAndroidPackageName;
			SourceApkPath = InApkPath;
			FilesToInstall = InFilesToInstall;
			Flags = InFlags;
		}

		public bool CanSupportRole(UnrealTargetRole RoleType)
		{
			if (RoleType.IsClient())
			{
				return true;
			}

			return false;
		}

		public static IEnumerable<AndroidBuild> CreateFromPath(string InProjectName, string InPath)
		{
			string BuildPath = InPath;

			List<AndroidBuild> DiscoveredBuilds = new List<AndroidBuild>();

			DirectoryInfo Di = new DirectoryInfo(BuildPath);

			
			// find all install batchfiles
			FileInfo[] InstallFiles = Di.GetFiles("Install_*");

			foreach (FileInfo Fi in InstallFiles)
			{
				// todo 
				if (Fi.FullName.Contains("armv7"))
				{
					continue;
				}

                // Unreal naming. In test & shipping the platform name and config will be includedn
                string RegEx = "Install_(.+?)(client|game|server)-(Android)?-?(test|shipping)?-?(.+).bat";

				Match Info = Regex.Match(Fi.Name, RegEx, RegexOptions.IgnoreCase);

				if (Info.Success == false)
				{
					continue;
				}

				string TargetName = Info.Groups[1].ToString();
				string TypeName = Info.Groups[2].ToString();
				string ConfigName = Info.Groups[4].ToString();

				if (string.IsNullOrEmpty(ConfigName))
				{
					ConfigName = "Development";
				}


				Log.Verbose("Pulling install data from {0}", Fi.FullName);

				string AbsPath = Fi.Directory.FullName;

				// read contents and replace linefeeds (regex doesn't stop on them :((
				string BatContents = File.ReadAllText(Fi.FullName).Replace(Environment.NewLine, "\n");

				// Replace .bat with .apk and strip up to and including the first _, that is then our APK name
				string SourceApkPath = Regex.Replace(Fi.Name, ".bat", ".apk", RegexOptions.IgnoreCase);
				SourceApkPath = SourceApkPath.Substring(SourceApkPath.IndexOf("_") + 1);
				SourceApkPath = Path.Combine(AbsPath, SourceApkPath);

				// save com.companyname.product
				string AndroidPackageName = Regex.Match(BatContents, @"uninstall\s+(com\..+)").Groups[1].ToString();

				// pull all OBBs (probably just one..)
				var OBBMatches = Regex.Matches(BatContents, @"push\s+(.+?)\s+(.+)");

				// save them as a dict of full paths as keys and dest paths as values
				Dictionary<string, string> FilesToInstall = OBBMatches.Cast<Match>().ToDictionary(M => Path.Combine(AbsPath, M.Groups[1].ToString()), M => M.Groups[2].ToString());
	
				if (string.IsNullOrEmpty(SourceApkPath))
				{
					Log.Warning("No APK found for build at {0}", Fi.FullName);
					continue;
				}

				if (string.IsNullOrEmpty(AndroidPackageName))
				{
					Log.Warning("No product name found for build at {0}", Fi.FullName);
					continue;
				}

				UnrealTargetConfiguration UnrealConfig;
				if (Enum.TryParse(ConfigName, true, out UnrealConfig) == false)
				{
					Log.Warning("Could not determine Unreal Config type from '{0}'", ConfigName);
					continue;
				}

				// Android builds are always packaged, and we can always replace the command line
				BuildFlags Flags = BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine;

				// if there's data then the pak files are in an obb and we can sub in a new exe
				if (FilesToInstall.Count() > 0)
				{
					Flags |= BuildFlags.CanReplaceExecutable;
				}
                if (AbsPath.Contains("Bulk"))
                {
                    Flags |= BuildFlags.Bulk;
                }

                AndroidBuild NewBuild = new AndroidBuild(UnrealConfig, AndroidPackageName, SourceApkPath, FilesToInstall, Flags);

				DiscoveredBuilds.Add(NewBuild);

				Log.Verbose("Found {0} {1} build at {2}", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);

			}

			return DiscoveredBuilds;
		}
	}
	
	public class AndroidBuildSource : IFolderBuildSource
	{	
		public string BuildName { get { return "AndroidBuildSource";  } }

		public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == UnrealTargetPlatform.Android;
		}

		public string ProjectName { get; protected set; }

		public AndroidBuildSource()
		{
		}

		public List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{

			List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			if (PathDI.Name.IndexOf("Android", StringComparison.OrdinalIgnoreCase) >= 0)
			{
				AllDirs.Add(PathDI);
			}

			// find all directories that begin with Android
			DirectoryInfo[] AndroidDirs = PathDI.GetDirectories("Android*", SearchOption.TopDirectoryOnly);

			AllDirs.AddRange(AndroidDirs);

			List<DirectoryInfo> DirsToRecurse = AllDirs;

			// now get subdirs
			while (MaxRecursion-- > 0)
			{
				List<DirectoryInfo> DiscoveredDirs = new List<DirectoryInfo>();

				DirsToRecurse.ToList().ForEach((D) =>
				{
					DiscoveredDirs.AddRange(D.GetDirectories("*", SearchOption.TopDirectoryOnly));
				});

				AllDirs.AddRange(DiscoveredDirs);
				DirsToRecurse = DiscoveredDirs;
			}

			//AndroidBuildSource BuildSource = null;
			List<IBuild> Builds = new List<IBuild>();

            string AndroidBuildFilter = Globals.Params.ParseValue("AndroidBuildFilter", "");
            foreach (DirectoryInfo Di in AllDirs)
			{
				IEnumerable<AndroidBuild> FoundBuilds = AndroidBuild.CreateFromPath(InProjectName, Di.FullName);

				if (FoundBuilds != null)
				{
                    if (!string.IsNullOrEmpty(AndroidBuildFilter))
                    {
                        //IndexOf used because Contains must be case-sensitive
                        FoundBuilds = FoundBuilds.Where(B => B.SourceApkPath.IndexOf(AndroidBuildFilter, StringComparison.OrdinalIgnoreCase) >= 0);
                    }
                    Builds.AddRange(FoundBuilds);
				}
			}

			return Builds;
		}

		/*public AndroidBuild GetBuild(UnrealTargetConfiguration InConfig, BuildFlags InFlags)
		{
			return Builds.Where((B) => {
				return B.Configuration == InConfig && (B.Flags & InFlags) > 0;
				}).FirstOrDefault();
		}*/
	}
}