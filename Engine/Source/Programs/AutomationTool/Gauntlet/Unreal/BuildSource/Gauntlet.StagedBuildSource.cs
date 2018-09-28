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

	public class EditorBuild : IBuild 
	{
		public UnrealTargetPlatform Platform { get { return BuildHostPlatform.Current.Platform; } }

		public UnrealTargetConfiguration Configuration { get { return UnrealTargetConfiguration.Development; } }

		public BuildFlags Flags { get { return BuildFlags.CanReplaceCommandLine | BuildFlags.Loose; } }

		public bool CanSupportRole(UnrealTargetRole InRoleType) { return InRoleType.UsesEditor(); }

		public string ExecutablePath { get; protected set; }

		public EditorBuild(string InExecutablePath)
		{
			ExecutablePath = InExecutablePath;
		}
	}


	public class StagedBuild : IBuild
	{
		public UnrealTargetPlatform Platform { get; protected set; }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public UnrealTargetRole Role { get; protected set; }

		public BuildFlags Flags { get; protected set; }

		public string BuildPath { get; protected set; }

		public string ExecutablePath { get; protected set; }

		public virtual bool CanSupportRole(UnrealTargetRole InRoleType) { return InRoleType == Role; }

		public StagedBuild(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfig, UnrealTargetRole InRole, string InBuildPath, string InExecutablePath)
		{
			Platform = InPlatform;
			Configuration = InConfig;
			Role = InRole;
			BuildPath = InBuildPath;
			ExecutablePath = InExecutablePath;
			Flags = BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable | BuildFlags.Loose;
		}

		public static IEnumerable<T> CreateFromPath<T>(UnrealTargetPlatform InPlatform, string InProjectName, string InPath, string InExecutableExtension)
			where T : StagedBuild
		{
			string BuildPath = InPath;

			List<T> DiscoveredBuilds = new List<T>();

			// Turn FooGame into just Foo as we need to check for client/server builds too
			string ShortName = Regex.Replace(InProjectName, "Game", "", RegexOptions.IgnoreCase);

			string ContentPath = Path.Combine(InPath, InProjectName, "Content", "Paks");

			if (Directory.Exists(ContentPath))
			{
				string EngineBinaryPath = Path.Combine(InPath, "Engine", "Binaries", InPlatform.ToString());
				string GameBinaryPath = Path.Combine(InPath, InProjectName, "Binaries", InPlatform.ToString());

				// Executable will either be Project*.exe or for content-only UE4Game.exe
				string[] ExecutableMatches = new string[]
				{
					ShortName + "*" + InExecutableExtension,
					"UE4Game*" + InExecutableExtension,
				};

				// check 
				// 1) Path/Project/Binaries/Platform
				// 2) Path (content only builds on some platforms write out a stub exe here)
				// 3) path/Engine/Binaries/Platform 

				string[] ExecutablePaths = new string[]
				{
					Path.Combine(InPath, InProjectName, "Binaries", InPlatform.ToString()),
					Path.Combine(InPath),
					Path.Combine(InPath, "Engine", "Binaries", InPlatform.ToString()),
				};

				List<FileSystemInfo> Binaries = new List<FileSystemInfo>();

				foreach (var BinaryPath in ExecutablePaths)
				{
					if (Directory.Exists(BinaryPath))
					{
						DirectoryInfo Di = new DirectoryInfo(BinaryPath);

						foreach (var FileMatch in ExecutableMatches)
						{
							// Look at files & directories since apps on Mac are bundles
							FileSystemInfo[] AppFiles = Di.GetFileSystemInfos(FileMatch);
							Binaries.AddRange(AppFiles);
						}
					}
				}

				foreach (FileSystemInfo App in Binaries)
				{
					UnrealTargetConfiguration Config = UnrealHelpers.GetConfigurationFromExecutableName(InProjectName, App.Name);
					UnrealTargetRole Role = UnrealHelpers.GetRoleFromExecutableName(InProjectName, App.Name);

					if (Config != UnrealTargetConfiguration.Unknown && Role != UnrealTargetRole.Unknown)
					{
						// store the exe path as relative to the staged dir path
						T NewBuild = Activator.CreateInstance(typeof(T), new object[] { InPlatform, Config, Role, InPath, Utils.SystemHelpers.MakePathRelative(App.FullName, InPath) }) as T;

						if (App.Name.StartsWith("UE4Game", StringComparison.OrdinalIgnoreCase))
						{
							NewBuild.Flags |= BuildFlags.ContentOnlyProject;
						}

						DiscoveredBuilds.Add(NewBuild);
					}
				}
			}

			return DiscoveredBuilds;
		}
	}


	public abstract class StagedBuildSource<T> : IFolderBuildSource 
		where T : StagedBuild
	{
		public abstract string BuildName { get; }

		public abstract UnrealTargetPlatform Platform { get; }

		public abstract string PlatformFolderPrefix { get; }

		virtual public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == Platform; 
		}

		virtual public string ExecutableExtension
		{
			get { return AutomationTool.Platform.GetExeExtension(Platform); }
		}

		public virtual List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{
			List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			// If the path is directly to a platform folder, add us
			if (PathDI.Name.IndexOf(PlatformFolderPrefix, StringComparison.OrdinalIgnoreCase) >= 0)
			{
				AllDirs.Add(PathDI);
			}

			// Assume it's a folder of all build types so find all sub directories that begin with the foldername for this platform
			IEnumerable<DirectoryInfo> MatchingDirs = PathDI.GetDirectories("*", SearchOption.TopDirectoryOnly);

			MatchingDirs = MatchingDirs.Where(D => D.Name.StartsWith(PlatformFolderPrefix, StringComparison.OrdinalIgnoreCase)).ToArray();

			AllDirs.AddRange(MatchingDirs);

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

			// every directory that contains a valid build should have at least two things, an Engine folder and a ProjectName folder
			AllDirs = AllDirs.Where(D =>
			{
				var SubDirs = D.GetDirectories();
				return SubDirs.Any(SD => SD.Name.Equals("Engine", StringComparison.OrdinalIgnoreCase)) &&
						SubDirs.Any(SD => SD.Name.Equals(InProjectName, StringComparison.OrdinalIgnoreCase));

			}).ToList();

			List<IBuild> Builds = new List<IBuild>();

			foreach (DirectoryInfo Di in AllDirs)
			{
				IEnumerable<IBuild> FoundBuilds = StagedBuild.CreateFromPath<T>(Platform, InProjectName, Di.FullName, ExecutableExtension);

				if (FoundBuilds != null)
				{
					Builds.AddRange(FoundBuilds);
				}
			}

			return Builds;
		}
	}

}
