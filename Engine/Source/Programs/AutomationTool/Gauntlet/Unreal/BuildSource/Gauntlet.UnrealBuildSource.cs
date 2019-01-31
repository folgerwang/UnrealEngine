// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using System.IO;
using System.Linq;

namespace Gauntlet
{
	public class UnrealBuildSource : IBuildSource
	{
		public string UnrealPath { get; protected set; }

		public string ProjectName { get; protected set; }

		public bool UsesSharedBuildType { get; protected set; }

		public string BuildName { get; protected set; }

		public IEnumerable<string> BuildPaths { get; protected set; }

		public string Branch { get; protected set; }

		public int Changelist { get; protected set; }

		public bool EditorValid { get; protected set; }

		protected List<IBuild> DiscoveredBuilds;

		public int BuildCount { get { return DiscoveredBuilds.Count; } }

		public UnrealBuildSource(string InProjectName, bool InUsesSharedBuildType, string InUnrealPath, string BuildReference) 
		{
			InitBuildSource(InProjectName, InUsesSharedBuildType, InUnrealPath, BuildReference, null);
		}

		public UnrealBuildSource(string InProjectName, bool InUsesSharedBuildType, string InUnrealPath, string BuildReference, Func<string, string> ResolutionDelegate)
		{
			InitBuildSource(InProjectName, InUsesSharedBuildType, InUnrealPath, BuildReference, ResolutionDelegate);
		}

		public UnrealBuildSource(string InProjectName, bool InUsesSharedBuildType, string InUnrealPath, string BuildReference, IEnumerable<string> InSearchPaths)
		{
			InitBuildSource(InProjectName, InUsesSharedBuildType, InUnrealPath, BuildReference, (string BuildRef) =>
			{
				foreach (string SearchPath in InSearchPaths)
				{
					DirectoryInfo SearchDir = new DirectoryInfo(Path.Combine(SearchPath, BuildRef));

					if (SearchDir.Exists)
					{
						return SearchDir.FullName;
					}
				}

				return null;
			});
		}

		public bool CanSupportPlatform(UnrealTargetPlatform Platform)
		{
			// todo - need to return values of discovered builds
			return Platform == UnrealTargetPlatform.Win64
				|| Platform == UnrealTargetPlatform.PS4
				|| Platform == UnrealTargetPlatform.XboxOne
				|| Platform == UnrealTargetPlatform.Android
				|| Platform == UnrealTargetPlatform.Mac;
		}


		protected void InitBuildSource(string InProjectName, bool InUsesSharedBuildType, string InUnrealPath, string InBuildArgument, Func<string, string> ResolutionDelegate)
		{
			UnrealPath = InUnrealPath;
			UsesSharedBuildType = InUsesSharedBuildType;

			ResolveProjectName(InProjectName);

			// Resolve the build argument into something meaningful
			string ResolvedBuildName;
			IEnumerable<string> ResolvedPaths = null;

			if (!ResolveBuildReference(InBuildArgument, ResolutionDelegate, out ResolvedPaths, out ResolvedBuildName))
			{
				throw new AutomationException("Unable to resolve {0} to a valid build", InBuildArgument);
			}

			BuildName = ResolvedBuildName;
			BuildPaths = ResolvedPaths;

			DiscoveredBuilds = DiscoverBuilds();

			if (DiscoveredBuilds.Count() == 0)
			{
				throw new AutomationException("No builds were discovered from resolved build argument {0}", InBuildArgument);
			}

			// any Branch/CL info?
			Match M = Regex.Match(BuildName, @"(\+\+.+)-CL-(\d+)");

			if (M.Success)
			{
				Branch = M.Groups[1].Value.Replace("+", "/");
				Changelist = Convert.ToInt32(M.Groups[2].Value);
			}
			else
			{
				Branch = "";
				Changelist = 0;
			}

			// allow user overrides (TODO - centralize all this!)
			Branch = Globals.Params.ParseValue("branch", Branch);
			Changelist = Convert.ToInt32(Globals.Params.ParseValue("changelist", Changelist.ToString()));
		}

		void ResolveProjectName(string InProjectName)
		{
			// figure out the game name - they may have passed Foo or FooGame
			string ProjectOption1 = Path.Combine(UnrealPath, InProjectName, string.Format(@"{0}.uproject", InProjectName));
			string ProjectOption2 = Path.Combine(UnrealPath, InProjectName + "Game", string.Format(@"{0}Game.uproject", InProjectName));
			string ProjectPath = "";

			if (File.Exists(ProjectOption1))
			{
				ProjectPath = ProjectOption1;
				ProjectName = InProjectName;
			}
			else if (File.Exists(ProjectOption2))
			{
				ProjectPath = ProjectOption2;
				ProjectName = InProjectName + "Game";
			}
			else
			{
				// Not resolving is not an error, may just be staged build. Also above doesn't account for sample projects.
				ProjectName = InProjectName;
			}
		}

		virtual protected bool ResolveBuildReference(string InBuildReference, Func<string, string> ResolutionDelegate, out IEnumerable<string> OutBuildPaths, out string OutBuildName)
		{
			OutBuildName = null;
			OutBuildPaths = null;

			if (string.IsNullOrEmpty(InBuildReference))
			{
				return false;
			}

			if (InBuildReference.Equals("AutoP4", StringComparison.InvariantCultureIgnoreCase))
			{
				if (!CommandUtils.P4Enabled)
				{
					throw new AutomationException("-Build=AutoP4 requires -P4");
				}
				if (CommandUtils.P4Env.Changelist < 1000)
				{
					throw new AutomationException("-Build=AutoP4 requires a CL from P4 and we have {0}", CommandUtils.P4Env.Changelist);
				}

				string BuildRoot = CommandUtils.CombinePaths(CommandUtils.RootBuildStorageDirectory());
				string CachePath = InternalUtils.GetEnvironmentVariable("UE-BuildCachePath", "");

				string SrcBuildPath = CommandUtils.CombinePaths(BuildRoot, ProjectName);
				string SrcBuildPath2 = CommandUtils.CombinePaths(BuildRoot, ProjectName.Replace("Game", "").Replace("game", ""));

				string SrcBuildPath_Cache = CommandUtils.CombinePaths(CachePath, ProjectName);
				string SrcBuildPath2_Cache = CommandUtils.CombinePaths(CachePath, ProjectName.Replace("Game", "").Replace("game", ""));

				if (!InternalUtils.SafeDirectoryExists(SrcBuildPath))
				{
					if (!InternalUtils.SafeDirectoryExists(SrcBuildPath2))
					{
						throw new AutomationException("-Build=AutoP4: Neither {0} nor {1} exists.", SrcBuildPath, SrcBuildPath2);
					}
					SrcBuildPath = SrcBuildPath2;
					SrcBuildPath_Cache = SrcBuildPath2_Cache;
				}
				string SrcCLPath = CommandUtils.CombinePaths(SrcBuildPath, CommandUtils.EscapePath(CommandUtils.P4Env.Branch) + "-CL-" + CommandUtils.P4Env.Changelist.ToString());
				string SrcCLPath_Cache = CommandUtils.CombinePaths(SrcBuildPath_Cache, CommandUtils.EscapePath(CommandUtils.P4Env.Branch) + "-CL-" + CommandUtils.P4Env.Changelist.ToString());
				if (!InternalUtils.SafeDirectoryExists(SrcCLPath))
				{
					throw new AutomationException("-Build=AutoP4: {0} does not exist.", SrcCLPath);
				}

				if (InternalUtils.SafeDirectoryExists(SrcCLPath_Cache))
				{
					InBuildReference = SrcCLPath_Cache;
				}
				else
				{
					InBuildReference = SrcCLPath;
				}
				Log.Verbose("Using AutoP4 path {0}", InBuildReference);
			}

			// BuildParam could be a path, a name that we should resolve to a path, Staged, or Editor
			DirectoryInfo BuildDir = new DirectoryInfo(InBuildReference);

			if (BuildDir.Exists)
			{
				// Easy option first - is this a full path?
				OutBuildName = BuildDir.Name;
				OutBuildPaths = new string[] { BuildDir.FullName };
			}
			else if (BuildDir.Name.Equals("local", StringComparison.OrdinalIgnoreCase) || BuildDir.Name.Equals("staged", StringComparison.OrdinalIgnoreCase))
			{
				string ProjectDir = Path.GetDirectoryName(UnrealHelpers.GetProjectPath(ProjectName));

				if (string.IsNullOrEmpty(ProjectDir))
				{
					throw new AutomationException("Could not find uproject for {0}.", ProjectName);
				}

				// First special case - "Staged" means use whats locally staged
				OutBuildName = "Local";
				string StagedPath = Path.Combine(ProjectDir, "Saved", "StagedBuilds");

				if (Directory.Exists(StagedPath) == false)
				{
					Log.Error("BuildReference was Staged but staged directory {0} not found", StagedPath);
					return false;
				}
				
				// include binaries path for packaged builds
				string BinariesPath = Path.Combine(ProjectDir, "Binaries");

				OutBuildPaths = new string[] { StagedPath, BinariesPath };
			}
			else if (BuildDir.Name.Equals("editor", StringComparison.OrdinalIgnoreCase))
			{
				// Second special case - "Editor" means run using the editor, no path needed
				OutBuildName = "Editor";
				OutBuildPaths = new string[] { Environment.CurrentDirectory };
			}
			else
			{
				// todo - make this more generic
				if (BuildDir.Name.Equals("usesyncedbuild", StringComparison.OrdinalIgnoreCase))
				{
					BuildVersion Version;
					if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
					{
						InBuildReference = Version.BranchName + "-CL-" + Version.Changelist.ToString();
					}
				}

				// See if it's in the passed locations
				if (ResolutionDelegate != null)
				{
					string FullPath = ResolutionDelegate(InBuildReference);

					if (string.IsNullOrEmpty(FullPath) == false)
					{
						DirectoryInfo Di = new DirectoryInfo(FullPath);

						if (Di.Exists == false)
						{
							throw new AutomationException("Resolution delegate returned non existent path");
						}

						OutBuildName = Di.Name;
						OutBuildPaths = new string[] { Di.FullName };
					}
				}
			}

			if (string.IsNullOrEmpty(OutBuildName) || (OutBuildPaths == null || OutBuildPaths.Count() == 0))
			{
				Log.Error("Unable to resolve build argument '{0}'", InBuildReference);
				return false;
			}

			return true;
		}

		virtual protected bool ShouldMakeBuildAvailable(IBuild InBuild)
		{
			return true;
		}

		virtual protected List<IBuild> DiscoverBuilds()
		{
			var BuildList = new List<IBuild>();

			if (BuildPaths.Count() > 0)
			{
				foreach (string Path in BuildPaths)
				{
					IEnumerable<IFolderBuildSource> BuildSources = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IFolderBuildSource>();

					foreach (var BS in BuildSources)
					{
						IEnumerable<IBuild> Builds = BS.GetBuildsAtPath(ProjectName, Path);

						BuildList.AddRange(Builds);
					}
				}
			}

			// Editor?
			IBuild EditorBuild = CreateEditorBuild(ProjectName, UnrealPath);

			if (EditorBuild != null)
			{
				BuildList.Add(EditorBuild);
			}
			else
			{
				Log.Verbose("No editor found for {0}, editor-builds will be unavailable", ProjectName);
			}

			// give higher level code a chance to reject stuff
			return BuildList.Where(B => ShouldMakeBuildAvailable(B)).ToList();
		}

		IEnumerable<IBuild> GetMatchingBuilds(UnrealTargetRole InRole, UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, BuildFlags InFlags)
		{
			if (DiscoveredBuilds == null)
			{
				return new IBuild[0];
			}

			IEnumerable<IBuild> MatchingBuilds = DiscoveredBuilds.Where((B) => 
			{
				if (B.Platform == InPlatform
					&& B.CanSupportRole(InRole)
					&& B.Configuration == InConfiguration
					&& (B.Flags & InFlags) == InFlags)
				{
					return true;
				}

				return false;
			});

			if (MatchingBuilds.Count() > 0)
			{
				return MatchingBuilds;
			}

			MatchingBuilds = DiscoveredBuilds.Where((B) =>
			{
				if ((InFlags & BuildFlags.CanReplaceExecutable) == BuildFlags.CanReplaceExecutable)
				{
					if (B.Platform == InPlatform
						&& B.CanSupportRole(InRole)
						&& (B.Flags & InFlags) == InFlags)
					{
						Log.Warning("Build did not have configuration {0} for {1}, but selecting due to presence of -dev flag",
							InConfiguration, InPlatform);
						return true;
					}
				}

				return false;
			});

			return MatchingBuilds;
		}


		public bool CanSupportRole(UnrealSessionRole Role, ref List<string> Reasons)
		{
			if (Role.RoleType.UsesEditor() && string.IsNullOrEmpty(UnrealPath))
			{
				Reasons.Add(string.Format("Role {0} wants editor but no path to Unreal exists", Role));
				return false;
			}

			// null platform. Need a better way of specifying this
			if (Role.IsNullRole())
			{
				return true;
			}

			// Query our build list
			var MatchingBuilds = GetMatchingBuilds(Role.RoleType, Role.Platform, Role.Configuration, Role.RequiredBuildFlags);
		
			if (MatchingBuilds.Count() > 0)
			{
				return true;
			}

			Reasons.Add(string.Format("No build at {0} that matches {1}", string.Join(",", BuildPaths), Role.ToString()));

			return false;
		}


		virtual public UnrealAppConfig CreateConfiguration(UnrealSessionRole Role)
		{
			List<string> Issues = new List<string>();

			Log.Verbose("Creating configuration Role {0}", Role);
			if (!CanSupportRole(Role, ref Issues))
			{
				Issues.ForEach(S => Log.Error(S));
				return null;
			}

			UnrealAppConfig Config = new UnrealAppConfig();

			Config.Name = this.BuildName;
			Config.ProjectName = ProjectName;
			Config.ProcessType = Role.RoleType;
			Config.Platform = Role.Platform;
			Config.Configuration = Role.Configuration;
			Config.CommandLine = "";
            Config.FilesToCopy = new List<UnrealFileToCopy>();

			// new system of retrieving and encapsulating the info needed to install/launch. Android & Mac
			Config.Build = GetMatchingBuilds(Role.RoleType, Role.Platform, Role.Configuration, Role.RequiredBuildFlags).FirstOrDefault();

			if (Config.Build == null && Role.IsNullRole() == false)
			{
				var SupportedBuilds = String.Join("\n", DiscoveredBuilds.Select(B => B.ToString()));

				Log.Info("Available builds:\n{0}", SupportedBuilds);
				throw new AutomationException("No build found that can support a role of {0}.", Role);
			}

			if (Role.Options != null)
			{
				Role.Options.ApplyToConfig(Config);
			}

			if (string.IsNullOrEmpty(Role.CommandLine) == false)
			{
				Config.CommandLine += " " + Role.CommandLine;
			}

			bool IsContentOnlyProject = (Config.Build.Flags & BuildFlags.ContentOnlyProject) == BuildFlags.ContentOnlyProject;

			// Add in editor - TODO, should this be in the editor build?
			if (Role.RoleType.UsesEditor() || IsContentOnlyProject)
			{
				string ProjectParam = ProjectName;

				// if content only we need to provide a relative path to the uproject.
				if (IsContentOnlyProject)
				{
					ProjectParam = string.Format("../../../{0}/{0}.uproject", ProjectName);
				}

				// project must be first
				Config.CommandLine = ProjectParam + " " + Config.CommandLine;

				// add in -game or -server
				if (Role.RoleType.IsClient())
				{
					Config.CommandLine += " -game";
				}
				else if (Role.RoleType.IsServer())
				{
					Config.CommandLine += " -server";
				}
			}

            if (Role.FilesToCopy != null)
            {
                Config.FilesToCopy = Role.FilesToCopy;
            }
			Config.CommandLine = GenerateProcessedCommandLine(Config.CommandLine);
			return Config;
		}

		/// <summary>
		/// Remove all duplicate flags and combine any execcmd strings that might be floating around in the commandline.
		/// </summary>
		/// <param name="InCommandLine"></param>
		/// <returns></returns>
		private string GenerateProcessedCommandLine(string InCommandLine)
		{

			// Break down Commandline into individual tokens 
			Dictionary<string, string> CommandlineTokens = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
			// turn Name(p1,etc) into a collection of Name|(p1,etc) groups
			MatchCollection Matches = Regex.Matches(InCommandLine, "(?<option>-?[\\w\\d.:\\[\\]\\/\\\\]+)(=(?<value>(\"([^\"]*)\")|(\\S+)))?");

			foreach (Match M in Matches)
			{
				if (M.Groups["option"] == null || string.IsNullOrWhiteSpace(M.Groups["option"].ToString()))
				{
					Log.Warning("Unable to parse option in commandline. Please check syntax/regex. This should never be hit.");
					continue;
				}

				string Name = M.Groups["option"].ToString().Trim();

				string Params = M.Groups["value"] != null ? M.Groups["value"].ToString() : string.Empty;

				if (CommandlineTokens.ContainsKey(Name))
				{

					if (string.IsNullOrWhiteSpace(Params))
					{
						Log.Info(string.Format("Duplicate flag {0} found and ignored. Please fix this as it will increase in severity in 01/2019. ", Name));
					}
					else if (Name.ToLower() == "-execcmds")
					{
						// Special cased as execcmds is something that is totally able to be appended to. Requote everything when we're done.
						CommandlineTokens[Name] = string.Format("\"{0}, {1}\"", CommandlineTokens[Name].Replace("\"", ""), Params.Replace("\"", ""));
					}
					else
					{
						if (CommandlineTokens[Name] == Params)
						{
							Log.Info(string.Format("Duplicate flag {0}={1} found and ignored. Please fix this as this log line will increase in severity in 01/2019. ", Name, Params));
						}
						else
						{
							Log.Warning(string.Format("Multiple values for flag {0} found: {1} and {2}. The former value will be discarded. ", Name, CommandlineTokens[Name], Params));
							CommandlineTokens[Name] = Params.Trim();
						}
					}
				}
				else
				{
					CommandlineTokens.Add(Name, (Params.Contains(' ') && !Params.Contains('\"')) ? string.Format("\"{0}\"", Params) : Params);
				}
			}

			string CommandlineToReturn = "";
			foreach (string DictKey in CommandlineTokens.Keys)
			{
				CommandlineToReturn += string.Format("{0}{1} ",
					DictKey,
					string.IsNullOrWhiteSpace(CommandlineTokens[DictKey]) ? "" : string.Format("={0}", CommandlineTokens[DictKey])
					);
			}
			Gauntlet.Log.Verbose(string.Format("Pre-formatting Commandline: {0}", InCommandLine));
			Gauntlet.Log.Verbose(string.Format("Post-formatting Commandline: {0}", CommandlineToReturn));

			return CommandlineToReturn;
		}


		/// <summary>
		/// Given a platform, a build config, and true/false for client, returns the path to the binary for that config. E.g.
		/// Win64, Shipping, false = Binaries\Win64\FooServer-Win64-Shipping.exe
		/// </summary>
		/// <param name="TargetPlatform"></param>
		/// <param name="BuildConfig"></param>
		/// <param name="IsClient"></param>
		/// <returns></returns>
		virtual public string GetRelativeExecutablePath(UnrealTargetRole TargetType, UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration TargetConfiguration)
		{
			string ExePath;

			if (TargetType.UsesEditor())
			{
				ExePath = string.Format("Engine/Binaries/{0}/UE4Editor{1}", BuildHostPlatform.Current.Platform, Platform.GetExeExtension(TargetPlatform));
			}
			else
			{
				string BuildType = "";

				if (TargetType == UnrealTargetRole.Client)
				{
					if (!UsesSharedBuildType)
					{
						BuildType = "Client";
					}
				}
				else if (TargetType == UnrealTargetRole.Server)
				{
					if (!UsesSharedBuildType)
					{
						BuildType = "Server";
					}
				}

				bool IsRunningDev = Globals.Params.ParseParam("dev");

				// Turn FooGame into Foo
				string ExeBase = ProjectName.Replace("Game", "");

				if (TargetPlatform == UnrealTargetPlatform.Android)
				{
					// use the archive results for android.
					//var AndroidSource = new AndroidBuild(ProjectName, GetPlatformPath(TargetType, TargetPlatform), TargetConfiguration);

					// We always (currently..) need to be able to replace the command line
					BuildFlags Flags = BuildFlags.CanReplaceCommandLine;
					if (IsRunningDev)
					{
						Flags |= BuildFlags.CanReplaceExecutable;
					}
                    if (Globals.Params.ParseParam("bulk"))
                    {
                        Flags |= BuildFlags.Bulk;
                    }

                    var Build = GetMatchingBuilds(TargetType, TargetPlatform, TargetConfiguration, Flags).FirstOrDefault();

					if (Build != null)
					{
						AndroidBuild AndroidBuild = Build as AndroidBuild;
						ExePath = AndroidBuild.SourceApkPath;
					}
					else
					{
						throw new AutomationException("No suitable build for {0} found at {1}", TargetPlatform, string.Join(",", BuildPaths));
					}

					//ExePath = AndroidSource.SourceApkPath;			
				}
				else
				{
					string ExeFileName = string.Format("{0}{1}", ExeBase, BuildType);

					if (TargetConfiguration != UnrealTargetConfiguration.Development)
					{
						ExeFileName += string.Format("-{0}-{1}", TargetPlatform.ToString(), TargetConfiguration.ToString());
					}

					ExeFileName += Platform.GetExeExtension(TargetPlatform);

					string BasePath = GetPlatformPath(TargetType, TargetPlatform);
					string ProjectBinary = string.Format("{0}\\Binaries\\{1}\\{2}", ProjectName, TargetPlatform.ToString(), ExeFileName);
					string StubBinary = Path.Combine(BasePath, ExeFileName);
					string DevBinary = Path.Combine(Environment.CurrentDirectory, ProjectBinary);

					string NonCodeProjectName = "UE4Game" + Platform.GetExeExtension(TargetPlatform);
					string NonCodeProjectBinary = Path.Combine(BasePath, "Engine", "Binaries", TargetPlatform.ToString());
					NonCodeProjectBinary = Path.Combine(NonCodeProjectBinary, NonCodeProjectName);

					// check the project binaries folder
					if (File.Exists(Path.Combine(BasePath, ProjectBinary)))
					{
						ExePath = ProjectBinary;
					}
					else if (File.Exists(StubBinary))
					{
						ExePath = Path.Combine(BasePath, ExeFileName);
					}
					else if (IsRunningDev && File.Exists(DevBinary))
					{
						ExePath = DevBinary;
					}
					else if (File.Exists(NonCodeProjectBinary))
					{
						ExePath = NonCodeProjectBinary;
					}
					else
					{
						List<string> CheckedFiles = new List<String>() { Path.Combine(BasePath, ProjectBinary), StubBinary, NonCodeProjectBinary };
						if (IsRunningDev)
						{
							CheckedFiles.Add(DevBinary);
						}

						throw new AutomationException("Executable not found, upstream compile job may have failed.  Could not find executable {0} within {1}, binaries checked: {2}", ExeFileName, BasePath, String.Join(" - ", CheckedFiles));
					}

				}
			}

			return Utils.SystemHelpers.CorrectDirectorySeparators(ExePath);
		}

		public string GetPlatformPath(UnrealTargetRole Type, UnrealTargetPlatform Platform)
		{
			if (Type.UsesEditor())
			{
				return UnrealPath;
			}

			string BuildPath = BuildPaths.ElementAt(0);

			if (string.IsNullOrEmpty(BuildPath))
			{
				return null;
			}

			string PlatformPath = Path.Combine(BuildPath, UnrealHelpers.GetPlatformName(Platform, Type, UsesSharedBuildType));

			// On some builds we stage the actual loose files into a "Staged" folder
			if (Directory.Exists(PlatformPath) && Directory.Exists(Path.Combine(PlatformPath, "staged")))
			{
				PlatformPath = Path.Combine(PlatformPath, "Staged");
			}

			// Urgh - build share uses a different style...
			if (Platform == UnrealTargetPlatform.Android && BuildName.Equals("Local", StringComparison.OrdinalIgnoreCase) == false)
			{
				PlatformPath = PlatformPath.Replace("Android_ETC2Client", "Android\\FullPackages");
			}

			return PlatformPath;
		}

		EditorBuild CreateEditorBuild(string InProjectName, string InUnrealPath)
		{
			if (string.IsNullOrEmpty(InUnrealPath))
			{
				return null;
			}

			// check for the editor
			string EditorExe = Path.Combine(InUnrealPath, GetRelativeExecutablePath(UnrealTargetRole.Editor, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development));

			if (!Utils.SystemHelpers.ApplicationExists(EditorExe))
			{
				return null;
			}

			// figure out the game name - they may have passed Foo or FooGame
			string ProjectPath = UnrealHelpers.GetProjectPath(InProjectName);

			if (File.Exists(ProjectPath))
			{
				ProjectName = InProjectName;
			}
			else
			{
				// todo - this is ok, because we want people to be able to run staged builds
				// where no uproject file is available.
				/*throw new AutomationException("Unable to find project file for {0}. Neither {1} nor {2} exists.",
					InProjectName, ProjectOption1, ProjectOption2);*/
				return null;
			}

			EditorBuild NewBuild = new EditorBuild(EditorExe);

			return NewBuild;

			//List<string> Empty = new List<string>();
			//return CanSupportRole(new UnrealSessionRole(UnrealRoleType.Editor, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development), ref Empty); ;
		}
	}
}
