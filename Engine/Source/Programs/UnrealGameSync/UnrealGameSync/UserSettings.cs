// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum BuildConfig
	{
		Debug,
		DebugGame,
		Development,
	}

	enum TabLabels
	{
		Stream,
		WorkspaceName,
		WorkspaceRoot,
		ProjectFile,
	}

	enum BisectState
	{
		Include,
		Exclude,
		Pass,
		Fail,
	}

	enum UserSelectedProjectType
	{
		Client,
		Local
	}

	class UserSelectedProjectSettings
	{
		public readonly string ServerAndPort;
		public readonly string UserName;
		public readonly UserSelectedProjectType Type;
		public readonly string ClientPath;
		public readonly string LocalPath;

		public UserSelectedProjectSettings(string ServerAndPort, string UserName, UserSelectedProjectType Type, string ClientPath, string LocalPath)
		{
			this.ServerAndPort = ServerAndPort;
			this.UserName = UserName;
			this.Type = Type;
			this.ClientPath = ClientPath;
			this.LocalPath = LocalPath;
		}

		public static bool TryParseConfigEntry(string Text, out UserSelectedProjectSettings Project)
		{
			ConfigObject Object = new ConfigObject(Text);

			UserSelectedProjectType Type;
			if(Enum.TryParse(Object.GetValue("Type", ""), out Type))
			{
				string ServerAndPort = Object.GetValue("ServerAndPort", null);
				if(String.IsNullOrWhiteSpace(ServerAndPort))
				{
					ServerAndPort = null;
				}

				string UserName = Object.GetValue("UserName", null);
				if(String.IsNullOrWhiteSpace(UserName))
				{
					UserName = null;
				}

				string LocalPath = Object.GetValue("LocalPath", null);
				if(String.IsNullOrWhiteSpace(LocalPath))
				{
					LocalPath = null;
				}

				string ClientPath = Object.GetValue("ClientPath", null);
				if(String.IsNullOrWhiteSpace(ClientPath))
				{
					ClientPath = null;
				}

				if((Type == UserSelectedProjectType.Client && ClientPath != null) || (Type == UserSelectedProjectType.Local && LocalPath != null))
				{
					Project = new UserSelectedProjectSettings(ServerAndPort, UserName, Type, ClientPath, LocalPath);
					return true;
				}
			}

			Project = null;
			return false;
		}

		public string ToConfigEntry()
		{
			ConfigObject Object = new ConfigObject();

			if(ServerAndPort != null)
			{
				Object.SetValue("ServerAndPort", ServerAndPort);
			}
			if(UserName != null)
			{
				Object.SetValue("UserName", UserName);
			}

			Object.SetValue("Type", Type.ToString());

			if(ClientPath != null)
			{
				Object.SetValue("ClientPath", ClientPath);
			}
			if(LocalPath != null)
			{
				Object.SetValue("LocalPath", LocalPath);
			}

			return Object.ToString();
		}

		public override string ToString()
		{
			return LocalPath ?? ClientPath;
		}
	}

	class UserWorkspaceSettings
	{
		// Settings for the currently synced project in this workspace. CurrentChangeNumber is only valid for this workspace if CurrentProjectPath is the current project.
		public string CurrentProjectIdentifier;
		public int CurrentChangeNumber;
		public string CurrentSyncFilterHash;
		public List<int> AdditionalChangeNumbers = new List<int>();

		// Settings for the last attempted sync. These values are set to persist error messages between runs.
		public int LastSyncChangeNumber;
		public WorkspaceUpdateResult LastSyncResult;
		public string LastSyncResultMessage;
		public DateTime? LastSyncTime;
		public int LastSyncDurationSeconds;

		// The last successful build, regardless of whether a failed sync has happened in the meantime. Used to determine whether to force a clean due to entries in the project config file.
		public int LastBuiltChangeNumber;

		// Expanded archives in the workspace
		public string[] ExpandedArchiveTypes;

		// The changes that we're regressing at the moment
		public Dictionary<int, BisectState> ChangeNumberToBisectState = new Dictionary<int, BisectState>();

		// Workspace specific SyncFilters
		public string[] SyncView;
		public Guid[] SyncIncludedCategories;
		public Guid[] SyncExcludedCategories;
		public bool? bSyncAllProjects;
	}

	class UserProjectSettings
	{
		public List<ConfigObject> BuildSteps = new List<ConfigObject>();
	}

	class UserSettings
	{
		string FileName;
		ConfigFile ConfigFile = new ConfigFile();

		// General settings
		public bool bBuildAfterSync;
		public bool bRunAfterSync;
		public bool bSyncPrecompiledEditor;
		public bool bOpenSolutionAfterSync;
		public bool bShowLogWindow;
		public bool bAutoResolveConflicts;
		public bool bUseIncrementalBuilds;
		public bool bShowUnreviewedChanges;
		public bool bShowAutomatedChanges;
		public bool bShowLocalTimes;
		public bool bKeepInTray;
		public int FilterIndex;
		public UserSelectedProjectSettings LastProject;
		public List<UserSelectedProjectSettings> OpenProjects;
		public List<UserSelectedProjectSettings> RecentProjects;
		public string[] SyncView;
		public Guid[] SyncExcludedCategories;
		public bool bSyncAllProjects;
		public LatestChangeType SyncType;
		public BuildConfig CompiledEditorBuildConfig; // NB: This assumes not using precompiled editor. See CurrentBuildConfig.
		public TabLabels TabLabels;

		// Window settings
		public bool bWindowVisible;
		
		// Schedule settings
		public bool bScheduleEnabled;
		public TimeSpan ScheduleTime;
		public LatestChangeType ScheduleChange;
		public bool ScheduleAnyOpenProject;
		public List<UserSelectedProjectSettings> ScheduleProjects;

		// Run configuration
		public List<Tuple<string, bool>> EditorArguments = new List<Tuple<string,bool>>();
		public bool bEditorArgumentsPrompt;

		// Project settings
		Dictionary<string, UserWorkspaceSettings> WorkspaceKeyToSettings = new Dictionary<string,UserWorkspaceSettings>();
		Dictionary<string, UserProjectSettings> ProjectKeyToSettings = new Dictionary<string,UserProjectSettings>();

		// Perforce settings
		public PerforceSyncOptions SyncOptions = new PerforceSyncOptions();

		private List<UserSelectedProjectSettings> ReadProjectList(string SettingName, string LegacySettingName)
		{
			List<UserSelectedProjectSettings> Projects = new List<UserSelectedProjectSettings>();

			string[] ProjectStrings = ConfigFile.GetValues(SettingName, null);
			if(ProjectStrings != null)
			{
				foreach(string ProjectString in ProjectStrings)
				{
					UserSelectedProjectSettings Project;
					if(UserSelectedProjectSettings.TryParseConfigEntry(ProjectString, out Project))
					{
						Projects.Add(Project);
					}
				}
			}
			else if(LegacySettingName != null)
			{
				string[] LegacyProjectStrings = ConfigFile.GetValues(LegacySettingName, null);
				if(LegacyProjectStrings != null)
				{
					foreach(string LegacyProjectString in LegacyProjectStrings)
					{
						if(!String.IsNullOrWhiteSpace(LegacyProjectString))
						{
							Projects.Add(new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, LegacyProjectString));
						}
					}
				}
			}

			return Projects;
		}

		public UserSettings(string InFileName)
		{
			FileName = InFileName;
			if(File.Exists(FileName))
			{
				ConfigFile.Load(FileName);
			}

			// General settings
			bBuildAfterSync = (ConfigFile.GetValue("General.BuildAfterSync", "1") != "0");
			bRunAfterSync = (ConfigFile.GetValue("General.RunAfterSync", "1") != "0");
			bSyncPrecompiledEditor = (ConfigFile.GetValue("General.SyncPrecompiledEditor", "0") != "0");
			bOpenSolutionAfterSync = (ConfigFile.GetValue("General.OpenSolutionAfterSync", "0") != "0");
			bShowLogWindow = (ConfigFile.GetValue("General.ShowLogWindow", false));
			bAutoResolveConflicts = (ConfigFile.GetValue("General.AutoResolveConflicts", "1") != "0");
			bUseIncrementalBuilds = ConfigFile.GetValue("General.IncrementalBuilds", true);
			bShowUnreviewedChanges = ConfigFile.GetValue("General.ShowUnreviewed", true);
			bShowAutomatedChanges = ConfigFile.GetValue("General.ShowAutomated", false);
			bShowLocalTimes = ConfigFile.GetValue("General.ShowLocalTimes", false);
			bKeepInTray = ConfigFile.GetValue("General.KeepInTray", true);
			int.TryParse(ConfigFile.GetValue("General.FilterIndex", "0"), out FilterIndex);

			string LastProjectString = ConfigFile.GetValue("General.LastProject", null);
			if(LastProjectString != null)
			{
				UserSelectedProjectSettings.TryParseConfigEntry(LastProjectString, out LastProject);
			}
			else
			{
				string LastProjectFileName = ConfigFile.GetValue("General.LastProjectFileName", null);
				if(LastProjectFileName != null)
				{
					LastProject = new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, LastProjectFileName);
				}
			}

			OpenProjects = ReadProjectList("General.OpenProjects", "General.OpenProjectFileNames");
			RecentProjects = ReadProjectList("General.RecentProjects", "General.OtherProjectFileNames");

			SyncView = ConfigFile.GetValues("General.SyncFilter", new string[0]);
			SyncExcludedCategories = ConfigFile.GetGuidValues("General.SyncExcludedCategories", new Guid[0]);
			bSyncAllProjects = ConfigFile.GetValue("General.SyncAllProjects", false);
			if(!Enum.TryParse(ConfigFile.GetValue("General.SyncType", ""), out SyncType))
			{
				SyncType = LatestChangeType.Good;
			}

			// Build configuration
			string CompiledEditorBuildConfigName = ConfigFile.GetValue("General.BuildConfig", "");
			if(!Enum.TryParse(CompiledEditorBuildConfigName, true, out CompiledEditorBuildConfig))
			{
				CompiledEditorBuildConfig = BuildConfig.DebugGame;
			}

			// Tab names
			string TabLabelsValue = ConfigFile.GetValue("General.TabLabels", "");
			if(!Enum.TryParse(TabLabelsValue, true, out TabLabels))
			{
				TabLabels = TabLabels.Stream;
			}

			// Editor arguments
			string[] Arguments = ConfigFile.GetValues("General.EditorArguments", new string[]{ "0:-log", "0:-fastload" });
			foreach(string Argument in Arguments)
			{
				if(Argument.StartsWith("0:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument.Substring(2), false));
				}
				else if(Argument.StartsWith("1:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument.Substring(2), true));
				}
				else
				{
					EditorArguments.Add(new Tuple<string,bool>(Argument, true));
				}
			}
			bEditorArgumentsPrompt = ConfigFile.GetValue("General.EditorArgumentsPrompt", false);

			// Window settings
			bWindowVisible = ConfigFile.GetValue("Window.Visible", true);

			// Schedule settings
			bScheduleEnabled = ConfigFile.GetValue("Schedule.Enabled", false);
			if(!TimeSpan.TryParse(ConfigFile.GetValue("Schedule.Time", ""), out ScheduleTime))
			{
				ScheduleTime = new TimeSpan(6, 0, 0);
			}
			if(!Enum.TryParse(ConfigFile.GetValue("Schedule.Change", ""), out ScheduleChange))
			{
				ScheduleChange = LatestChangeType.Good;
			}
			ScheduleAnyOpenProject = ConfigFile.GetValue("Schedule.AnyOpenProject", true);
			ScheduleProjects = ReadProjectList("Schedule.Projects", "Schedule.ProjectFileNames");

			// Perforce settings
			if(!int.TryParse(ConfigFile.GetValue("Perforce.NumRetries", "0"), out SyncOptions.NumRetries))
			{
				SyncOptions.NumRetries = 0;
			}
			if(!int.TryParse(ConfigFile.GetValue("Perforce.NumThreads", "0"), out SyncOptions.NumThreads))
			{
				SyncOptions.NumThreads = 0;
			}
			if(!int.TryParse(ConfigFile.GetValue("Perforce.TcpBufferSize", "0"), out SyncOptions.TcpBufferSize))
			{
				SyncOptions.TcpBufferSize = 0;
			}
		}

		public UserWorkspaceSettings FindOrAddWorkspace(string ClientBranchPath)
		{
			// Update the current workspace
			string CurrentWorkspaceKey = ClientBranchPath.Trim('/');

			UserWorkspaceSettings CurrentWorkspace;
			if(!WorkspaceKeyToSettings.TryGetValue(CurrentWorkspaceKey, out CurrentWorkspace))
			{
				// Create a new workspace settings object
				CurrentWorkspace = new UserWorkspaceSettings();
				WorkspaceKeyToSettings.Add(CurrentWorkspaceKey, CurrentWorkspace);

				// Read the workspace settings
				ConfigSection WorkspaceSection = ConfigFile.FindSection(CurrentWorkspaceKey);
				if(WorkspaceSection == null)
				{
					string LegacyBranchAndClientKey = ClientBranchPath.Trim('/');

					int SlashIdx = LegacyBranchAndClientKey.IndexOf('/');
					if(SlashIdx != -1)
					{
						LegacyBranchAndClientKey = LegacyBranchAndClientKey.Substring(0, SlashIdx) + "$" + LegacyBranchAndClientKey.Substring(SlashIdx + 1);
					}

					string CurrentSync = ConfigFile.GetValue("Clients." + LegacyBranchAndClientKey, null);
					if(CurrentSync != null)
					{
						int AtIdx = CurrentSync.LastIndexOf('@');
						if(AtIdx != -1)
						{
							int ChangeNumber;
							if(int.TryParse(CurrentSync.Substring(AtIdx + 1), out ChangeNumber))
							{
								CurrentWorkspace.CurrentProjectIdentifier = CurrentSync.Substring(0, AtIdx);
								CurrentWorkspace.CurrentChangeNumber = ChangeNumber;
							}
						}
					}

					string LastUpdateResultText = ConfigFile.GetValue("Clients." + LegacyBranchAndClientKey + "$LastUpdate", null);
					if(LastUpdateResultText != null)
					{
						int ColonIdx = LastUpdateResultText.LastIndexOf(':');
						if(ColonIdx != -1)
						{
							int ChangeNumber;
							if(int.TryParse(LastUpdateResultText.Substring(0, ColonIdx), out ChangeNumber))
							{
								WorkspaceUpdateResult Result;
								if(Enum.TryParse(LastUpdateResultText.Substring(ColonIdx + 1), out Result))
								{
									CurrentWorkspace.LastSyncChangeNumber = ChangeNumber;
									CurrentWorkspace.LastSyncResult = Result;
								}
							}
						}
					}

					CurrentWorkspace.SyncView = new string[0];
					CurrentWorkspace.SyncIncludedCategories = new Guid[0];
					CurrentWorkspace.SyncExcludedCategories = new Guid[0];
					CurrentWorkspace.bSyncAllProjects = null;
				}
				else
				{
					CurrentWorkspace.CurrentProjectIdentifier = WorkspaceSection.GetValue("CurrentProjectPath");
					CurrentWorkspace.CurrentChangeNumber = WorkspaceSection.GetValue("CurrentChangeNumber", -1);
					CurrentWorkspace.CurrentSyncFilterHash = WorkspaceSection.GetValue("CurrentSyncFilterHash", null);
					foreach(string AdditionalChangeNumberString in WorkspaceSection.GetValues("AdditionalChangeNumbers", new string[0]))
					{
						int AdditionalChangeNumber;
						if(int.TryParse(AdditionalChangeNumberString, out AdditionalChangeNumber))
						{
							CurrentWorkspace.AdditionalChangeNumbers.Add(AdditionalChangeNumber);
						}
					}
					Enum.TryParse(WorkspaceSection.GetValue("LastSyncResult", ""), out CurrentWorkspace.LastSyncResult);
					CurrentWorkspace.LastSyncResultMessage = UnescapeText(WorkspaceSection.GetValue("LastSyncResultMessage"));
					CurrentWorkspace.LastSyncChangeNumber = WorkspaceSection.GetValue("LastSyncChangeNumber", -1);

					DateTime LastSyncTime;
					if(DateTime.TryParse(WorkspaceSection.GetValue("LastSyncTime", ""), out LastSyncTime))
					{
						CurrentWorkspace.LastSyncTime = LastSyncTime;
					}

					CurrentWorkspace.LastSyncDurationSeconds = WorkspaceSection.GetValue("LastSyncDuration", 0);
					CurrentWorkspace.LastBuiltChangeNumber = WorkspaceSection.GetValue("LastBuiltChangeNumber", 0);
					CurrentWorkspace.ExpandedArchiveTypes = WorkspaceSection.GetValues("ExpandedArchiveName", new string[0]);

					CurrentWorkspace.SyncView = WorkspaceSection.GetValues("SyncFilter", new string[0]);
					CurrentWorkspace.SyncIncludedCategories = WorkspaceSection.GetValues("SyncIncludedCategories", new Guid[0]);
					CurrentWorkspace.SyncExcludedCategories = WorkspaceSection.GetValues("SyncExcludedCategories", new Guid[0]);

					int SyncAllProjects = WorkspaceSection.GetValue("SyncAllProjects", -1);
					CurrentWorkspace.bSyncAllProjects = (SyncAllProjects == 0)? (bool?)false : (SyncAllProjects == 1)? (bool?)true : (bool?)null;

					string[] BisectEntries = WorkspaceSection.GetValues("Bisect", new string[0]);
					foreach(string BisectEntry in BisectEntries)
					{
						ConfigObject BisectEntryObject = new ConfigObject(BisectEntry);

						int ChangeNumber = BisectEntryObject.GetValue("Change", -1);
						if(ChangeNumber != -1)
						{
							BisectState State;
							if(Enum.TryParse(BisectEntryObject.GetValue("State", ""), out State))
							{
								CurrentWorkspace.ChangeNumberToBisectState[ChangeNumber] = State;
							}
						}
					}
				}
			}
			return CurrentWorkspace;
		}

		public UserProjectSettings FindOrAddProject(string ClientProjectFileName)
		{
			// Read the project settings
			UserProjectSettings CurrentProject;
			if(!ProjectKeyToSettings.TryGetValue(ClientProjectFileName, out CurrentProject))
			{
				CurrentProject = new UserProjectSettings();
				ProjectKeyToSettings.Add(ClientProjectFileName, CurrentProject);
	
				ConfigSection ProjectSection = ConfigFile.FindOrAddSection(ClientProjectFileName);
				CurrentProject.BuildSteps.AddRange(ProjectSection.GetValues("BuildStep", new string[0]).Select(x => new ConfigObject(x)));
			}
			return CurrentProject;
		}

		public void Save()
		{
			// General settings
			ConfigSection GeneralSection = ConfigFile.FindOrAddSection("General");
			GeneralSection.Clear();
			GeneralSection.SetValue("BuildAfterSync", bBuildAfterSync);
			GeneralSection.SetValue("RunAfterSync", bRunAfterSync);
			GeneralSection.SetValue("SyncPrecompiledEditor", bSyncPrecompiledEditor);
			GeneralSection.SetValue("OpenSolutionAfterSync", bOpenSolutionAfterSync);
			GeneralSection.SetValue("ShowLogWindow", bShowLogWindow);
			GeneralSection.SetValue("AutoResolveConflicts", bAutoResolveConflicts);
			GeneralSection.SetValue("IncrementalBuilds", bUseIncrementalBuilds);
			GeneralSection.SetValue("ShowUnreviewed", bShowUnreviewedChanges);
			GeneralSection.SetValue("ShowAutomated", bShowAutomatedChanges);
			GeneralSection.SetValue("ShowLocalTimes", bShowLocalTimes);
			if(LastProject != null)
			{
				GeneralSection.SetValue("LastProject", LastProject.ToConfigEntry());
			}
			GeneralSection.SetValues("OpenProjects", OpenProjects.Select(x => x.ToConfigEntry()).ToArray());
			GeneralSection.SetValue("KeepInTray", bKeepInTray);
			GeneralSection.SetValue("FilterIndex", FilterIndex);
			GeneralSection.SetValues("RecentProjects", RecentProjects.Select(x => x.ToConfigEntry()).ToArray());
			GeneralSection.SetValues("SyncFilter", SyncView);
			GeneralSection.SetValues("SyncExcludedCategories", SyncExcludedCategories);
			GeneralSection.SetValue("SyncAllProjects", bSyncAllProjects);
			GeneralSection.SetValue("SyncType", SyncType.ToString());

			// Build configuration
			GeneralSection.SetValue("BuildConfig", CompiledEditorBuildConfig.ToString());

			// Tab labels
			GeneralSection.SetValue("TabLabels", TabLabels.ToString());

			// Editor arguments
			List<string> EditorArgumentList = new List<string>();
			foreach(Tuple<string, bool> EditorArgument in EditorArguments)
			{
				EditorArgumentList.Add(String.Format("{0}:{1}", EditorArgument.Item2? 1 : 0, EditorArgument.Item1));
			}
			GeneralSection.SetValues("EditorArguments", EditorArgumentList.ToArray());
			GeneralSection.SetValue("EditorArgumentsPrompt", bEditorArgumentsPrompt);

			// Schedule settings
			ConfigSection ScheduleSection = ConfigFile.FindOrAddSection("Schedule");
			ScheduleSection.Clear();
			ScheduleSection.SetValue("Enabled", bScheduleEnabled);
			ScheduleSection.SetValue("Time", ScheduleTime.ToString());
			ScheduleSection.SetValue("Change", ScheduleChange.ToString());
			ScheduleSection.SetValue("AnyOpenProject", ScheduleAnyOpenProject);
			ScheduleSection.SetValues("Projects", ScheduleProjects.Select(x => x.ToConfigEntry()).ToArray());

			// Window settings
			ConfigSection WindowSection = ConfigFile.FindOrAddSection("Window");
			WindowSection.Clear();
			WindowSection.SetValue("Visible", bWindowVisible);

			// Current workspace settings
			foreach(KeyValuePair<string, UserWorkspaceSettings> Pair in WorkspaceKeyToSettings)
			{
				string CurrentWorkspaceKey = Pair.Key;
				UserWorkspaceSettings CurrentWorkspace = Pair.Value;

				ConfigSection WorkspaceSection = ConfigFile.FindOrAddSection(CurrentWorkspaceKey);
				WorkspaceSection.Clear();
				WorkspaceSection.SetValue("CurrentProjectPath", CurrentWorkspace.CurrentProjectIdentifier);
				WorkspaceSection.SetValue("CurrentChangeNumber", CurrentWorkspace.CurrentChangeNumber);
				if(CurrentWorkspace.CurrentSyncFilterHash != null)
				{
					WorkspaceSection.SetValue("CurrentSyncFilterHash", CurrentWorkspace.CurrentSyncFilterHash);
				}
				WorkspaceSection.SetValues("AdditionalChangeNumbers", CurrentWorkspace.AdditionalChangeNumbers.Select(x => x.ToString()).ToArray());
				WorkspaceSection.SetValue("LastSyncResult", CurrentWorkspace.LastSyncResult.ToString());
				WorkspaceSection.SetValue("LastSyncResultMessage", EscapeText(CurrentWorkspace.LastSyncResultMessage));
				WorkspaceSection.SetValue("LastSyncChangeNumber", CurrentWorkspace.LastSyncChangeNumber);
				if(CurrentWorkspace.LastSyncTime.HasValue)
				{
					WorkspaceSection.SetValue("LastSyncTime", CurrentWorkspace.LastSyncTime.ToString());
				}
				if(CurrentWorkspace.LastSyncDurationSeconds > 0)
				{
					WorkspaceSection.SetValue("LastSyncDuration", CurrentWorkspace.LastSyncDurationSeconds);
				}
				WorkspaceSection.SetValue("LastBuiltChangeNumber", CurrentWorkspace.LastBuiltChangeNumber);
				WorkspaceSection.SetValues("ExpandedArchiveName", CurrentWorkspace.ExpandedArchiveTypes);
				WorkspaceSection.SetValues("SyncFilter", CurrentWorkspace.SyncView);
				WorkspaceSection.SetValues("SyncIncludedCategories", CurrentWorkspace.SyncIncludedCategories);
				WorkspaceSection.SetValues("SyncExcludedCategories", CurrentWorkspace.SyncExcludedCategories);
				if(CurrentWorkspace.bSyncAllProjects.HasValue)
				{
					WorkspaceSection.SetValue("SyncAllProjects", CurrentWorkspace.bSyncAllProjects.Value);
				}

				List<ConfigObject> BisectEntryObjects = new List<ConfigObject>();
				foreach(KeyValuePair<int, BisectState> BisectPair in CurrentWorkspace.ChangeNumberToBisectState)
				{
					ConfigObject BisectEntryObject = new ConfigObject();
					BisectEntryObject.SetValue("Change", BisectPair.Key);
					BisectEntryObject.SetValue("State", BisectPair.Value.ToString());
					BisectEntryObjects.Add(BisectEntryObject);
				}
				WorkspaceSection.SetValues("Bisect", BisectEntryObjects.Select(x => x.ToString()).ToArray());
			}

			// Current project settings
			foreach(KeyValuePair<string, UserProjectSettings> Pair in ProjectKeyToSettings)
			{
				string CurrentProjectKey = Pair.Key;
				UserProjectSettings CurrentProject = Pair.Value;

				ConfigSection ProjectSection = ConfigFile.FindOrAddSection(CurrentProjectKey);
				ProjectSection.Clear();
				ProjectSection.SetValues("BuildStep", CurrentProject.BuildSteps.Select(x => x.ToString()).ToArray());
			}

			// Perforce settings
			ConfigSection PerforceSection = ConfigFile.FindOrAddSection("Perforce");
			PerforceSection.Clear();
			if(SyncOptions.NumRetries > 0)
			{
				PerforceSection.SetValue("NumRetries", SyncOptions.NumRetries);
			}
			if(SyncOptions.NumThreads > 0)
			{
				PerforceSection.SetValue("NumThreads", SyncOptions.NumThreads);
			}
			if(SyncOptions.TcpBufferSize > 0)
			{
				PerforceSection.SetValue("TcpBufferSize", SyncOptions.TcpBufferSize);
			}

			// Save the file
			ConfigFile.Save(FileName);
		}

		public static Guid[] GetEffectiveExcludedCategories(Guid[] GlobalExcludedCategories, Guid[] WorkspaceIncludedCategories, Guid[] WorkspaceExcludedCategories)
		{
			return GlobalExcludedCategories.Except(WorkspaceIncludedCategories).Union(WorkspaceExcludedCategories).ToArray();
		}

		public static string[] GetCombinedSyncFilter(Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToFilter, string[] GlobalView, Guid[] GlobalExcludedCategories, string[] WorkspaceView, Guid[] WorkspaceIncludedCategories, Guid[] WorkspaceExcludedCategories)
		{
			List<string> Lines = new List<string>();
			foreach(string ViewLine in Enumerable.Concat(GlobalView, WorkspaceView).Select(x => x.Trim()).Where(x => x.Length > 0 && !x.StartsWith(";")))
			{
				Lines.Add(ViewLine);
			}

			HashSet<Guid> ExcludedCategories = new HashSet<Guid>(GetEffectiveExcludedCategories(GlobalExcludedCategories, WorkspaceIncludedCategories, WorkspaceExcludedCategories));
			foreach(WorkspaceSyncCategory Filter in UniqueIdToFilter.Values.Where(x => x.bEnable && ExcludedCategories.Contains(x.UniqueId)).OrderBy(x => x.Name))
			{
				Lines.AddRange(Filter.Paths.Select(x => "-" + x.Trim()));
			}

			return Lines.ToArray();
		}

		static string EscapeText(string Text)
		{
			if(Text == null)
			{
				return null;
			}

			StringBuilder Result = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				switch(Text[Idx])
				{
					case '\\':
						Result.Append("\\\\");
						break;
					case '\t':
						Result.Append("\\t");
						break;
					case '\r':
						Result.Append("\\r");
						break;
					case '\n':
						Result.Append("\\n");
						break;
					case '\'':
						Result.Append("\\\'");
						break;
					case '\"':
						Result.Append("\\\"");
						break;
					default:
						Result.Append(Text[Idx]);
						break;
				}
			}
			return Result.ToString();
		}

		static string UnescapeText(string Text)
		{
			if(Text == null)
			{
				return null;
			}

			StringBuilder Result = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == '\\' && Idx + 1 < Text.Length)
				{
					switch(Text[++Idx])
					{
						case 't':
							Result.Append('\t');
							break;
						case 'r':
							Result.Append('\r');
							break;
						case 'n':
							Result.Append('\n');
							break;
						case '\'':
							Result.Append('\'');
							break;
						case '\"':
							Result.Append('\"');
							break;
						default:
							Result.Append(Text[Idx]);
							break;
					}
				}
				else
				{
					Result.Append(Text[Idx]);
				}
			}
			return Result.ToString();
		}
	}
}
