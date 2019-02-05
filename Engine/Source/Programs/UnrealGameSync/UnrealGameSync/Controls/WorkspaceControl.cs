// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Data.SqlClient;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using Microsoft.Win32;
using System.Threading;
using System.Drawing.Imaging;
using System.Web.Script.Serialization;

namespace UnrealGameSync
{
	public enum LatestChangeType
	{
		Any,
		Good,
		Starred,
	}

	interface IWorkspaceControlOwner
	{
		void EditSelectedProject(WorkspaceControl Workspace);
		void RequestProjectChange(WorkspaceControl Workspace, UserSelectedProjectSettings Project, bool bModal);
		void ShowAndActivate();
		void StreamChanged(WorkspaceControl Workspace);
		void SetTabNames(TabLabels TabNames);
		void SetupScheduledSync();
		void UpdateProgress();
		void ModifyApplicationSettings();
	}

	delegate void WorkspaceStartupCallback(WorkspaceControl Workspace, bool bCancel);
	delegate void WorkspaceUpdateCallback(WorkspaceUpdateResult Result);

	partial class WorkspaceControl : UserControl, IMainWindowTabPanel
	{
		enum HorizontalAlignment
		{
			Left,
			Center,
			Right
		}

		enum VerticalAlignment
		{
			Top,
			Middle,
			Bottom
		}

		class BadgeInfo
		{
			public string Label;
			public string Group;
			public string UniqueId;
			public int Offset;
			public int Width;
			public int Height;
			public Color BackgroundColor;
			public Color HoverBackgroundColor;
			public Action ClickHandler;
			public string ToolTip;

			public BadgeInfo(string Label, string Group, Color BadgeColor)
				: this(Label, Group, null, BadgeColor, BadgeColor, null)
			{
			}

			public BadgeInfo(string Label, string Group, string UniqueId, Color BackgroundColor, Color HoverBackgroundColor, Action ClickHandler)
			{
				this.Label = Label;
				this.Group = Group;
				this.UniqueId = UniqueId;
				this.BackgroundColor = BackgroundColor;
				this.HoverBackgroundColor = HoverBackgroundColor;
				this.ClickHandler = ClickHandler;
			}

			public BadgeInfo(BadgeInfo Other)
				: this(Other.Label, Other.Group, Other.UniqueId, Other.BackgroundColor, Other.HoverBackgroundColor, Other.ClickHandler)
			{
				this.Offset = Other.Offset;
				this.Width = Other.Width;
				this.Height = Other.Height;
			}

			public Rectangle GetBounds(Point ListLocation)
			{
				return new Rectangle(ListLocation.X + Offset, ListLocation.Y, Width, Height);
			}
		}

		class ChangeLayoutInfo
		{
			public List<BadgeInfo> DescriptionBadges;
			public List<BadgeInfo> TypeBadges;
			public List<BadgeInfo> BuildBadges;
			public Dictionary<string, List<BadgeInfo>> CustomBadges;
		}

		static Rectangle GoodBuildIcon = new Rectangle(0, 0, 16, 16);
		static Rectangle MixedBuildIcon = new Rectangle(16, 0, 16, 16);
		static Rectangle BadBuildIcon = new Rectangle(32, 0, 16, 16);
		static Rectangle DefaultBuildIcon = new Rectangle(48, 0, 16, 16);
		static Rectangle PromotedBuildIcon = new Rectangle(64, 0, 16, 16);
		static Rectangle DetailsIcon = new Rectangle(80, 0, 16, 16);
		static Rectangle InfoIcon = new Rectangle(96, 0, 16, 16);
		static Rectangle CancelIcon = new Rectangle(112, 0, 16, 16);
		static Rectangle SyncIcon = new Rectangle(128, 0, 32, 16);
		static Rectangle HappyIcon = new Rectangle(160, 0, 16, 16);
		static Rectangle DisabledHappyIcon = new Rectangle(176, 0, 16, 16);
		static Rectangle FrownIcon = new Rectangle(192, 0, 16, 16);
		static Rectangle DisabledFrownIcon = new Rectangle(208, 0, 16, 16);
		static Rectangle PreviousSyncIcon = new Rectangle(224, 0, 16, 16);
		static Rectangle AdditionalSyncIcon = new Rectangle(240, 0, 16, 16);
		static Rectangle BisectPassIcon = new Rectangle(256, 0, 16, 16);
		static Rectangle BisectFailIcon = new Rectangle(273, 0, 16, 16);
		static Rectangle BisectImplicitPassIcon = new Rectangle(290, 0, 16, 16);
		static Rectangle BisectImplicitFailIcon = new Rectangle(307, 0, 16, 16);

		[DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
		static extern int SetWindowTheme(IntPtr hWnd, string pszSubAppName, string pszSubIdList);

		const int BuildListExpandCount = 250;

		const string EditorArchiveType = "Editor";

		IWorkspaceControlOwner Owner;
		string ApiUrl;
		string DataFolder;
		LineBasedTextWriter Log;

		UserSettings Settings;
		UserWorkspaceSettings WorkspaceSettings;
		UserProjectSettings ProjectSettings;

		public UserSelectedProjectSettings SelectedProject
		{
			get;
			private set;
		}

		public string SelectedFileName
		{
			get;
			private set;
		}

		string SelectedProjectIdentifier;

		public string BranchDirectoryName
		{
			get;
			private set;
		}

		public string StreamName
		{
			get;
			private set;
		}

		public string ClientName
		{
			get;
			private set;
		}

		SynchronizationContext MainThreadSynchronizationContext;
		bool bIsDisposing;

		bool bUnstable;
		string EditorTargetName;
		bool bIsEnterpriseProject;
		PerforceMonitor PerforceMonitor;
		Workspace Workspace;
		EventMonitor EventMonitor;
		System.Windows.Forms.Timer UpdateTimer;
		HashSet<int> PromotedChangeNumbers = new HashSet<int>();
		List<int> ListIndexToChangeIndex = new List<int>();
		List<int> SortedChangeNumbers = new List<int>();
		Dictionary<int, string> ChangeNumberToArchivePath = new Dictionary<int,string>();
		Dictionary<int, ChangeLayoutInfo> ChangeNumberToLayoutInfo = new Dictionary<int, ChangeLayoutInfo>();
		List<ToolStripMenuItem> CustomToolMenuItems = new List<ToolStripMenuItem>();
		int NumChanges;
		int PendingSelectedChangeNumber = -1;
		bool bHasBuildSteps = false;

		Dictionary<string, int> NotifiedBuildTypeToChangeNumber = new Dictionary<string,int>();

		bool bMouseOverExpandLink;

		string HoverBadgeUniqueId = null;
		bool bHoverSync;
		PerforceChangeSummary ContextMenuChange;
		Font BuildFont;
		Font SelectedBuildFont;
		Font BadgeFont;
		List<KeyValuePair<string, string>> BadgeNameAndGroupPairs = new List<KeyValuePair<string, string>>();
		Dictionary<string, Size> BadgeLabelToSize = new Dictionary<string, Size>();
		List<KeyValuePair<string, BadgeData>> ServiceBadges = new List<KeyValuePair<string, BadgeData>>();

		string OriginalExecutableFileName;

		NotificationWindow NotificationWindow;

		public Tuple<TaskbarState, float> DesiredTaskbarState
		{
			get;
			private set;
		}

		int BuildListWidth;
		float[] ColumnWidths;
		float[] ColumnWeights;
		int[] MinColumnWidths;
		int[] DesiredColumnWidths;
		string LastColumnSettings;
		List<ColumnHeader> CustomColumns;
		int MaxBuildBadgeChars;
		ListViewItem ExpandItem;

		bool bUpdateBuildListPosted;
		bool bUpdateBuildMetadataPosted;
		bool bUpdateReviewsPosted;

		WorkspaceUpdateCallback UpdateCallback;

		System.Threading.Timer StartupTimer;
		List<WorkspaceStartupCallback> StartupCallbacks;

		public WorkspaceControl(IWorkspaceControlOwner InOwner, string InApiUrl, string InOriginalExecutableFileName, bool bInUnstable, DetectProjectSettingsTask DetectSettings, LineBasedTextWriter InLog, UserSettings InSettings)
		{
			InitializeComponent();

			MainThreadSynchronizationContext = SynchronizationContext.Current;

			Owner = InOwner;
			ApiUrl = InApiUrl;
			DataFolder = DetectSettings.DataFolder;
			OriginalExecutableFileName = InOriginalExecutableFileName;
			bUnstable = bInUnstable;
			Log = InLog;
			Settings = InSettings;
			WorkspaceSettings = InSettings.FindOrAddWorkspace(DetectSettings.BranchClientPath);
			ProjectSettings = InSettings.FindOrAddProject(DetectSettings.NewSelectedClientFileName);

			DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
			
			System.Reflection.PropertyInfo DoubleBufferedProperty = typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
			DoubleBufferedProperty.SetValue(BuildList, true, null); 

			// force the height of the rows
			BuildList.SmallImageList = new ImageList(){ ImageSize = new Size(1, 20) };
			BuildList_FontChanged(null, null);
			BuildList.OnScroll += BuildList_OnScroll;

			Splitter.OnVisibilityChanged += Splitter_OnVisibilityChanged;

			UpdateTimer = new System.Windows.Forms.Timer();
			UpdateTimer.Interval = 30;
			UpdateTimer.Tick += TimerCallback;

			UpdateCheckedBuildConfig();

			UpdateSyncActionCheckboxes();

			// Set the project logo on the status panel and notification window
			NotificationWindow = new NotificationWindow(Properties.Resources.DefaultNotificationLogo);
			StatusPanel.SetProjectLogo(DetectSettings.ProjectLogo, true);
			DetectSettings.ProjectLogo = null;

			// Commit all the new project info
			PerforceConnection PerforceClient = DetectSettings.PerforceClient;
			ClientName = PerforceClient.ClientName;
			SelectedProject = DetectSettings.SelectedProject;
			SelectedProjectIdentifier = DetectSettings.NewSelectedProjectIdentifier;
			SelectedFileName = DetectSettings.NewSelectedFileName;
			EditorTargetName = DetectSettings.NewProjectEditorTarget;
			bIsEnterpriseProject = DetectSettings.bIsEnterpriseProject;
			StreamName = DetectSettings.StreamName;

			// Update the branch directory
			BranchDirectoryName = DetectSettings.BranchDirectoryName;

			// Check if we've the project we've got open in this workspace is the one we're actually synced to
			int CurrentChangeNumber = -1;
			string CurrentSyncFilterHash = null;
			if(String.Compare(WorkspaceSettings.CurrentProjectIdentifier, SelectedProjectIdentifier, true) == 0)
			{
				CurrentChangeNumber = WorkspaceSettings.CurrentChangeNumber;
				CurrentSyncFilterHash = WorkspaceSettings.CurrentSyncFilterHash;
			}

			string TelemetryProjectIdentifier = PerforceUtils.GetClientOrDepotDirectoryName(DetectSettings.NewSelectedProjectIdentifier);

			Workspace = new Workspace(PerforceClient, BranchDirectoryName, SelectedFileName, DetectSettings.BranchClientPath, DetectSettings.NewSelectedClientFileName, CurrentChangeNumber, CurrentSyncFilterHash, WorkspaceSettings.LastBuiltChangeNumber, TelemetryProjectIdentifier, DetectSettings.bIsEnterpriseProject, new LogControlTextWriter(SyncLog));
			Workspace.OnUpdateComplete += UpdateCompleteCallback;

			string ProjectLogBaseName = Path.Combine(DataFolder, String.Format("{0}@{1}", PerforceClient.ClientName, DetectSettings.BranchClientPath.Replace("//" + PerforceClient.ClientName + "/", "").Trim('/').Replace("/", "$")));

			PerforceMonitor = new PerforceMonitor(PerforceClient, DetectSettings.BranchClientPath, DetectSettings.NewSelectedClientFileName, DetectSettings.NewSelectedProjectIdentifier, ProjectLogBaseName + ".p4.log", DetectSettings.bIsEnterpriseProject, DetectSettings.LatestProjectConfigFile, DetectSettings.CacheFolder, DetectSettings.LocalConfigFiles);
			PerforceMonitor.OnUpdate += UpdateBuildListCallback;
			PerforceMonitor.OnUpdateMetadata += UpdateBuildMetadataCallback;
			PerforceMonitor.OnStreamChange += StreamChangedCallback;
			PerforceMonitor.OnLoginExpired += LoginExpiredCallback;

			EventMonitor = new EventMonitor(ApiUrl, PerforceUtils.GetClientOrDepotDirectoryName(SelectedProjectIdentifier), DetectSettings.PerforceClient.UserName, ProjectLogBaseName + ".review.log");
			EventMonitor.OnUpdatesReady += UpdateReviewsCallback;

			UpdateColumnSettings();

			string LogFileName = Path.Combine(DataFolder, ProjectLogBaseName + ".sync.log");
			SyncLog.OpenFile(LogFileName);

			Splitter.SetLogVisibility(Settings.bShowLogWindow);

			BuildList.Items.Clear();
			UpdateBuildList();
			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
			UpdateStatusPanel();
			UpdateServiceBadges();

			PerforceMonitor.Start();
			EventMonitor.Start();

			StartupTimer = new System.Threading.Timer(x => MainThreadSynchronizationContext.Post((o) => { if(!IsDisposed){ StartupTimerElapsed(false); } }, null), null, TimeSpan.FromSeconds(20.0), TimeSpan.FromMilliseconds(-1.0));
			StartupCallbacks = new List<WorkspaceStartupCallback>();
		}

		private void CheckForStartupComplete()
		{
			if(StartupTimer != null)
			{
				int LatestChangeNumber;
				if(FindChangeToSync(Settings.SyncType, out LatestChangeNumber))
				{
					StartupTimerElapsed(false);
				}
			}
		}

		private void StartupTimerElapsed(bool bCancel)
		{
			if(StartupTimer != null)
			{
				StartupTimer.Dispose();
				StartupTimer = null;
			}

			if(StartupCallbacks != null)
			{
				foreach(WorkspaceStartupCallback StartupCallback in StartupCallbacks)
				{
					StartupCallback(this, bCancel);
				}
				StartupCallbacks = null;
			}
		}

		public void AddStartupCallback(WorkspaceStartupCallback StartupCallback)
		{
			if(StartupTimer == null)
			{
				StartupCallback(this, false);
			}
			else
			{
				StartupCallbacks.Add(StartupCallback);
			}
		}

		private void UpdateColumnSettings()
		{
			string NextColumnSettings;
			TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "Columns", out NextColumnSettings);

			if(CustomColumns == null || NextColumnSettings != LastColumnSettings)
			{
				LastColumnSettings = NextColumnSettings;

				if(CustomColumns != null)
				{
					foreach(ColumnHeader CustomColumn in CustomColumns)
					{
						BuildList.Columns.Remove(CustomColumn);
					}
				}

				Dictionary<string, ColumnHeader> NameToColumn = new Dictionary<string, ColumnHeader>();
				foreach(ColumnHeader Column in BuildList.Columns)
				{
					NameToColumn[Column.Text] = Column;
					Column.Tag = null;
				}

				CustomColumns = new List<ColumnHeader>();
				if(NextColumnSettings != null)
				{
					foreach(string CustomColumn in NextColumnSettings.Split('\n'))
					{
						ConfigObject ColumnConfig = new ConfigObject(CustomColumn);

						string Name = ColumnConfig.GetValue("Name", null);
						if(Name != null)
						{
							ColumnHeader Column;
							if(NameToColumn.TryGetValue(Name, out Column))
							{
								Column.Tag = ColumnConfig;
							}
							else
							{
								int Index = ColumnConfig.GetValue("Index", -1);
								if(Index < 0 || Index > BuildList.Columns.Count)
								{
									Index = ((CustomColumns.Count > 0)? CustomColumns[CustomColumns.Count - 1].Index : CISColumn.Index) + 1;
								}

								Column = new ColumnHeader();
								Column.Text = Name;
								Column.Tag = ColumnConfig;
								BuildList.Columns.Insert(Index, Column);

								CustomColumns.Add(Column);
							}
						}
					}
				}

				ColumnWidths = new float[BuildList.Columns.Count];
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					ColumnWidths[Idx] = BuildList.Columns[Idx].Width;
				}

				using(Graphics Graphics = Graphics.FromHwnd(IntPtr.Zero))
				{
					float DpiScaleX = Graphics.DpiX / 96.0f;

					MinColumnWidths = Enumerable.Repeat(32, BuildList.Columns.Count).ToArray();
					MinColumnWidths[IconColumn.Index] = (int)(50 * DpiScaleX);
					MinColumnWidths[TypeColumn.Index] = (int)(100 * DpiScaleX);
					MinColumnWidths[TimeColumn.Index] = (int)(75 * DpiScaleX);
					MinColumnWidths[ChangeColumn.Index] = (int)(75 * DpiScaleX);
					MinColumnWidths[CISColumn.Index] = (int)(200 * DpiScaleX);

					DesiredColumnWidths = Enumerable.Repeat(65536, BuildList.Columns.Count).ToArray();
					DesiredColumnWidths[IconColumn.Index] = MinColumnWidths[IconColumn.Index];
					DesiredColumnWidths[TypeColumn.Index] = MinColumnWidths[TypeColumn.Index];
					DesiredColumnWidths[TimeColumn.Index] = MinColumnWidths[TimeColumn.Index];
					DesiredColumnWidths[ChangeColumn.Index] = MinColumnWidths[ChangeColumn.Index];
					DesiredColumnWidths[AuthorColumn.Index] = (int)(120 * DpiScaleX);
					DesiredColumnWidths[CISColumn.Index] = (int)(200 * DpiScaleX);
					DesiredColumnWidths[StatusColumn.Index] = (int)(300 * DpiScaleX);
				}

				ColumnWeights = Enumerable.Repeat(1.0f, BuildList.Columns.Count).ToArray();
				ColumnWeights[IconColumn.Index] = 3.0f;
				ColumnWeights[TypeColumn.Index] = 3.0f;
				ColumnWeights[TimeColumn.Index] = 3.0f;
				ColumnWeights[ChangeColumn.Index] = 3.0f;
				ColumnWeights[DescriptionColumn.Index] = 1.25f;
				ColumnWeights[CISColumn.Index] = 1.5f;

				foreach(ColumnHeader Column in BuildList.Columns)
				{
					ConfigObject ColumnConfig = (ConfigObject)Column.Tag;
					if(ColumnConfig != null)
					{
						MinColumnWidths[Column.Index] = ColumnConfig.GetValue("MinWidth", MinColumnWidths[Column.Index]);
						DesiredColumnWidths[Column.Index] = ColumnConfig.GetValue("DesiredWidth", DesiredColumnWidths[Column.Index]);
						ColumnWeights[Column.Index] = ColumnConfig.GetValue("Weight", MinColumnWidths[Column.Index]);
					}
				}

				ConfigFile ProjectConfigFile = PerforceMonitor.LatestProjectConfigFile;
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					if(!String.IsNullOrEmpty(BuildList.Columns[Idx].Text))
					{
						string StringValue;
						if(TryGetProjectSetting(ProjectConfigFile, String.Format("ColumnWidth_{0}", BuildList.Columns[Idx].Text), out StringValue))
						{
							int IntValue;
							if(Int32.TryParse(StringValue, out IntValue))
							{
								DesiredColumnWidths[Idx] = IntValue;
							}
						}
					}
				}

				if(ColumnWidths != null)
				{
					ResizeColumns(ColumnWidths.Sum());
				}
			}
		}

		private void UpdateServiceBadges()
		{
			string[] ServiceBadgeNames;
			if(!TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "ServiceBadges", out ServiceBadgeNames))
			{
				ServiceBadgeNames = new string[0];
			}

			ServiceBadges.Clear();
			foreach(string ServiceBadgeName in ServiceBadgeNames)
			{
				BadgeData LatestBuild;
				EventMonitor.TryGetLatestBadge(ServiceBadgeName, out LatestBuild);
				ServiceBadges.Add(new KeyValuePair<string, BadgeData>(ServiceBadgeName, LatestBuild));
			}
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			BuildListWidth = BuildList.Width;

			// Find the default widths 
			ColumnWidths = new float[BuildList.Columns.Count];
			for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
			{
				if(DesiredColumnWidths[Idx] > 0)
				{
					ColumnWidths[Idx] = DesiredColumnWidths[Idx];
				}
				else
				{
					ColumnWidths[Idx] = BuildList.Columns[Idx].Width;
				}
			}

			// Resize them to fit the size of the window
			ResizeColumns(BuildList.Width - 32);
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			bIsDisposing = true;

			if (disposing && (components != null))
			{
				components.Dispose();
			}

			UpdateTimer.Stop();

			if(StartupCallbacks != null)
			{
				foreach(WorkspaceStartupCallback StartupCallback in StartupCallbacks)
				{
					StartupCallback(this, true);
				}
				StartupCallbacks = null;
			}

			if(StartupTimer != null)
			{
				StartupTimer.Dispose();
				StartupTimer = null;
			}
			if(NotificationWindow != null)
			{
				NotificationWindow.Dispose();
				NotificationWindow = null;
			}
			if(PerforceMonitor != null)
			{
				PerforceMonitor.Dispose();
				PerforceMonitor = null;
			}
			if(Workspace != null)
			{
				Workspace.Dispose();
				Workspace = null;
			}
			if(EventMonitor != null)
			{
				EventMonitor.Dispose();
				EventMonitor = null;
			}
			if(BuildFont != null)
			{
				BuildFont.Dispose();
				BuildFont = null;
			}
			if(SelectedBuildFont != null)
			{
				SelectedBuildFont.Dispose();
				SelectedBuildFont = null;
			}
			if(BadgeFont != null)
			{
				BadgeFont.Dispose();
				BadgeFont = null;
			}

			base.Dispose(disposing);
		}

		public bool IsBusy()
		{
			return Workspace.IsBusy();
		}

		public bool CanSyncNow()
		{
			return Workspace != null && !Workspace.IsBusy();
		}

		public bool CanLaunchEditor()
		{
			return Workspace != null && !Workspace.IsBusy() && Workspace.CurrentChangeNumber != -1;
		}

		private void MainWindow_Load(object sender, EventArgs e)
		{
			UpdateStatusPanel();
		}

		private void ShowErrorDialog(string Format, params object[] Args)
		{
			string Message = String.Format(Format, Args);
			Log.WriteLine(Message);
			MessageBox.Show(Message);
		}

		public bool CanClose()
		{
			CancelWorkspaceUpdate();
			return !Workspace.IsBusy();
		}

		private void StreamChanged()
		{
			Owner.StreamChanged(this);
/*
			StatusPanel.SuspendDisplay();

			string PrevSelectedFileName = SelectedFileName;
			if(TryCloseProject())
			{
				OpenProject(PrevSelectedFileName);
			}

			StatusPanel.ResumeDisplay();*/
		}

		private void StreamChangedCallback()
		{
			MainThreadSynchronizationContext.Post((o) => StreamChanged(), null);
		}

		private void LoginExpired()
		{
			if(!bIsDisposing)
			{
				Log.WriteLine("Login has expired. Requesting project to be closed.");
				Owner.RequestProjectChange(this, SelectedProject, false);
			}
		}

		private void LoginExpiredCallback()
		{
			MainThreadSynchronizationContext.Post((o) => LoginExpired(), null);
		}

		private void UpdateSyncConfig(int ChangeNumber, string SyncFilterHash)
		{
			WorkspaceSettings.CurrentProjectIdentifier = SelectedProjectIdentifier;
			WorkspaceSettings.CurrentChangeNumber = ChangeNumber;
			WorkspaceSettings.CurrentSyncFilterHash = SyncFilterHash;
			if(ChangeNumber == -1 || ChangeNumber != WorkspaceSettings.CurrentChangeNumber)
			{ 
				WorkspaceSettings.AdditionalChangeNumbers.Clear();
			}
			Settings.Save();
		}

		private void BuildList_OnScroll()
		{
			PendingSelectedChangeNumber = -1;
		}

		void ShrinkNumRequestedBuilds()
		{
			if(PerforceMonitor != null && BuildList.Items.Count > 0 && PendingSelectedChangeNumber == -1)
			{
				// Find the number of visible items using a (slightly wasteful) binary search
				int VisibleItemCount = 1;
				for(int StepSize = BuildList.Items.Count / 2; StepSize >= 1; )
				{
					int TestIndex = VisibleItemCount + StepSize;
					if(TestIndex < BuildList.Items.Count && BuildList.GetItemRect(TestIndex).Top < BuildList.Height)
					{
						VisibleItemCount += StepSize;
					}
					else
					{
						StepSize /= 2;
					}
				}

				// Figure out the last index to ensure is visible
				int LastVisibleIndex = VisibleItemCount;
				if(LastVisibleIndex >= ListIndexToChangeIndex.Count)
				{
					LastVisibleIndex = ListIndexToChangeIndex.Count - 1;
				}

				// Get the max number of changes to ensure this
				int NewPendingMaxChanges = ListIndexToChangeIndex[LastVisibleIndex];
				NewPendingMaxChanges = PerforceMonitor.InitialMaxChangesValue + ((Math.Max(NewPendingMaxChanges - PerforceMonitor.InitialMaxChangesValue, 0) + BuildListExpandCount - 1) / BuildListExpandCount) * BuildListExpandCount;

				// Shrink the number of changes retained by the PerforceMonitor class
				if(PerforceMonitor.PendingMaxChanges > NewPendingMaxChanges)
				{
					PerforceMonitor.PendingMaxChanges = NewPendingMaxChanges;
				}
			}
		}

		void StartSync(int ChangeNumber)
		{
			StartSync(ChangeNumber, null);
		}

		void StartSync(int ChangeNumber, WorkspaceUpdateCallback Callback)
		{
			WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles;
			if(Settings.bBuildAfterSync)
			{
				Options |= WorkspaceUpdateOptions.Build;
			}
			if(Settings.bBuildAfterSync && Settings.bRunAfterSync)
			{
				Options |= WorkspaceUpdateOptions.RunAfterSync;
			}
			if(Settings.bOpenSolutionAfterSync)
			{
				Options |= WorkspaceUpdateOptions.OpenSolutionAfterSync;
			}
			StartWorkspaceUpdate(ChangeNumber, Options, Callback);
		}

		void StartWorkspaceUpdate(int ChangeNumber, WorkspaceUpdateOptions Options)
		{
			StartWorkspaceUpdate(ChangeNumber, Options, null);
		}

		void StartWorkspaceUpdate(int ChangeNumber, WorkspaceUpdateOptions Options, WorkspaceUpdateCallback Callback)
		{
			if((Options & (WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.Build)) != 0 && GetProcessesRunningInWorkspace().Length > 0)
			{
				if((Options & WorkspaceUpdateOptions.ScheduledBuild) != 0)
				{
					SyncLog.Clear();
					SyncLog.AppendLine("Editor is open; scheduled sync has been aborted.");
					return;
				}
				else
				{
					if(!WaitForProgramsToFinish())
					{
						return;
					}
				}
			}

			string[] CombinedSyncFilter = UserSettings.GetCombinedSyncFilter(Workspace.GetSyncCategories(), Settings.SyncView, Settings.SyncExcludedCategories, WorkspaceSettings.SyncView, WorkspaceSettings.SyncIncludedCategories, WorkspaceSettings.SyncExcludedCategories);

			WorkspaceUpdateContext Context = new WorkspaceUpdateContext(ChangeNumber, Options, CombinedSyncFilter, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, null, GetWorkspaceVariables(ChangeNumber));
			if(Options.HasFlag(WorkspaceUpdateOptions.SyncArchives))
			{
				string EditorArchivePath = null;
				if(ShouldSyncPrecompiledEditor)
				{
					EditorArchivePath = GetArchivePathForChangeNumber(ChangeNumber);
					if(EditorArchivePath == null)
					{
						MessageBox.Show("There are no compiled editor binaries for this change. To sync it, you must disable syncing of precompiled editor binaries.");
						return;
					}
					Context.Options &= ~WorkspaceUpdateOptions.GenerateProjectFiles;
				}
				Context.ArchiveTypeToDepotPath.Add(EditorArchiveType, EditorArchivePath);
			}
			StartWorkspaceUpdate(Context, Callback);
		}

		void StartWorkspaceUpdate(WorkspaceUpdateContext Context, WorkspaceUpdateCallback Callback)
		{
			if(Settings.bAutoResolveConflicts)
			{
				Context.Options |= WorkspaceUpdateOptions.AutoResolveChanges;
			}
			if(Settings.bUseIncrementalBuilds)
			{
				Context.Options |= WorkspaceUpdateOptions.UseIncrementalBuilds;
			}
			if(WorkspaceSettings.bSyncAllProjects ?? Settings.bSyncAllProjects)
			{
				Context.Options |= WorkspaceUpdateOptions.SyncAllProjects | WorkspaceUpdateOptions.IncludeAllProjectsInSolution;
			}
			if(WorkspaceSettings.bIncludeAllProjectsInSolution ?? Settings.bIncludeAllProjectsInSolution)
			{
				Context.Options |= WorkspaceUpdateOptions.IncludeAllProjectsInSolution;
			}

			UpdateCallback = Callback;

			Context.StartTime = DateTime.UtcNow;
			Context.PerforceSyncOptions = (PerforceSyncOptions)Settings.SyncOptions.Clone();

			Log.WriteLine("Updating workspace at {0}...", Context.StartTime.ToLocalTime().ToString());
			Log.WriteLine("  ChangeNumber={0}", Context.ChangeNumber);
			Log.WriteLine("  Options={0}", Context.Options.ToString());
			Log.WriteLine("  Clobbering {0} files", Context.ClobberFiles.Count);

			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync))
			{
				UpdateSyncConfig(-1, Workspace.CurrentSyncFilterHash);
				EventMonitor.PostEvent(Context.ChangeNumber, EventType.Syncing);
			}

			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) || Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				if(!Context.Options.HasFlag(WorkspaceUpdateOptions.ContentOnly) && (Context.CustomBuildSteps == null || Context.CustomBuildSteps.Count == 0))
				{
					foreach(BuildConfig Config in Enum.GetValues(typeof(BuildConfig)))
					{
						List<string> EditorReceiptPaths = GetEditorReceiptPaths(Config);
						foreach(string EditorReceiptPath in EditorReceiptPaths)
						{
							if(File.Exists(EditorReceiptPath))
							{
								try { File.Delete(EditorReceiptPath); } catch(Exception){ }
							}
						}
					}
				}
			}

			SyncLog.Clear();
			Workspace.Update(Context);
			UpdateSyncActionCheckboxes();
			Refresh();
			UpdateTimer.Start();
		}

		void CancelWorkspaceUpdate()
		{
			if(Workspace.IsBusy() && MessageBox.Show("Are you sure you want to cancel the current operation?", "Cancel operation", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				WorkspaceSettings.LastSyncChangeNumber = Workspace.PendingChangeNumber;
				WorkspaceSettings.LastSyncResult = WorkspaceUpdateResult.Canceled;
				WorkspaceSettings.LastSyncResultMessage = null;
				WorkspaceSettings.LastSyncTime = null;
				WorkspaceSettings.LastSyncDurationSeconds = 0;
				Settings.Save();

				Workspace.CancelUpdate();

				if(UpdateCallback != null)
				{
					UpdateCallback(WorkspaceUpdateResult.Canceled);
					UpdateCallback = null;
				}

				UpdateTimer.Stop();

				UpdateSyncActionCheckboxes();
				Refresh();
				UpdateSyncConfig(Workspace.CurrentChangeNumber, Workspace.CurrentSyncFilterHash);
				UpdateStatusPanel();
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				Owner.UpdateProgress();
			}
		}

		void UpdateCompleteCallback(WorkspaceUpdateContext Context, WorkspaceUpdateResult Result, string ResultMessage)
		{
			MainThreadSynchronizationContext.Post((o) => UpdateComplete(Context, Result, ResultMessage), null);
		}

		void UpdateComplete(WorkspaceUpdateContext Context, WorkspaceUpdateResult Result, string ResultMessage)
		{
			UpdateTimer.Stop();

			UpdateSyncConfig(Workspace.CurrentChangeNumber, Workspace.CurrentSyncFilterHash);

			if(Result == WorkspaceUpdateResult.Success && Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
			{
				WorkspaceSettings.AdditionalChangeNumbers.Add(Context.ChangeNumber);
				Settings.Save();
			}

			if(Result == WorkspaceUpdateResult.Success && WorkspaceSettings.ExpandedArchiveTypes != null)
			{
				WorkspaceSettings.ExpandedArchiveTypes = WorkspaceSettings.ExpandedArchiveTypes.Except(Context.ArchiveTypeToDepotPath.Where(x => x.Value == null).Select(x => x.Key)).ToArray();
			}

			WorkspaceSettings.LastSyncChangeNumber = Context.ChangeNumber;
			WorkspaceSettings.LastSyncResult = Result;
			WorkspaceSettings.LastSyncResultMessage = ResultMessage;
			WorkspaceSettings.LastSyncTime = DateTime.UtcNow;
			WorkspaceSettings.LastSyncDurationSeconds = (int)(WorkspaceSettings.LastSyncTime.Value - Context.StartTime).TotalSeconds;
			WorkspaceSettings.LastBuiltChangeNumber = Workspace.LastBuiltChangeNumber;
			Settings.Save();

			if(Result == WorkspaceUpdateResult.FilesToResolve)
			{
				MessageBox.Show("You have files to resolve after syncing your workspace. Please check P4.");
			}
			else if(Result == WorkspaceUpdateResult.FilesToDelete)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Paused, 0.0f);
				Owner.UpdateProgress();

				DeleteWindow Window = new DeleteWindow(Context.DeleteFiles);
				if(Window.ShowDialog(this) == DialogResult.OK)
				{
					StartWorkspaceUpdate(Context, UpdateCallback);
					return;
				}
			}
			else if(Result == WorkspaceUpdateResult.FilesToClobber)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Paused, 0.0f);
				Owner.UpdateProgress();

				ClobberWindow Window = new ClobberWindow(Context.ClobberFiles);
				if(Window.ShowDialog(this) == DialogResult.OK)
				{
					StartWorkspaceUpdate(Context, UpdateCallback);
					return;
				}
			}
			else if(Result == WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace)
			{
				EventMonitor.PostEvent(Context.ChangeNumber, EventType.DoesNotCompile);
			}
			else if(Result == WorkspaceUpdateResult.Success)
			{
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
				{
					EventMonitor.PostEvent(Context.ChangeNumber, EventType.Compiles);
				}
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.RunAfterSync))
				{
					LaunchEditor();
				}
				if(Context.Options.HasFlag(WorkspaceUpdateOptions.OpenSolutionAfterSync))
				{
					OpenSolution();
				}
			}

			if(UpdateCallback != null)
			{
				UpdateCallback(Result);
				UpdateCallback = null;
			}

			DesiredTaskbarState = Tuple.Create((Result == WorkspaceUpdateResult.Success)? TaskbarState.NoProgress : TaskbarState.Error, 0.0f);
			Owner.UpdateProgress();

			BuildList.Invalidate();
			Refresh();
			UpdateStatusPanel();
			UpdateSyncActionCheckboxes();

			// Do this last because it may result in the control being disposed
			if(Result == WorkspaceUpdateResult.FailedToSyncLoginExpired)
			{
				LoginExpired();
			}
		}

		void UpdateBuildListCallback()
		{
			if(!bUpdateBuildListPosted)
			{
				bUpdateBuildListPosted = true;
				MainThreadSynchronizationContext.Post((o) => { bUpdateBuildListPosted = false; if(!bIsDisposing) { UpdateBuildList(); } }, null);
			}
		}

		void UpdateBuildList()
		{
			if(SelectedFileName != null)
			{
				List<int> SelectedChanges = new List<int>();
				foreach(ListViewItem SelectedItem in BuildList.SelectedItems)
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)SelectedItem.Tag;
					if(Change != null)
					{
						SelectedChanges.Add(Change.Number);
					}
				}

				ChangeNumberToArchivePath.Clear();
				ChangeNumberToLayoutInfo.Clear();

				ExpandItem = null;

				BuildList.BeginUpdate();

				foreach(ListViewGroup Group in BuildList.Groups)
				{
					Group.Name = "xxx " + Group.Name;
				}

				int RemoveItems = BuildList.Items.Count;
				int RemoveGroups = BuildList.Groups.Count;

				List<PerforceChangeSummary> Changes = PerforceMonitor.GetChanges();
				EventMonitor.FilterChanges(Changes.Select(x => x.Number));

				PromotedChangeNumbers = PerforceMonitor.GetPromotedChangeNumbers();

				string[] ExcludeChanges = new string[0];
				if(Workspace != null)
				{
					ConfigFile ProjectConfigFile = PerforceMonitor.LatestProjectConfigFile;
					if(ProjectConfigFile != null)
					{
						ExcludeChanges = ProjectConfigFile.GetValues("Options.ExcludeChanges", ExcludeChanges);
					}
				}

				bool bFirstChange = true;
				bool bHideUnreviewed = !Settings.bShowUnreviewedChanges;

				NumChanges = Changes.Count;
				ListIndexToChangeIndex = new List<int>();
				SortedChangeNumbers = new List<int>();
				
				for(int ChangeIdx = 0; ChangeIdx < Changes.Count; ChangeIdx++)
				{
					PerforceChangeSummary Change = Changes[ChangeIdx];
					if(ShouldShowChange(Change, ExcludeChanges) || PromotedChangeNumbers.Contains(Change.Number))
					{
						SortedChangeNumbers.Add(Change.Number);

						if(!bHideUnreviewed || (!EventMonitor.IsUnderInvestigation(Change.Number) && (ShouldIncludeInReviewedList(Change.Number) || bFirstChange)))
						{
							bFirstChange = false;

							ListViewGroup Group;

							DateTime DisplayTime = Change.Date;
							if(Settings.bShowLocalTimes)
							{
								DisplayTime = (DisplayTime - PerforceMonitor.ServerTimeZone).ToLocalTime();
							}

							string GroupName = DisplayTime.ToString("D");//"dddd\\,\\ h\\.mmtt");
							for(int Idx = 0;;Idx++)
							{
								if(Idx == BuildList.Groups.Count)
								{
									Group = new ListViewGroup(GroupName);
									Group.Name = GroupName;
									BuildList.Groups.Add(Group);
									break;
								}
								else if(BuildList.Groups[Idx].Name == GroupName)
								{
									Group = BuildList.Groups[Idx];
									break;
								}
							}

							string[] SubItemLabels = new string[BuildList.Columns.Count];
							SubItemLabels[ChangeColumn.Index] = String.Format("{0}", Change.Number);
							SubItemLabels[TimeColumn.Index] = DisplayTime.ToString("h\\.mmtt");
							SubItemLabels[AuthorColumn.Index] = FormatUserName(Change.User);
							SubItemLabels[DescriptionColumn.Index] = Change.Description.Replace('\n', ' ');

							ListViewItem Item = new ListViewItem(Group);
							Item.Tag = Change;
							Item.Selected = SelectedChanges.Contains(Change.Number);
							for(int ColumnIdx = 1; ColumnIdx < BuildList.Columns.Count; ColumnIdx++)
							{
								Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, SubItemLabels[ColumnIdx] ?? ""));
							}

							// Insert it at the right position within the group
							int GroupInsertIdx = 0;
							while(GroupInsertIdx < Group.Items.Count && Change.Number < ((PerforceChangeSummary)Group.Items[GroupInsertIdx].Tag).Number)
							{
								GroupInsertIdx++;
							}
							Group.Items.Insert(GroupInsertIdx, Item);

							// Insert it at the right place in the list
							BuildList.Items.Add(Item);

							// Store off the list index for this change
							ListIndexToChangeIndex.Add(ChangeIdx);
						}
					}
				}

				SortedChangeNumbers.Sort();

				for(int Idx = 0; Idx < RemoveItems; Idx++)
				{
					BuildList.Items.RemoveAt(0);
				}
				for(int Idx = 0; Idx < RemoveGroups; Idx++)
				{
					BuildList.Groups.RemoveAt(0);
				}

				if(Changes.Count > 0)
				{
					ExpandItem = new ListViewItem((BuildList.Groups.Count > 0)? BuildList.Groups[BuildList.Groups.Count - 1] : null);
					ExpandItem.Tag = null;
					ExpandItem.Selected = false;
					ExpandItem.Text = "";
					for(int ColumnIdx = 1; ColumnIdx < BuildList.Columns.Count; ColumnIdx++)
					{
						ExpandItem.SubItems.Add(new ListViewItem.ListViewSubItem(ExpandItem, ""));
					}
					BuildList.Items.Add(ExpandItem);
				}

				BuildList.EndUpdate();

				if(PendingSelectedChangeNumber != -1)
				{
					SelectChange(PendingSelectedChangeNumber);
				}
			}

			if(BuildList.HoverItem > BuildList.Items.Count)
			{
				BuildList.HoverItem = -1;
			}

			UpdateBuildFailureNotification();

			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
		}

		bool ShouldShowChange(PerforceChangeSummary Change, string[] ExcludeChanges)
		{
			if(ProjectSettings.FilterType != FilterType.None)
			{
				PerforceChangeDetails Details;
				if(!PerforceMonitor.TryGetChangeDetails(Change.Number, out Details))
				{
					return false;
				}
				if(ProjectSettings.FilterType == FilterType.Code && !Details.bContainsCode)
				{
					return false;
				}
				if(ProjectSettings.FilterType == FilterType.Content && !Details.bContainsContent)
				{
					return false;
				}
			}
			if(ProjectSettings.FilterBadges.Count > 0)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
				if(Summary == null || !Summary.Badges.Any(x => ProjectSettings.FilterBadges.Contains(x.BadgeName)))
				{
					return false;
				}
			}
			if(!Settings.bShowAutomatedChanges)
			{
				foreach(string ExcludeChange in ExcludeChanges)
				{
					if(Regex.IsMatch(Change.Description, ExcludeChange, RegexOptions.IgnoreCase))
					{
						return false;
					}
				}

				if(String.Compare(Change.User, "buildmachine", true) == 0 && Change.Description.IndexOf("lightmaps", StringComparison.InvariantCultureIgnoreCase) == -1)
				{
					return false;
				}
			}
			if(IsBisectModeEnabled() && !WorkspaceSettings.ChangeNumberToBisectState.ContainsKey(Change.Number))
			{
				return false;
			}
			return true;
		}

		void UpdateBuildMetadataCallback()
		{
			if(!bUpdateBuildMetadataPosted)
			{
				bUpdateBuildMetadataPosted = true;
				MainThreadSynchronizationContext.Post((o) => { bUpdateBuildMetadataPosted = false; if(!bIsDisposing) { UpdateBuildMetadata(); } }, null);
			}
		}

		void UpdateBuildMetadata()
		{
			// Update the column settings first, since this may affect the badges we hide
			UpdateColumnSettings();

			// Clear the badge size cache
			BadgeLabelToSize.Clear();

			// Reset the badge groups
			Dictionary<string, string> BadgeNameToGroup = new Dictionary<string, string>();

			// Read the group mappings from the config file
			string GroupDefinitions;
			if(TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "BadgeGroups", out GroupDefinitions))
			{
				string[] GroupDefinitionsArray = GroupDefinitions.Split('\n');
				for(int Idx = 0; Idx < GroupDefinitionsArray.Length; Idx++)
				{
					string GroupName = String.Format("{0:0000}", Idx);
					foreach(string BadgeName in GroupDefinitionsArray[Idx].Split(',').Select(x => x.Trim()))
					{
						BadgeNameToGroup[BadgeName] = GroupName;
					}
				}
			}

			// Add a dummy group for any other badges we have
			foreach(ListViewItem Item in BuildList.Items)
			{
				if(Item.Tag != null)
				{
					EventSummary Summary = EventMonitor.GetSummaryForChange(((PerforceChangeSummary)Item.Tag).Number);
					if(Summary != null)
					{
						foreach(BadgeData Badge in Summary.Badges)
						{
							string BadgeSlot = Badge.BadgeName;
							if(!BadgeNameToGroup.ContainsKey(BadgeSlot))
							{
								BadgeNameToGroup.Add(BadgeSlot, "XXXX");
							}
						}
					}
				}
			}

			// Remove anything that's a service badge
			foreach(KeyValuePair<string, BadgeData> ServiceBadge in ServiceBadges)
			{
				BadgeNameToGroup.Remove(ServiceBadge.Key);
			}

			// Remove everything that's in a custom column
			foreach(ColumnHeader CustomColumn in CustomColumns)
			{
				ConfigObject ColumnConfig = (ConfigObject)CustomColumn.Tag;
				foreach(string BadgeName in ColumnConfig.GetValue("Badges", "").Split(','))
				{
					BadgeNameToGroup.Remove(BadgeName);
				}
			}

			// Sort the list of groups for display
			BadgeNameAndGroupPairs = BadgeNameToGroup.OrderBy(x => x.Value).ThenBy(x => x.Key).ToList();

			// Figure out whether to show smaller badges due to limited space
			UpdateMaxBuildBadgeChars();

			// Update everything else
			ChangeNumberToArchivePath.Clear();
			ChangeNumberToLayoutInfo.Clear();
			BuildList.Invalidate();
			UpdateServiceBadges();
			UpdateStatusPanel();
			UpdateBuildFailureNotification();
			CheckForStartupComplete();

			// If we are filtering by badges, we may also need to update the build list
			if(ProjectSettings.FilterType != FilterType.None || ProjectSettings.FilterBadges.Count > 0)
			{
				UpdateBuildList();
			}
		}

		void UpdateMaxBuildBadgeChars()
		{
			int NewMaxBuildBadgeChars;
			if(GetBuildBadgeStripWidth(-1) < CISColumn.Width)
			{
				NewMaxBuildBadgeChars = -1;
			}
			else if(GetBuildBadgeStripWidth(3) < CISColumn.Width)
			{
				NewMaxBuildBadgeChars = 3;
			}
			else if(GetBuildBadgeStripWidth(2) < CISColumn.Width)
			{
				NewMaxBuildBadgeChars = 2;
			}
			else
			{
				NewMaxBuildBadgeChars = 1;
			}

			if(NewMaxBuildBadgeChars != MaxBuildBadgeChars)
			{
				ChangeNumberToLayoutInfo.Clear();
				BuildList.Invalidate();
				MaxBuildBadgeChars = NewMaxBuildBadgeChars;
			}
		}

		int GetBuildBadgeStripWidth(int MaxNumChars)
		{
			// Create dummy badges for each badge name
			List<BadgeInfo> DummyBadges = new List<BadgeInfo>();
			foreach(KeyValuePair<string, string> BadgeNameAndGroupPair in BadgeNameAndGroupPairs)
			{
				string BadgeName = BadgeNameAndGroupPair.Key;
				if(MaxNumChars != -1 && BadgeName.Length > MaxNumChars)
				{
					BadgeName = BadgeName.Substring(0, MaxNumChars);
				}

				BadgeInfo DummyBadge = CreateBadge(-1, BadgeName, BadgeNameAndGroupPair.Value, null);
				DummyBadges.Add(DummyBadge);
			}

			// Check if they fit within the column
			int Width = 0;
			if(DummyBadges.Count > 0)
			{
				LayoutBadges(DummyBadges);
				Width = DummyBadges[DummyBadges.Count - 1].Offset + DummyBadges[DummyBadges.Count - 1].Width;
			}
			return Width;
		}

		bool ShouldIncludeInReviewedList(int ChangeNumber)
		{
			if(PromotedChangeNumbers.Contains(ChangeNumber))
			{
				return true;
			}

			EventSummary Review = EventMonitor.GetSummaryForChange(ChangeNumber);
			if(Review != null)
			{
				if(Review.LastStarReview != null && Review.LastStarReview.Type == EventType.Starred)
				{
					return true;
				}
				if(Review.Verdict == ReviewVerdict.Good || Review.Verdict == ReviewVerdict.Mixed)
				{
					return true;
				}
			}
			return false;
		}

		void UpdateReviewsCallback()
		{
			if(!bUpdateReviewsPosted)
			{
				bUpdateReviewsPosted = true;
				MainThreadSynchronizationContext.Post((o) => { bUpdateReviewsPosted = false; if(!bIsDisposing) { UpdateReviews(); } }, null);
			}
		}

		void UpdateReviews()
		{
			ChangeNumberToArchivePath.Clear();
			ChangeNumberToLayoutInfo.Clear();
			EventMonitor.ApplyUpdates();
			Refresh();
			UpdateBuildFailureNotification();
			CheckForStartupComplete();
		}

		void UpdateBuildFailureNotification()
		{
			int LastChangeByCurrentUser = PerforceMonitor.LastChangeByCurrentUser;
			int LastCodeChangeByCurrentUser = PerforceMonitor.LastCodeChangeByCurrentUser;

			// Find all the badges which should notify users due to content changes
			HashSet<string> ContentBadges = new HashSet<string>();
			ContentBadges.UnionWith(PerforceMonitor.LatestProjectConfigFile.GetValues("Notifications.ContentBadges", new string[0]));

			// Find the most recent build of each type, and the last time it succeeded
			Dictionary<string, BadgeData> TypeToLastBuild = new Dictionary<string,BadgeData>();
			Dictionary<string, BadgeData> TypeToLastSucceededBuild = new Dictionary<string,BadgeData>();
			for(int Idx = SortedChangeNumbers.Count - 1; Idx >= 0; Idx--)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(SortedChangeNumbers[Idx]);
				if(Summary != null)
				{
					foreach(BadgeData Badge in Summary.Badges)
					{
						if(!TypeToLastBuild.ContainsKey(Badge.BuildType) && (Badge.Result == BadgeResult.Success || Badge.Result == BadgeResult.Warning || Badge.Result == BadgeResult.Failure))
						{
							TypeToLastBuild.Add(Badge.BuildType, Badge);
						}
						if(!TypeToLastSucceededBuild.ContainsKey(Badge.BuildType) && Badge.Result == BadgeResult.Success)
						{
							TypeToLastSucceededBuild.Add(Badge.BuildType, Badge);
						}
					}
				}
			}

			// Find all the build types that the user needs to be notified about.
			int RequireNotificationForChange = -1;
			List<BadgeData> NotifyBuilds = new List<BadgeData>();
			foreach(BadgeData LastBuild in TypeToLastBuild.Values.OrderBy(x => x.BuildType))
			{
				if(LastBuild.Result == BadgeResult.Failure || LastBuild.Result == BadgeResult.Warning)
				{
					// Get the last submitted changelist by this user of the correct type
					int LastChangeByCurrentUserOfType;
					if(ContentBadges.Contains(LastBuild.BuildType))
					{
						LastChangeByCurrentUserOfType = LastChangeByCurrentUser;
					}
					else
					{
						LastChangeByCurrentUserOfType = LastCodeChangeByCurrentUser;
					}

					// Check if the failed build was after we submitted
					if(LastChangeByCurrentUserOfType > 0 && LastBuild.ChangeNumber >= LastChangeByCurrentUserOfType)
					{
						// And check that there wasn't a successful build after we submitted (if there was, we're in the clear)
						BadgeData LastSuccessfulBuild;
						if(!TypeToLastSucceededBuild.TryGetValue(LastBuild.BuildType, out LastSuccessfulBuild) || LastSuccessfulBuild.ChangeNumber < LastChangeByCurrentUserOfType)
						{
							// Add it to the list of notifications
							NotifyBuilds.Add(LastBuild);

							// Check if this is a new notification, rather than one we've already dismissed
							int NotifiedChangeNumber;
							if(!NotifiedBuildTypeToChangeNumber.TryGetValue(LastBuild.BuildType, out NotifiedChangeNumber) || NotifiedChangeNumber < LastChangeByCurrentUserOfType)
							{
								RequireNotificationForChange = Math.Max(RequireNotificationForChange, LastChangeByCurrentUserOfType);
							}
						}
					}
				}
			}

			// If there's anything we haven't already notified the user about, do so now
			if(RequireNotificationForChange != -1)
			{
				// Format the platform list
				StringBuilder PlatformList = new StringBuilder(NotifyBuilds[0].BuildType);
				for(int Idx = 1; Idx < NotifyBuilds.Count - 1; Idx++)
				{
					PlatformList.AppendFormat(", {0}", NotifyBuilds[Idx].BuildType);
				}
				if(NotifyBuilds.Count > 1)
				{
					PlatformList.AppendFormat(" and {0}", NotifyBuilds[NotifyBuilds.Count - 1].BuildType);
				}

				// Show the balloon tooltip
				if(NotifyBuilds.Any(x => x.Result == BadgeResult.Failure))
				{
					string Title = String.Format("{0} Errors", PlatformList.ToString());
					string Message = String.Format("CIS failed after your last submitted changelist ({0}).", RequireNotificationForChange);
					NotificationWindow.Show(NotificationType.Error, Title, Message);
				}
				else
				{
					string Title = String.Format("{0} Warnings", PlatformList.ToString());
					string Message = String.Format("CIS completed with warnings after your last submitted changelist ({0}).", RequireNotificationForChange);
					NotificationWindow.Show(NotificationType.Warning, Title, Message);
				}

				// Set the link to open the right build pages
				int HighlightChange = NotifyBuilds.Max(x => x.ChangeNumber);
				NotificationWindow.OnMoreInformation = () => { Owner.ShowAndActivate(); SelectChange(HighlightChange); };
				
				// Don't show messages for this change again
				foreach(BadgeData NotifyBuild in NotifyBuilds)
				{
					NotifiedBuildTypeToChangeNumber[NotifyBuild.BuildType] = RequireNotificationForChange;
				}
			}
		}

		private void BuildList_DrawColumnHeader(object sender, DrawListViewColumnHeaderEventArgs e)
		{
			e.DrawDefault = true;
		}

		class ExpandRowLayout
		{
			public string MainText;
			public Rectangle MainRect;
			public string LinkText;
			public Rectangle LinkRect;
		}

		private ExpandRowLayout LayoutExpandRow(Graphics Graphics, Rectangle Bounds)
		{
			ExpandRowLayout Layout = new ExpandRowLayout();

			string ShowingChanges = String.Format("Showing {0}/{1} changes.", ListIndexToChangeIndex.Count, NumChanges);

			int CurrentMaxChanges = PerforceMonitor.CurrentMaxChanges;
			if(PerforceMonitor.PendingMaxChanges > CurrentMaxChanges)
			{
				Layout.MainText = String.Format("{0}. Fetching {1} more from server...  ", ShowingChanges, PerforceMonitor.PendingMaxChanges - CurrentMaxChanges);
				Layout.LinkText = "Cancel";
			}
			else if(PerforceMonitor.CurrentMaxChanges > NumChanges)
			{
				Layout.MainText = ShowingChanges;
				Layout.LinkText = "";
			}
			else
			{
				Layout.MainText = String.Format("{0}  ", ShowingChanges);
				Layout.LinkText = "Show more...";
			}

			Size MainTextSize = TextRenderer.MeasureText(Graphics, Layout.MainText, BuildFont, new Size(1000, 1000), TextFormatFlags.NoPadding);
			Size LinkTextSize = TextRenderer.MeasureText(Graphics, Layout.LinkText, BuildFont, new Size(1000, 1000), TextFormatFlags.NoPadding);

			int MinX = Bounds.Left + (Bounds.Width - MainTextSize.Width - LinkTextSize.Width) / 2;
			int MinY = Bounds.Bottom - MainTextSize.Height - 1;

			Layout.MainRect = new Rectangle(new Point(MinX, MinY), MainTextSize);
			Layout.LinkRect = new Rectangle(new Point(MinX + MainTextSize.Width, MinY), LinkTextSize);

			return Layout;
		}

		private void DrawExpandRow(Graphics Graphics, ExpandRowLayout Layout)
		{
			TextRenderer.DrawText(Graphics, Layout.MainText, BuildFont, Layout.MainRect, SystemColors.WindowText, TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix | TextFormatFlags.NoPadding);

			Color LinkColor = SystemColors.HotTrack;
			if(bMouseOverExpandLink)
			{
				LinkColor = Color.FromArgb(LinkColor.B, LinkColor.G, LinkColor.R);
			}

			TextRenderer.DrawText(Graphics, Layout.LinkText, BuildFont, Layout.LinkRect, LinkColor, TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix | TextFormatFlags.NoPadding);
		}

		private bool HitTestExpandLink(Point Location)
		{
			if(ExpandItem == null)
			{
				return false;
			}
			using(Graphics Graphics = Graphics.FromHwnd(IntPtr.Zero))
			{
				ExpandRowLayout Layout = LayoutExpandRow(Graphics, ExpandItem.Bounds);
				return Layout.LinkRect.Contains(Location);
			}
		}

		private void BuildList_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			if(e.Item == ExpandItem)
			{
				BuildList.DrawDefaultBackground(e.Graphics, e.Bounds);

				ExpandRowLayout ExpandLayout = LayoutExpandRow(e.Graphics, e.Bounds);
				DrawExpandRow(e.Graphics, ExpandLayout);
			}
			else
			{
				if(e.State.HasFlag(ListViewItemStates.Selected))
				{
					BuildList.DrawSelectedBackground(e.Graphics, e.Bounds);
				}
				else if(e.ItemIndex == BuildList.HoverItem)
				{
					BuildList.DrawTrackedBackground(e.Graphics, e.Bounds);
				}
				else if(((PerforceChangeSummary)e.Item.Tag).Number == Workspace.PendingChangeNumber)
				{
					BuildList.DrawTrackedBackground(e.Graphics, e.Bounds);
				}
				else
				{
					BuildList.DrawDefaultBackground(e.Graphics, e.Bounds);
				}
			}
		}

		private string GetArchivePathForChangeNumber(int ChangeNumber)
		{
			string ArchivePath;
			if(!ChangeNumberToArchivePath.TryGetValue(ChangeNumber, out ArchivePath))
			{
				PerforceChangeDetails Details;
				if(PerforceMonitor.TryGetChangeDetails(ChangeNumber, out Details))
				{
					// Try to get the archive for this CL
					if(!PerforceMonitor.TryGetArchivePathForChangeNumber(ChangeNumber, out ArchivePath) && !Details.bContainsCode)
					{
						// Otherwise if it's a content-only change, find the previous build any use the archive path from that
						int Index = SortedChangeNumbers.BinarySearch(ChangeNumber);
						if(Index > 0)
						{
							ArchivePath = GetArchivePathForChangeNumber(SortedChangeNumbers[Index - 1]);
						}
					}
				}
				ChangeNumberToArchivePath.Add(ChangeNumber, ArchivePath);
			}
			return ArchivePath;
		}

		private Color Blend(Color First, Color Second, float T)
		{
			return Color.FromArgb((int)(First.R + (Second.R - First.R) * T), (int)(First.G + (Second.G - First.G) * T), (int)(First.B + (Second.B - First.B) * T));
		}

		private bool CanSyncChange(int ChangeNumber)
		{
			return !ShouldSyncPrecompiledEditor || GetArchivePathForChangeNumber(ChangeNumber) != null;
		}

		private ChangeLayoutInfo GetChangeLayoutInfo(PerforceChangeSummary Change)
		{
			ChangeLayoutInfo LayoutInfo;
			if(!ChangeNumberToLayoutInfo.TryGetValue(Change.Number, out LayoutInfo))
			{
				LayoutInfo = new ChangeLayoutInfo();

				LayoutInfo.DescriptionBadges = CreateDescriptionBadges(Change);
				LayoutInfo.TypeBadges = CreateTypeBadges(Change.Number);

				EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
				LayoutInfo.BuildBadges = CreateBuildBadges(Change.Number, Summary);
				LayoutInfo.CustomBadges = CreateCustomBadges(Change.Number, Summary);

				ChangeNumberToLayoutInfo.Add(Change.Number, LayoutInfo);
			}
			return LayoutInfo;
		}

		private void GetRemainingBisectRange(out int OutPassChangeNumber, out int OutFailChangeNumber)
		{
			int PassChangeNumber = -1;
			foreach(KeyValuePair<int, BisectState> Pair in WorkspaceSettings.ChangeNumberToBisectState)
			{
				if(Pair.Value == BisectState.Pass && (Pair.Key > PassChangeNumber || PassChangeNumber == -1))
				{
					PassChangeNumber = Pair.Key;
				}
			}

			int FailChangeNumber = -1;
			foreach(KeyValuePair<int, BisectState> Pair in WorkspaceSettings.ChangeNumberToBisectState)
			{
				if(Pair.Value == BisectState.Fail && Pair.Key > PassChangeNumber && (Pair.Key < FailChangeNumber || FailChangeNumber == -1))
				{
					FailChangeNumber = Pair.Key;
				}
			}

			OutPassChangeNumber = PassChangeNumber;
			OutFailChangeNumber = FailChangeNumber;
		}

		private void BuildList_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			PerforceChangeSummary Change = (PerforceChangeSummary)e.Item.Tag;
			if(Change == null)
			{
				return;
			}

			float DpiScaleX = e.Graphics.DpiX / 96.0f;
			float DpiScaleY = e.Graphics.DpiY / 96.0f;

			float IconY = e.Bounds.Top + (e.Bounds.Height - 16 * DpiScaleY) / 2;

			StringFormat Format = new StringFormat();
			Format.LineAlignment = StringAlignment.Center;
			Format.FormatFlags = StringFormatFlags.NoWrap;
			Format.Trimming = StringTrimming.EllipsisCharacter;

			Font CurrentFont = (Change.Number == Workspace.PendingChangeNumber || Change.Number == Workspace.CurrentChangeNumber)? SelectedBuildFont : BuildFont;

			bool bAllowSync = CanSyncChange(Change.Number);
			int BadgeAlpha = bAllowSync? 255 : 128;
			Color TextColor = (bAllowSync || Change.Number == Workspace.PendingChangeNumber || Change.Number == Workspace.CurrentChangeNumber || (WorkspaceSettings != null && WorkspaceSettings.AdditionalChangeNumbers.Contains(Change.Number)))? SystemColors.WindowText : Blend(SystemColors.Window, SystemColors.WindowText, 0.25f);

			const int FadeRange = 6;
			if(e.ItemIndex >= BuildList.Items.Count - FadeRange && NumChanges >= PerforceMonitor.CurrentMaxChanges)
			{
				float Opacity = (float)(BuildList.Items.Count - e.ItemIndex - 0.9f) / FadeRange;
				BadgeAlpha = (int)(BadgeAlpha * Opacity);
				TextColor = Blend(SystemColors.Window, TextColor, Opacity);
			}

			if(e.ColumnIndex == IconColumn.Index)
			{
				EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);

				float MinX = 4 * DpiScaleX;
				if((Summary != null && EventMonitor.WasSyncedByCurrentUser(Summary.ChangeNumber)) || (Workspace != null && Workspace.CurrentChangeNumber == Change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX * DpiScaleX, IconY, PreviousSyncIcon, GraphicsUnit.Pixel);
				}
				else if(WorkspaceSettings != null && WorkspaceSettings.AdditionalChangeNumbers.Contains(Change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX * DpiScaleX, IconY, AdditionalSyncIcon, GraphicsUnit.Pixel);
				}
				else if(bAllowSync && ((Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred) || PromotedChangeNumbers.Contains(Change.Number)))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX * DpiScaleX, IconY, PromotedBuildIcon, GraphicsUnit.Pixel);
				}
				MinX += PromotedBuildIcon.Width * DpiScaleX;

				if(bAllowSync)
				{
					Rectangle QualityIcon = DefaultBuildIcon;

					if(IsBisectModeEnabled())
					{
						int FirstPass, FirstFail;
						GetRemainingBisectRange(out FirstPass, out FirstFail);

						BisectState State;
						if(!WorkspaceSettings.ChangeNumberToBisectState.TryGetValue(Change.Number, out State) || State == BisectState.Exclude)
						{
							QualityIcon = new Rectangle(0, 0, 0, 0);
						}
						if(State == BisectState.Pass)
						{
							QualityIcon = BisectPassIcon;
						}
						else if(State == BisectState.Fail)
						{
							QualityIcon = BisectFailIcon;
						}
						else if(FirstFail != -1 && Change.Number > FirstFail)
						{
							QualityIcon = BisectImplicitFailIcon;
						}
						else if(FirstPass != -1 && Change.Number < FirstPass)
						{
							QualityIcon = BisectImplicitPassIcon;
						}
					}
					else
					{
						if(EventMonitor.IsUnderInvestigation(Change.Number))
						{
							QualityIcon = BadBuildIcon;
						}
						else if(Summary != null)
						{
							if(Summary.Verdict == ReviewVerdict.Good)
							{
								QualityIcon = GoodBuildIcon;
							}
							else if(Summary.Verdict == ReviewVerdict.Bad)
							{
								QualityIcon = BadBuildIcon;
							}
							else if(Summary.Verdict == ReviewVerdict.Mixed)
							{
								QualityIcon = MixedBuildIcon;
							}
						}
					}
					e.Graphics.DrawImage(Properties.Resources.Icons, MinX, IconY, QualityIcon, GraphicsUnit.Pixel);

					MinX += QualityIcon.Width * DpiScaleX;
				}
			}
			else if(e.ColumnIndex == ChangeColumn.Index || e.ColumnIndex == TimeColumn.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, CurrentFont, e.Bounds, TextColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if(e.ColumnIndex == AuthorColumn.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, CurrentFont, e.Bounds, TextColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if(e.ColumnIndex == DescriptionColumn.Index)
			{
				ChangeLayoutInfo Layout = GetChangeLayoutInfo(Change);

				Rectangle RemainingBounds = e.Bounds;
				if (Layout.DescriptionBadges.Count > 0)
				{
				    e.Graphics.IntersectClip(e.Bounds);
				    e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					RemainingBounds = new Rectangle(RemainingBounds.Left, RemainingBounds.Top, RemainingBounds.Width - (int)(2 * DpiScaleX), RemainingBounds.Height);

					Point ListLocation = GetBadgeListLocation(Layout.DescriptionBadges, RemainingBounds, HorizontalAlign.Right, VerticalAlignment.Middle);
					DrawBadges(e.Graphics, ListLocation, Layout.DescriptionBadges, BadgeAlpha);

					RemainingBounds = new Rectangle(RemainingBounds.Left, RemainingBounds.Top, ListLocation.X - RemainingBounds.Left - (int)(2 * DpiScaleX), RemainingBounds.Height);
				}

				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, CurrentFont, RemainingBounds, TextColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if(e.ColumnIndex == TypeColumn.Index)
			{
				ChangeLayoutInfo Layout = GetChangeLayoutInfo(Change);
				if(Layout.TypeBadges.Count > 0)
				{
				    e.Graphics.IntersectClip(e.Bounds);
				    e.Graphics.SmoothingMode = SmoothingMode.HighQuality;
					Point TypeLocation = GetBadgeListLocation(Layout.TypeBadges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
					DrawBadges(e.Graphics, TypeLocation, Layout.TypeBadges, BadgeAlpha);
				}
			}
			else if(e.ColumnIndex == CISColumn.Index)
			{
				ChangeLayoutInfo Layout = GetChangeLayoutInfo(Change);
				if(Layout.BuildBadges.Count > 0)
				{
				    e.Graphics.IntersectClip(e.Bounds);
				    e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					Point BuildsLocation = GetBadgeListLocation(Layout.BuildBadges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
					BuildsLocation.X = Math.Max(BuildsLocation.X, e.Bounds.Left);

					DrawBadges(e.Graphics, BuildsLocation, Layout.BuildBadges, BadgeAlpha);
				}
			}
			else if(e.ColumnIndex == StatusColumn.Index)
			{
				float MaxX = e.SubItem.Bounds.Right;

				if(Change.Number == Workspace.PendingChangeNumber && Workspace.IsBusy())
				{
					Tuple<string, float> Progress = Workspace.CurrentProgress;

					MaxX -= CancelIcon.Width;
					e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, CancelIcon, GraphicsUnit.Pixel);

					if(!Splitter.IsLogVisible())
					{
						MaxX -= InfoIcon.Width; 
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, InfoIcon, GraphicsUnit.Pixel);
					}

					float MinX = e.Bounds.Left;

					TextRenderer.DrawText(e.Graphics, Progress.Item1, CurrentFont, new Rectangle((int)MinX, e.Bounds.Top, (int)(MaxX - MinX), e.Bounds.Height), TextColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
				else
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					if(Change.Number == Workspace.CurrentChangeNumber)
					{
						EventData Review = EventMonitor.GetReviewByCurrentUser(Change.Number);

						MaxX -= FrownIcon.Width * DpiScaleX;
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, (Review == null || !EventMonitor.IsPositiveReview(Review.Type))? FrownIcon : DisabledFrownIcon, GraphicsUnit.Pixel);

						MaxX -= HappyIcon.Width * DpiScaleX;
						e.Graphics.DrawImage(Properties.Resources.Icons, MaxX, IconY, (Review == null || !EventMonitor.IsNegativeReview(Review.Type))? HappyIcon : DisabledHappyIcon, GraphicsUnit.Pixel);
					}
					else if(e.ItemIndex == BuildList.HoverItem && bAllowSync)
					{
						Rectangle SyncBadgeRectangle = GetSyncBadgeRectangle(e.SubItem.Bounds);
						DrawBadge(e.Graphics, SyncBadgeRectangle, "Sync", bHoverSync? Color.FromArgb(140, 180, 230) : Color.FromArgb(112, 146, 190), true, true);
						MaxX = SyncBadgeRectangle.Left - (int)(2 * DpiScaleX);
					}

					string SummaryText;
					if(WorkspaceSettings.LastSyncChangeNumber == -1 || WorkspaceSettings.LastSyncChangeNumber != Change.Number || !GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
					{
						StringBuilder SummaryTextBuilder = new StringBuilder();

						EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);

						AppendItemList(SummaryTextBuilder, " ", "Under investigation by {0}.", EventMonitor.GetInvestigatingUsers(Change.Number).Select(x => FormatUserName(x)));

						if(Summary != null)
						{
							string Comments = String.Join(", ", Summary.Comments.Where(x => !String.IsNullOrWhiteSpace(x.Text)).Select(x => String.Format("{0}: \"{1}\"", FormatUserName(x.UserName), x.Text)));
							if(Comments.Length > 0)
							{
								SummaryTextBuilder.Append(((SummaryTextBuilder.Length == 0)? ""  : " ") + Comments);
							}
							else
							{
								AppendItemList(SummaryTextBuilder, " ", "Used by {0}.", Summary.CurrentUsers.Select(x => FormatUserName(x)));
							}
						}

						SummaryText = (SummaryTextBuilder.Length == 0)? "No current users." : SummaryTextBuilder.ToString();
					}

					if(SummaryText != null && SummaryText.Contains('\n'))
					{
						SummaryText = SummaryText.Substring(0, SummaryText.IndexOf('\n')).TrimEnd() + "...";
					}

					TextRenderer.DrawText(e.Graphics, SummaryText, CurrentFont, new Rectangle(e.Bounds.Left, e.Bounds.Top, (int)MaxX - e.Bounds.Left, e.Bounds.Height), TextColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
			}
			else
			{
				ColumnHeader Column = BuildList.Columns[e.ColumnIndex];
				if(CustomColumns.Contains(Column))
				{
				    ChangeLayoutInfo Layout = GetChangeLayoutInfo(Change);

				    List<BadgeInfo> Badges;
				    if(Layout.CustomBadges.TryGetValue(Column.Text, out Badges) && Badges.Count > 0)
				    {
				        e.Graphics.IntersectClip(e.Bounds);
				        e.Graphics.SmoothingMode = SmoothingMode.HighQuality;
    
					    Point BuildsLocation = GetBadgeListLocation(Badges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
					    DrawBadges(e.Graphics, BuildsLocation, Badges, BadgeAlpha);
				    }
				}
			}
		}

		private void LayoutBadges(List<BadgeInfo> Badges)
		{
			int Offset = 0;
			for (int Idx = 0; Idx < Badges.Count; Idx++)
			{
				BadgeInfo Badge = Badges[Idx];

				if (Idx > 0 && Badge.Group != Badges[Idx - 1].Group)
				{
					Offset += 6;
				}
				Badge.Offset = Offset;

				Size BadgeSize = GetBadgeSize(Badge.Label);
				Badge.Width = BadgeSize.Width;
				Badge.Height = BadgeSize.Height;

				Offset += BadgeSize.Width + 1;
			}
		}

		private Point GetBadgeListLocation(List<BadgeInfo> Badges, Rectangle Bounds, HorizontalAlign HorzAlign, VerticalAlignment VertAlign)
		{
			Point Location = Bounds.Location;
			if(Badges.Count == 0)
			{
				return Bounds.Location;
			}

			BadgeInfo LastBadge = Badges[Badges.Count - 1];

			int X = Bounds.X;
			switch(HorzAlign)
			{
				case HorizontalAlign.Center:
					X += (Bounds.Width - LastBadge.Width - LastBadge.Offset) / 2;
					break;
				case HorizontalAlign.Right:
					X += (Bounds.Width - LastBadge.Width - LastBadge.Offset);
					break;
			}

			int Y = Bounds.Y;
			switch(VertAlign)
			{
				case VerticalAlignment.Middle:
					Y += (Bounds.Height - LastBadge.Height) / 2;
					break;
				case VerticalAlignment.Bottom:
					Y += (Bounds.Height - LastBadge.Height);
					break;
			}

			return new Point(X, Y);
		}

		private void DrawBadges(Graphics Graphics, Point ListLocation, List<BadgeInfo> Badges, int Alpha)
		{
			for(int Idx = 0; Idx < Badges.Count; Idx++)
			{
				BadgeInfo Badge = Badges[Idx];
				bool bMergeLeft = (Idx > 0 && Badges[Idx - 1].Group == Badge.Group);
				bool bMergeRight = (Idx < Badges.Count - 1 && Badges[Idx + 1].Group == Badge.Group);

				Rectangle Bounds = new Rectangle(ListLocation.X + Badge.Offset, ListLocation.Y, Badge.Width, Badge.Height);
				if(Badge.UniqueId != null && Badge.UniqueId == HoverBadgeUniqueId)
				{
					DrawBadge(Graphics, Bounds, Badge.Label, Color.FromArgb((Alpha * Badge.HoverBackgroundColor.A) / 255, Badge.HoverBackgroundColor), bMergeLeft, bMergeRight);
				}
				else
				{
					DrawBadge(Graphics, Bounds, Badge.Label, Color.FromArgb((Alpha * Badge.BackgroundColor.A) / 255, Badge.BackgroundColor), bMergeLeft, bMergeRight);
				}
			}
		}

		private List<BadgeInfo> CreateDescriptionBadges(PerforceChangeSummary Change)
		{
			string Description = Change.Description;

			PerforceChangeDetails Details;
			if(PerforceMonitor.TryGetChangeDetails(Change.Number, out Details))
			{
				Description = Details.Description;
			}

			List<BadgeInfo> Badges = new List<BadgeInfo>();

			try
			{
				ConfigFile ProjectConfigFile = PerforceMonitor.LatestProjectConfigFile;
				if(ProjectConfigFile != null)
				{
					string[] BadgeDefinitions = ProjectConfigFile.GetValues("Badges.DescriptionBadges", new string[0]);
					foreach(string BadgeDefinition in BadgeDefinitions)
					{
						ConfigObject BadgeDefinitionObject = new ConfigObject(BadgeDefinition);
						string Pattern = BadgeDefinitionObject.GetValue("Pattern", null);
						string Name = BadgeDefinitionObject.GetValue("Name", null);
						string Group = BadgeDefinitionObject.GetValue("Group", null);
						string Color = BadgeDefinitionObject.GetValue("Color", "#909090");
						string HoverColor = BadgeDefinitionObject.GetValue("HoverColor", "#b0b0b0");
						string Url = BadgeDefinitionObject.GetValue("Url", null);
						string Arguments = BadgeDefinitionObject.GetValue("Arguments", null);
						if(!String.IsNullOrEmpty(Name) && !String.IsNullOrEmpty(Pattern))
						{
							foreach(Match MatchResult in Regex.Matches(Description, Pattern, RegexOptions.Multiline))
							{
								Color BadgeColor = System.Drawing.ColorTranslator.FromHtml(Color);
								Color HoverBadgeColor = System.Drawing.ColorTranslator.FromHtml(HoverColor);

								string UniqueId = String.IsNullOrEmpty(Url)? null : String.Format("Description:{0}:{1}", Change.Number, Badges.Count);

								string ExpandedUrl = ReplaceRegexMatches(Url, MatchResult);
								string ExpandedArguments = ReplaceRegexMatches(Arguments, MatchResult);

								Action ClickHandler;
								if(String.IsNullOrEmpty(ExpandedUrl))
								{
									ClickHandler = null;
								}
								else if(String.IsNullOrEmpty(ExpandedArguments))
								{
									ClickHandler = () => Process.Start(ExpandedUrl);
								}
								else
								{
									ClickHandler = () => Process.Start(ExpandedUrl, ExpandedArguments);
								}

								Badges.Add(new BadgeInfo(ReplaceRegexMatches(Name, MatchResult), Group, UniqueId, BadgeColor, HoverBadgeColor, ClickHandler));
							}
						}
					}
				}
			}
			catch(Exception)
			{
			}

			LayoutBadges(Badges);

			return Badges;
		}

		private string ReplaceRegexMatches(string Text, Match MatchResult)
		{
			if(Text != null)
			{
				for(int Idx = 1; Idx < MatchResult.Groups.Count; Idx++)
				{
					string SourceText = String.Format("${0}", Idx);
					Text = Text.Replace(SourceText, MatchResult.Groups[Idx].Value);
				}
			}
			return Text;
		}

		private List<BadgeInfo> CreateTypeBadges(int ChangeNumber)
		{
			List<BadgeInfo> Badges = new List<BadgeInfo>();

			PerforceChangeDetails Details;
			if(PerforceMonitor.TryGetChangeDetails(ChangeNumber, out Details))
			{
				if(Details.bContainsCode)
				{
					Badges.Add(new BadgeInfo("Code", "ChangeType", Color.FromArgb(116, 185, 255)));
				}
				if(Details.bContainsContent)
				{
					Badges.Add(new BadgeInfo("Content", "ChangeType", Color.FromArgb(162, 155, 255)));
				}
			}
			if(Badges.Count == 0)
			{
				Badges.Add(new BadgeInfo("Unknown", "ChangeType", Color.FromArgb(192, 192, 192)));
			}
			LayoutBadges(Badges);

			return Badges;
		}

		private bool TryGetProjectSetting(ConfigFile ProjectConfigFile, string Name, out string Value)
		{
			ConfigSection ProjectSection = ProjectConfigFile.FindSection(SelectedProjectIdentifier);
			if(ProjectSection != null)
			{
				string NewValue = ProjectSection.GetValue(Name, null);
				if(NewValue != null)
				{
					Value = NewValue;
					return true;
				}
			}

			ConfigSection DefaultSection = ProjectConfigFile.FindSection("Default");
			if(DefaultSection != null)
			{
				string NewValue = DefaultSection.GetValue(Name, null);
				if(NewValue != null)
				{
					Value = NewValue;
					return true;
				}
			}

			Value = null;
			return false;
		}

		private bool TryGetProjectSetting(ConfigFile ProjectConfigFile, string Name, out string[] Values)
		{
			string ValueList;
			if(TryGetProjectSetting(ProjectConfigFile, Name, out ValueList))
			{
				Values = ValueList.Split('\n').Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();
				return true;
			}
			else
			{
				Values = null;
				return false;
			}
		}

		private bool TryGetProjectSetting(ConfigFile ProjectConfigFile, string Name, string LegacyName, out string Value)
		{
			string NewValue;
			if(TryGetProjectSetting(ProjectConfigFile, Name, out NewValue))
			{
				Value = NewValue;
				return true;
			}

			NewValue = ProjectConfigFile.GetValue(LegacyName, null);
			if(NewValue != null)
			{
				Value = NewValue;
				return true;
			}

			Value = null;
			return false;
		}

		private List<BadgeInfo> CreateBuildBadges(int ChangeNumber, EventSummary Summary)
		{
			List<BadgeInfo> Badges = new List<BadgeInfo>();

			if(Summary != null && Summary.Badges.Count > 0)
			{
				// Create a lookup for build data for each badge name
				Dictionary<string, BadgeData> BadgeNameToBuildData = new Dictionary<string, BadgeData>();
				foreach(BadgeData Badge in Summary.Badges)
				{
					BadgeNameToBuildData[Badge.BadgeName] = Badge;
				}

				// Add all the badges, sorted by group
				foreach(KeyValuePair<string, string> BadgeNameAndGroup in BadgeNameAndGroupPairs)
				{
					BadgeData BadgeData;
					BadgeNameToBuildData.TryGetValue(BadgeNameAndGroup.Key, out BadgeData);

					BadgeInfo BadgeInfo = CreateBadge(ChangeNumber, BadgeNameAndGroup.Key, BadgeNameAndGroup.Value, BadgeData);
					if(MaxBuildBadgeChars != -1 && BadgeInfo.Label.Length > MaxBuildBadgeChars)
					{
						BadgeInfo.ToolTip = BadgeInfo.Label;
						BadgeInfo.Label = BadgeInfo.Label.Substring(0, MaxBuildBadgeChars);
					}
					Badges.Add(BadgeInfo);
				}
			}

			// Layout the badges
			LayoutBadges(Badges);
			return Badges;
		}

		private BadgeInfo CreateBadge(int ChangeNumber, string BadgeName, string BadgeGroup, BadgeData BadgeData)
		{
			string BadgeLabel = BadgeName;
			Color BadgeColor = Color.FromArgb(0, Color.White);

			if(BadgeData != null)
			{
				BadgeLabel = BadgeData.BadgeLabel;
				BadgeColor = GetBuildBadgeColor(BadgeData.Result);
			}

			Color HoverBadgeColor = Color.FromArgb(BadgeColor.A, Math.Min(BadgeColor.R + 32, 255), Math.Min(BadgeColor.G + 32, 255), Math.Min(BadgeColor.B + 32, 255));

			Action ClickHandler;
			if(BadgeData == null || String.IsNullOrEmpty(BadgeData.Url))
			{
				ClickHandler = null;
			}
			else
			{
				ClickHandler = () => Process.Start(BadgeData.Url);
			}

			string UniqueId = String.Format("{0}:{1}", ChangeNumber, BadgeName);
			return new BadgeInfo(BadgeLabel, BadgeGroup, UniqueId, BadgeColor, HoverBadgeColor, ClickHandler);
		}

		private Dictionary<string, List<BadgeInfo>> CreateCustomBadges(int ChangeNumber, EventSummary Summary)
		{
			Dictionary<string, List<BadgeInfo>> ColumnNameToBadges = new Dictionary<string, List<BadgeInfo>>();
			if(Summary != null && Summary.Badges.Count > 0)
			{
				foreach(ColumnHeader CustomColumn in CustomColumns)
				{
					ConfigObject Config = (ConfigObject)CustomColumn.Tag;
					if(Config != null)
					{
						List<BadgeInfo> Badges = new List<BadgeInfo>();

						string[] BadgeNames = Config.GetValue("Badges", "").Split(new char[]{ ',' }, StringSplitOptions.RemoveEmptyEntries);
						foreach(string BadgeName in BadgeNames)
						{
							BadgeInfo Badge = CreateBadge(ChangeNumber, BadgeName, "XXXX", Summary.Badges.FirstOrDefault(x => x.BadgeName == BadgeName));
							Badges.Add(Badge);
						}

						LayoutBadges(Badges);

						ColumnNameToBadges[CustomColumn.Text] = Badges;
					}
				}
			}
			return ColumnNameToBadges;
		}

		private static Color GetBuildBadgeColor(BadgeResult Result)
		{
			if(Result == BadgeResult.Starting)
			{
				return Color.FromArgb(128, 192, 255);
			}
			else if(Result == BadgeResult.Warning)
			{
				return Color.FromArgb(255, 192, 0);
			}
			else if(Result == BadgeResult.Failure)
			{
				return Color.FromArgb(192, 64, 0);
			}
			else if(Result == BadgeResult.Skipped)
			{
				return Color.FromArgb(192, 192, 192);
			}
			else
			{
				return Color.FromArgb(128, 192, 64);
			}
		}

		private Rectangle GetSyncBadgeRectangle(Rectangle Bounds)
		{
			Size BadgeSize = GetBadgeSize("Sync");
			return new Rectangle(Bounds.Right - BadgeSize.Width - 2, Bounds.Top + (Bounds.Height - BadgeSize.Height) / 2, BadgeSize.Width, BadgeSize.Height);
		}

		private void DrawSingleBadge(Graphics Graphics, Rectangle DisplayRectangle, string BadgeText, Color BadgeColor)
		{
			Size BadgeSize = GetBadgeSize(BadgeText);

			int X = DisplayRectangle.Left + (DisplayRectangle.Width - BadgeSize.Width) / 2;
			int Y = DisplayRectangle.Top + (DisplayRectangle.Height - BadgeSize.Height) / 2;

			DrawBadge(Graphics, new Rectangle(X, Y, BadgeSize.Width, BadgeSize.Height), BadgeText, BadgeColor, false, false);
		}

		private void DrawBadge(Graphics Graphics, Rectangle BadgeRect, string BadgeText, Color BadgeColor, bool bMergeLeft, bool bMergeRight)
		{
			if(BadgeColor.A != 0)
			{
				using (GraphicsPath Path = new GraphicsPath())
				{
					Path.StartFigure();
					Path.AddLine(BadgeRect.Left + (bMergeLeft? 1 : 0), BadgeRect.Top, BadgeRect.Left - (bMergeLeft? 1 : 0), BadgeRect.Bottom);
					Path.AddLine(BadgeRect.Left - (bMergeLeft? 1 : 0), BadgeRect.Bottom, BadgeRect.Right - 1 - (bMergeRight? 1 : 0), BadgeRect.Bottom);
					Path.AddLine(BadgeRect.Right - 1 - (bMergeRight? 1 : 0), BadgeRect.Bottom, BadgeRect.Right - 1 + (bMergeRight? 1 : 0), BadgeRect.Top);
					Path.AddLine(BadgeRect.Right - 1 + (bMergeRight? 1 : 0), BadgeRect.Top, BadgeRect.Left + (bMergeLeft? 1 : 0), BadgeRect.Top);
					Path.CloseFigure();

					using(SolidBrush Brush = new SolidBrush(BadgeColor))
					{
						Graphics.FillPath(Brush, Path);
					}
				}

				TextRenderer.DrawText(Graphics, BadgeText, BadgeFont, BadgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
			}
		}

		private Size GetBadgeSize(string BadgeText)
		{
			Size BadgeSize;
			if(!BadgeLabelToSize.TryGetValue(BadgeText, out BadgeSize))
			{
				Size LabelSize = TextRenderer.MeasureText(BadgeText, BadgeFont);
				int BadgeHeight = BadgeFont.Height + 1;

				BadgeSize = new Size(LabelSize.Width + BadgeHeight - 4, BadgeHeight);
				BadgeLabelToSize[BadgeText] = BadgeSize;
			}
			return BadgeSize;
		}

		private static bool GetLastUpdateMessage(WorkspaceUpdateResult Result, string ResultMessage, out string Message)
		{
			if(Result != WorkspaceUpdateResult.Success && ResultMessage != null)
			{
				Message = ResultMessage;
				return true;
			}
			return GetGenericLastUpdateMessage(Result, out Message);
		}

		private static bool GetGenericLastUpdateMessage(WorkspaceUpdateResult Result, out string Message)
		{
			switch(Result)
			{
				case WorkspaceUpdateResult.Canceled:
					Message = "Sync canceled.";
					return true;
				case WorkspaceUpdateResult.FailedToSync:
					Message = "Failed to sync files.";
					return true;
				case WorkspaceUpdateResult.FailedToSyncLoginExpired:
					Message = "Failed to sync files (login expired).";
					return true;
				case WorkspaceUpdateResult.FilesToResolve:
					Message = "Sync finished with unresolved files.";
					return true;
				case WorkspaceUpdateResult.FilesToClobber:
					Message = "Sync failed due to modified files in workspace.";
					return true;
				case WorkspaceUpdateResult.FilesToDelete:
					Message = "Sync aborted pending confirmation of files to delete.";
					return true;
				case WorkspaceUpdateResult.FailedToCompile:
				case WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace:
					Message = "Compile failed.";
					return true;
				default:
					Message = null;
					return false;
			}
		}

		private static string FormatUserName(string UserName)
		{
			StringBuilder NormalUserName = new StringBuilder();
			for(int Idx = 0; Idx < UserName.Length; Idx++)
			{
				if(Idx == 0 || UserName[Idx - 1] == '.')
				{
					NormalUserName.Append(Char.ToUpper(UserName[Idx]));
				}
				else if(UserName[Idx] == '.')
				{
					NormalUserName.Append(' ');
				}
				else
				{
					NormalUserName.Append(UserName[Idx]);
				}
			}
			return NormalUserName.ToString();
		}

		private static void AppendUserList(StringBuilder Builder, string Separator, string Format, IEnumerable<EventData> Reviews)
		{
			AppendItemList(Builder, Separator, Format, Reviews.Select(x => FormatUserName(x.UserName)));
		}

		private static void AppendItemList(StringBuilder Builder, string Separator, string Format, IEnumerable<string> Items)
		{
			string ItemList = FormatItemList(Format, Items);
			if(ItemList != null)
			{
				if(Builder.Length > 0)
				{
					Builder.Append(Separator);
				}
				Builder.Append(ItemList);
			}
		}

		private static string FormatItemList(string Format, IEnumerable<string> Items)
		{
			string[] ItemsArray = Items.Distinct().ToArray();
			if(ItemsArray.Length == 0)
			{
				return null;
			}

			StringBuilder Builder = new StringBuilder(ItemsArray[0]);
			if(ItemsArray.Length > 1)
			{
				for(int Idx = 1; Idx < ItemsArray.Length - 1; Idx++)
				{
					Builder.Append(", ");
					Builder.Append(ItemsArray[Idx]);
				}
				Builder.Append(" and ");
				Builder.Append(ItemsArray.Last());
			}
			return String.Format(Format, Builder.ToString());
		}

		private void BuildList_MouseDoubleClick(object Sender, MouseEventArgs Args)
		{
			if(Args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null)
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)HitTest.Item.Tag;
					if(Change != null)
					{
						if(Change.Number == Workspace.CurrentChangeNumber)
						{
							LaunchEditor();
						}
						else
						{
							StartSync(Change.Number, null);
						}
					}
				}
			}
		}

		public void LaunchEditor()
		{
			if(!Workspace.IsBusy() && Workspace.CurrentChangeNumber != -1)
			{
				BuildConfig EditorBuildConfig = GetEditorBuildConfig();

				List<string> ReceiptPaths = GetEditorReceiptPaths(EditorBuildConfig);

				string EditorExe = GetEditorExePath(EditorBuildConfig);
				if(ReceiptPaths.Any(x => File.Exists(x)) && File.Exists(EditorExe))
				{
					if(Settings.bEditorArgumentsPrompt && !ModifyEditorArguments())
					{
						return;
					}

					StringBuilder LaunchArguments = new StringBuilder();
					if(SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
					{
						LaunchArguments.AppendFormat("\"{0}\"", SelectedFileName);
					}
					foreach(Tuple<string, bool> EditorArgument in Settings.EditorArguments)
					{
						if(EditorArgument.Item2)
						{
							LaunchArguments.AppendFormat(" {0}", EditorArgument.Item1);
						}
					}
					if(EditorBuildConfig == BuildConfig.Debug || EditorBuildConfig == BuildConfig.DebugGame)
					{
						LaunchArguments.Append(" -debug");
					}
					
					if(!Utility.SpawnProcess(EditorExe, LaunchArguments.ToString()))
					{
						ShowErrorDialog("Unable to spawn {0} {1}", EditorExe, LaunchArguments.ToString());
					}
				}
				else
				{
					if(MessageBox.Show("The editor needs to be built before you can run it. Build it now?", "Editor out of date", MessageBoxButtons.YesNo) == System.Windows.Forms.DialogResult.Yes)
					{
						Owner.ShowAndActivate();

						WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.RunAfterSync;
						if(Settings.bUseIncrementalBuilds)
						{
							Options |= WorkspaceUpdateOptions.UseIncrementalBuilds;
						}
		
						WorkspaceUpdateContext Context = new WorkspaceUpdateContext(Workspace.CurrentChangeNumber, Options, null, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, null, GetWorkspaceVariables(Workspace.CurrentChangeNumber));
						StartWorkspaceUpdate(Context, null);
					}
				}
			}
		}

		private string GetEditorExePath(BuildConfig Config)
		{
			// Try to read the executable path from the target receipt
			List<string> ReceiptFileNames = GetEditorReceiptPaths(Config);
			foreach(string ReceiptFileName in ReceiptFileNames)
			{
				if(File.Exists(ReceiptFileName))
				{
					Log.WriteLine("Reading {0}", ReceiptFileName);
					try
					{
						string Text = File.ReadAllText(ReceiptFileName);
						JavaScriptSerializer Serializer = new JavaScriptSerializer();
						Dictionary<string, object> RawObject = Serializer.Deserialize<Dictionary<string, object>>(Text);

						object LaunchFileNameObject;
						if(RawObject.TryGetValue("Launch", out LaunchFileNameObject))
						{
							string LaunchFileName = LaunchFileNameObject as string;
							if(LaunchFileName != null)
							{
								LaunchFileName = LaunchFileName.Replace("$(EngineDir)", Path.Combine(BranchDirectoryName, "Engine"));
								LaunchFileName = LaunchFileName.Replace("$(ProjectDir)", Path.GetDirectoryName(SelectedFileName));
								return Path.GetFullPath(LaunchFileName);
							}
						}
					}
					catch(Exception Ex)
					{
						Log.WriteLine("  Exception while parsing receipt: {0}", Ex.ToString());
					}
					break;
				}
			}

			// Otherwise use the standard editor path
			string ExeFileName = "UE4Editor.exe";
			if((Config != BuildConfig.DebugGame || PerforceMonitor.LatestProjectConfigFile.GetValue("Options.DebugGameHasSeparateExecutable", false)) && Config != BuildConfig.Development)
			{
				ExeFileName	= String.Format("UE4Editor-Win64-{0}.exe", Config.ToString());
			}
			return Path.Combine(BranchDirectoryName, "Engine", "Binaries", "Win64", ExeFileName);
		}

		private bool WaitForProgramsToFinish()
		{
			string[] ProcessFileNames = GetProcessesRunningInWorkspace();
			if(ProcessFileNames.Length > 0)
			{
				ProgramsRunningWindow ProgramsRunning = new ProgramsRunningWindow(GetProcessesRunningInWorkspace, ProcessFileNames);
				if(ProgramsRunning.ShowDialog() != DialogResult.OK)
				{
					return false;
				}
			}
			return true;
		}

		private string[] GetProcessesRunningInWorkspace()
		{
			HashSet<string> ProcessNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
			ProcessNames.Add("UE4Editor");
			ProcessNames.Add("UE4Editor-Cmd");
			ProcessNames.Add("UE4Editor-Win64-Debug");
			ProcessNames.Add("UE4Editor-Win64-Debug-Cmd");
			ProcessNames.Add("CrashReportClient");
			ProcessNames.Add("CrashReportClient-Win64-Development");
			ProcessNames.Add("CrashReportClient-Win64-Debug");
			ProcessNames.Add("UnrealBuildTool");
			ProcessNames.Add("AutomationTool");

			List<string> ProcessFileNames = new List<string>();
			try
			{
				string RootDirectoryName = Path.GetFullPath(Workspace.LocalRootPath) + Path.DirectorySeparatorChar;
				foreach(Process ProcessInstance in Process.GetProcesses())
				{
					try
					{
						if(ProcessNames.Contains(ProcessInstance.ProcessName))
						{
							string ProcessFileName = Path.GetFullPath(ProcessInstance.MainModule.FileName);
							if(ProcessFileName.StartsWith(RootDirectoryName, StringComparison.InvariantCultureIgnoreCase))
							{
								string DisplayTitle = ProcessFileName;
								try
								{
									string MainWindowTitle = ProcessInstance.MainWindowTitle;
									if(!String.IsNullOrEmpty(MainWindowTitle))
									{
										DisplayTitle = String.Format("{0} ({1})", ProcessInstance.MainWindowTitle, DisplayTitle);
									}
								}
								catch
								{
								}
								ProcessFileNames.Add(DisplayTitle);
							}
						}
					}
					catch
					{
					}
				}
			}
			catch
			{
			}
			return ProcessFileNames.ToArray();
		}

		private List<string> GetEditorReceiptPaths(BuildConfig Config)
		{
			string ConfigSuffix = (Config == BuildConfig.Development)? "" : String.Format("-Win64-{0}", Config.ToString());

			List<string> PossiblePaths = new List<string>();
			if(EditorTargetName != null)
			{
				PossiblePaths.Add(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Binaries", "Win64", String.Format("{0}{1}.target", EditorTargetName, ConfigSuffix)));
				PossiblePaths.Add(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Build", "Receipts", String.Format("{0}-Win64-{1}.target.xml", EditorTargetName, Config.ToString())));
			}
			else if(SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase) && bIsEnterpriseProject)
			{
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Enterprise", "Binaries", "Win64", String.Format("StudioEditor{0}.target", ConfigSuffix)));
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Enterprise", "Build", "Receipts", String.Format("StudioEditor-Win64-{0}.target.xml", Config.ToString())));
			}
			else
			{
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Engine", "Binaries", "Win64", String.Format("UE4Editor{0}.target", ConfigSuffix)));
				PossiblePaths.Add(Path.Combine(BranchDirectoryName, "Engine", "Build", "Receipts", String.Format("UE4Editor-Win64-{0}.target.xml", Config.ToString())));
			}
			return PossiblePaths;
		}

		private void TimerCallback(object Sender, EventArgs Args)
		{
			Tuple<string, float> Progress = Workspace.CurrentProgress;
			if(Progress != null && Progress.Item2 > 0.0f)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Normal, Progress.Item2);
			}
			else
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
			}

			Owner.UpdateProgress();

			UpdateStatusPanel();
			BuildList.Refresh();
		}

		private void OpenPerforce()
		{
			StringBuilder CommandLine = new StringBuilder();
			if(Workspace != null && Workspace.Perforce != null)
			{
				CommandLine.AppendFormat("-p \"{0}\" -c \"{1}\" -u \"{2}\"", Workspace.Perforce.ServerAndPort ?? "perforce:1666", Workspace.Perforce.ClientName, Workspace.Perforce.UserName);
			}
			Process.Start("p4v.exe", CommandLine.ToString());
		}

		private void UpdateStatusPanel()
		{
			int NewContentWidth = Math.Max(TextRenderer.MeasureText(String.Format("Opened {0}  |  Browse...  |  Connect...", SelectedFileName), StatusPanel.Font).Width, 400);
			StatusPanel.SetContentWidth(NewContentWidth);

			List<StatusLine> Lines = new List<StatusLine>();
			if(Workspace.IsBusy())
			{
				// Sync in progress
				Tuple<string, float> Progress = Workspace.CurrentProgress;

				StatusLine SummaryLine = new StatusLine();
				if(Workspace.PendingChangeNumber == -1)
				{
					SummaryLine.AddText("Working... | ");
				}
				else
				{
					SummaryLine.AddText("Syncing to changelist ");
					SummaryLine.AddLink(Workspace.PendingChangeNumber.ToString(), FontStyle.Regular, () => { SelectChange(Workspace.PendingChangeNumber); });
					SummaryLine.AddText("... | ");
				}
				SummaryLine.AddLink(Splitter.IsLogVisible()? "Hide Log" : "Show Log", FontStyle.Bold | FontStyle.Underline, () => { ToggleLogVisibility(); });
				SummaryLine.AddText(" | ");
				SummaryLine.AddLink("Cancel", FontStyle.Bold | FontStyle.Underline, () => { CancelWorkspaceUpdate(); });
				Lines.Add(SummaryLine);

				StatusLine ProgressLine = new StatusLine();
				ProgressLine.AddText(String.Format("{0}  ", Progress.Item1));
				if(Progress.Item2 > 0.0f) ProgressLine.AddProgressBar(Progress.Item2);
				Lines.Add(ProgressLine);
			}
			else
			{
				// Project
				StatusLine ProjectLine = new StatusLine();
				ProjectLine.AddText(String.Format("Opened "));
				ProjectLine.AddLink(SelectedFileName + " \u25BE", FontStyle.Regular, (P, R) => { SelectRecentProject(R); });
				ProjectLine.AddText("  |  ");
				ProjectLine.AddLink("Browse...", FontStyle.Regular, (P, R) => { Owner.EditSelectedProject(this); });
				Lines.Add(ProjectLine);

				// Spacer
				Lines.Add(new StatusLine(){ LineHeight = 0.5f });

				// Sync status
				StatusLine SummaryLine = new StatusLine();
				if(IsBisectModeEnabled())
				{
					int PassChangeNumber;
					int FailChangeNumber;
					GetRemainingBisectRange(out PassChangeNumber, out FailChangeNumber);
					
					SummaryLine.AddText("Bisecting changes between ");
					SummaryLine.AddLink(String.Format("{0}", PassChangeNumber), FontStyle.Regular, () => { SelectChange(PassChangeNumber); });
					SummaryLine.AddText(" and ");
					SummaryLine.AddLink(String.Format("{0}", FailChangeNumber), FontStyle.Regular, () => { SelectChange(FailChangeNumber); });
					SummaryLine.AddText(".  ");

					int BisectChangeNumber = GetBisectChangeNumber();
					if(BisectChangeNumber != -1)
					{
						SummaryLine.AddLink("Sync Next", FontStyle.Bold | FontStyle.Underline, () => { SyncBisectChange(); });
						SummaryLine.AddText(" | ");
					}

					SummaryLine.AddLink("Cancel", FontStyle.Bold | FontStyle.Underline, () => { CancelBisectMode(); });
				}
				else
				{
					if(Workspace.CurrentChangeNumber != -1)
					{
						if(StreamName == null)
						{
							SummaryLine.AddText("Last synced to changelist ");
						}
						else
						{
							SummaryLine.AddText("Last synced to ");
							if(Workspace.ProjectStreamFilter == null || Workspace.ProjectStreamFilter.Count == 0)
							{
								SummaryLine.AddLink(StreamName, FontStyle.Regular, (P, R) => { SelectOtherStreamDialog(); });
							}
							else
							{
								SummaryLine.AddLink(StreamName + "\u25BE", FontStyle.Regular, (P, R) => { SelectOtherStream(R); });
							}
							SummaryLine.AddText(" at changelist ");
						}
						SummaryLine.AddLink(String.Format("{0}.", Workspace.CurrentChangeNumber), FontStyle.Regular, () => { SelectChange(Workspace.CurrentChangeNumber); });
					}
					else
					{
						SummaryLine.AddText("You are not currently synced to ");
						if(StreamName == null)
						{
							SummaryLine.AddText("this branch.");
						}
						else
						{
							SummaryLine.AddLink(StreamName + " \u25BE", FontStyle.Regular, (P, R) => { SelectOtherStream(R); });
						}
					}
					SummaryLine.AddText("  |  ");
					SummaryLine.AddLink("Sync Now", FontStyle.Bold | FontStyle.Underline, () => { SyncLatestChange(); });
					SummaryLine.AddText(" - ");
					SummaryLine.AddLink(" To... \u25BE", 0, (P, R) => { ShowSyncMenu(R); });
				}
				Lines.Add(SummaryLine);

				// Programs
				StatusLine ProgramsLine = new StatusLine();
				if(Workspace.CurrentChangeNumber != -1)
				{
					ProgramsLine.AddLink("Unreal Editor", FontStyle.Regular, () => { LaunchEditor(); });
					ProgramsLine.AddText("  |  ");
				}

				string[] SdkInfoEntries;
				if(TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "SdkInfo", out SdkInfoEntries))
				{
					ProgramsLine.AddLink("SDK Info", FontStyle.Regular, () => { ShowRequiredSdkInfo(); });
					ProgramsLine.AddText("  |  ");
				}

				ProgramsLine.AddLink("Perforce", FontStyle.Regular, () => { OpenPerforce(); });
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("Visual Studio", FontStyle.Regular, () => { OpenSolution(); });
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("Windows Explorer", FontStyle.Regular, () => { Process.Start("explorer.exe", String.Format("\"{0}\"", Path.GetDirectoryName(SelectedFileName))); });

				foreach(KeyValuePair<string, BadgeData> ServiceBadge in ServiceBadges)
				{
					ProgramsLine.AddText("  |  ");
					if(ServiceBadge.Value == null)
					{
						ProgramsLine.AddBadge(ServiceBadge.Key, GetBuildBadgeColor(BadgeResult.Skipped), null);
					}
					else
					{
						ProgramsLine.AddBadge(ServiceBadge.Key, GetBuildBadgeColor(ServiceBadge.Value.Result), () => { Process.Start(ServiceBadge.Value.Url); });
					}
				}
				ProgramsLine.AddText("  |  ");
				ProgramsLine.AddLink("More... \u25BE", FontStyle.Regular, (P, R) => { ShowActionsMenu(R); });
				Lines.Add(ProgramsLine);

				// Get the summary of the last sync
				if(WorkspaceSettings.LastSyncChangeNumber > 0)
				{
					string SummaryText;
					if(WorkspaceSettings.LastSyncChangeNumber == Workspace.CurrentChangeNumber && WorkspaceSettings.LastSyncResult == WorkspaceUpdateResult.Success && WorkspaceSettings.LastSyncTime.HasValue)
					{
						Lines.Add(new StatusLine(){ LineHeight = 0.5f });

						StatusLine SuccessLine = new StatusLine();
						SuccessLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 0);
						SuccessLine.AddText(String.Format("  Sync took {0}{1}s, completed at {2}.", (WorkspaceSettings.LastSyncDurationSeconds >= 60)? String.Format("{0}m ", WorkspaceSettings.LastSyncDurationSeconds / 60) : "", WorkspaceSettings.LastSyncDurationSeconds % 60, WorkspaceSettings.LastSyncTime.Value.ToLocalTime().ToString("h\\:mmtt").ToLowerInvariant()));
						Lines.Add(SuccessLine);
					}
					else if(GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
					{
						Lines.Add(new StatusLine(){ LineHeight = 0.5f });

						int SummaryTextLength = SummaryText.IndexOf('\n');
						if(SummaryTextLength == -1)
						{
							SummaryTextLength = SummaryText.Length;
						}
						SummaryTextLength = Math.Min(SummaryTextLength, 80);

						StatusLine FailLine = new StatusLine();
						FailLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 1);

						if(SummaryTextLength == SummaryText.Length)
						{
							FailLine.AddText(String.Format("  {0}  ", SummaryText));
						}
						else 
						{
							FailLine.AddText(String.Format("  {0}...  ", SummaryText.Substring(0, SummaryTextLength).TrimEnd()));
							FailLine.AddLink("More...", FontStyle.Bold | FontStyle.Underline, () => { ViewLastSyncStatus(); });
							FailLine.AddText("  |  ");
						}
						FailLine.AddLink("Show Log", FontStyle.Bold | FontStyle.Underline, () => { ShowErrorInLog(); });
						Lines.Add(FailLine);
					}
				}
			}

			StatusLine Caption = null;
			if(StreamName != null && !Workspace.IsBusy())
			{
				Caption = new StatusLine();
				Caption.AddLink(StreamName + "\u25BE", FontStyle.Bold, (P, R) => { SelectOtherStream(R); });
			}

			StatusLine Alert = null;
			Color? TintColor = null;

			ConfigFile ProjectConfigFile = PerforceMonitor.LatestProjectConfigFile;
			if(ProjectConfigFile != null)
			{
				string Message;
				if(TryGetProjectSetting(ProjectConfigFile, "Message", out Message))
				{
					Alert = CreateStatusLineFromMarkdown(Message);
				}

				string StatusPanelColor;
				if(TryGetProjectSetting(ProjectConfigFile, "StatusPanelColor", out StatusPanelColor))
				{
					TintColor = System.Drawing.ColorTranslator.FromHtml(StatusPanelColor);
				}
			}

			using(Graphics Graphics = CreateGraphics())
			{
				int StatusPanelHeight = (int)(148.0f * Graphics.DpiY / 96.0f);
				if(Alert != null)
				{
					StatusPanelHeight += (int)(40.0f * Graphics.DpiY / 96.0f);
				}
				if(StatusLayoutPanel.RowStyles[0].Height != StatusPanelHeight)
				{
					StatusLayoutPanel.RowStyles[0].Height = StatusPanelHeight;
				}
			}

			StatusPanel.Set(Lines, Caption, Alert, TintColor);
		}

		private void ShowRequiredSdkInfo()
		{
			string[] SdkInfoEntries;
			if(TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "SdkInfo", out SdkInfoEntries))
			{
				Dictionary<string, string> Variables = GetWorkspaceVariables(-1);
				SdkInfoWindow Window = new SdkInfoWindow(SdkInfoEntries, Variables, BadgeFont);
				Window.ShowDialog();
			}
		}

		private StatusLine CreateStatusLineFromMarkdown(string Text)
		{
			StatusLine Line = new StatusLine();

			FontStyle Style = FontStyle.Regular;

			StringBuilder ElementText = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; )
			{
				// Bold and italic
				if(Text[Idx] == '_' || Text[Idx] == '*')
				{
					if(Idx + 1 < Text.Length && Text[Idx + 1] == Text[Idx])
					{
						FlushMarkdownText(Line, ElementText, Style);
						Idx += 2;
						Style ^= FontStyle.Bold;
						continue;
					}
					else
					{
						FlushMarkdownText(Line, ElementText, Style);
						Idx++;
						Style ^= FontStyle.Italic;
						continue;
					}
				}

				// Strikethrough
				if(Idx + 2 < Text.Length && Text[Idx] == '~' && Text[Idx + 1] == '~')
				{
					FlushMarkdownText(Line, ElementText, Style);
					Idx += 2;
					Style ^= FontStyle.Strikeout;
					continue;
				}

				// Link
				if (Text[Idx] == '[')
				{
					if(Idx + 1 < Text.Length && Text[Idx + 1] == '[')
					{
						ElementText.Append(Text[Idx]);
						Idx += 2;
						continue;
					}

					int EndIdx = Text.IndexOf("](", Idx);
					if(EndIdx != -1)
					{
						int UrlEndIdx = Text.IndexOf(')', EndIdx + 2);
						if(UrlEndIdx != -1)
						{
							FlushMarkdownText(Line, ElementText, Style);
							string LinkText = Text.Substring(Idx + 1, EndIdx - (Idx + 1));
							string LinkUrl = Text.Substring(EndIdx + 2, UrlEndIdx - (EndIdx + 2));
							Line.AddLink(LinkText, Style, () => Process.Start(LinkUrl));
							Idx = UrlEndIdx + 1;
							continue;
						}
					}
				}

				// Icon
				if(Text[Idx] == ':')
				{
					int EndIdx = Text.IndexOf(':', Idx + 1);
					if(EndIdx != -1)
					{
						if(String.Compare(":alert:", 0, Text, Idx, EndIdx - Idx) == 0)
						{
							FlushMarkdownText(Line, ElementText, Style);
							Line.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 4);
							Idx = EndIdx + 1;
							continue;
						}
					}
				}

				// Otherwise, just append the current character
				ElementText.Append(Text[Idx++]);
			}
			FlushMarkdownText(Line, ElementText, Style);

			return Line;
		}

		private void FlushMarkdownText(StatusLine Line, StringBuilder ElementText, FontStyle Style)
		{
			if(ElementText.Length > 0)
			{
				Line.AddText(ElementText.ToString(), Style);
				ElementText.Clear();
			}
		}

		private void SelectOtherStream(Rectangle Bounds)
		{
			bool bShownContextMenu = false;
			if(StreamName != null)
			{
				IReadOnlyList<string> OtherStreamNames = Workspace.ProjectStreamFilter;
				if(OtherStreamNames != null)
				{
					StreamContextMenu.Items.Clear();

					ToolStripMenuItem CurrentStreamItem = new ToolStripMenuItem(StreamName, null, new EventHandler((S, E) => SelectStream(StreamName)));
					CurrentStreamItem.Checked = true;
					StreamContextMenu.Items.Add(CurrentStreamItem);
					
					StreamContextMenu.Items.Add(new ToolStripSeparator());

					foreach (string OtherStreamName in OtherStreamNames.OrderBy(x => x).Where(x => !x.EndsWith("/Dev-Binaries")))
					{
						string ThisStreamName = OtherStreamName; // Local for lambda capture
						if(String.Compare(StreamName, OtherStreamName, StringComparison.InvariantCultureIgnoreCase) != 0)
						{
							ToolStripMenuItem Item = new ToolStripMenuItem(ThisStreamName, null, new EventHandler((S, E) => SelectStream(ThisStreamName)));
							StreamContextMenu.Items.Add(Item);
						}
					}

					if(StreamContextMenu.Items.Count > 2)
					{
						StreamContextMenu.Items.Add(new ToolStripSeparator());
					}

					StreamContextMenu.Items.Add(new ToolStripMenuItem("Select Other...", null, new EventHandler((S, E) => SelectOtherStreamDialog())));

					int X = (Bounds.Left + Bounds.Right) / 2 + StreamContextMenu.Bounds.Width / 2;
					int Y = Bounds.Bottom + 2;
					StreamContextMenu.Show(StatusPanel, new Point(X, Y), ToolStripDropDownDirection.Left);

					bShownContextMenu = true;
				}
			}
			if(!bShownContextMenu)
			{
				SelectOtherStreamDialog();
			}
		}

		private void SelectOtherStreamDialog()
		{
			string NewStreamName;
			if(SelectStreamWindow.ShowModal(this, SelectedProject.ServerAndPort, SelectedProject.UserName, StreamName, Log, out NewStreamName))
			{
				SelectStream(NewStreamName);
			}
		}

		private void SelectStream(string NewStreamName)
		{
			if(StreamName != NewStreamName)
			{
				if(Workspace.IsBusy())
				{
					MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
				}
				else if(Workspace.Perforce != null)
				{
					if(!Workspace.Perforce.HasOpenFiles(Log) || MessageBox.Show("You have files open for edit in this workspace. If you continue, you will not be able to submit them until you switch back.\n\nContinue switching streams?", "Files checked out", MessageBoxButtons.YesNo) == DialogResult.Yes)
					{
						if(Workspace.Perforce.SwitchStream(NewStreamName, Log))
						{
							StatusPanel.SuspendLayout();
							StreamChanged();
							StatusPanel.ResumeLayout();
						}
					}
				}
			}
		}

		private void ViewLastSyncStatus()
		{
			string SummaryText;
			if(GetLastUpdateMessage(WorkspaceSettings.LastSyncResult, WorkspaceSettings.LastSyncResultMessage, out SummaryText))
			{
				string CaptionText;
				if(!GetGenericLastUpdateMessage(WorkspaceSettings.LastSyncResult, out CaptionText))
				{
					CaptionText = "Sync error";
				}
				MessageBox.Show(SummaryText, CaptionText);
			}
		}

		private void ShowErrorInLog()
		{
			if(!Splitter.IsLogVisible())
			{
				ToggleLogVisibility();
			}
			SyncLog.ScrollToEnd();
		}

		private void ShowActionsMenu(Rectangle Bounds)
		{
			MoreToolsContextMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SelectChange(int ChangeNumber)
		{
			BuildList.SelectedItems.Clear();

			PendingSelectedChangeNumber = -1;

			foreach(ListViewItem Item in BuildList.Items)
			{
				PerforceChangeSummary Summary = (PerforceChangeSummary)Item.Tag;
				if(Summary != null && Summary.Number <= ChangeNumber)
				{
					Item.Selected = true;
					Item.EnsureVisible();
					return;
				}
			}

			PendingSelectedChangeNumber = ChangeNumber;

			int CurrentMaxChanges = PerforceMonitor.CurrentMaxChanges;
			if(PerforceMonitor.PendingMaxChanges <= CurrentMaxChanges && BuildList.SelectedItems.Count == 0)
			{
				PerforceMonitor.PendingMaxChanges = CurrentMaxChanges + BuildListExpandCount;
				if(ExpandItem != null)
				{
					ExpandItem.EnsureVisible();
				}
			}
		}

		private void BuildList_MouseClick(object Sender, MouseEventArgs Args)
		{
			if(Args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null)
				{
					if(HitTest.Item == ExpandItem)
					{
						if(HitTestExpandLink(Args.Location))
						{
							int CurrentMaxChanges = PerforceMonitor.CurrentMaxChanges;
							if(PerforceMonitor.PendingMaxChanges > CurrentMaxChanges)
							{
								PerforceMonitor.PendingMaxChanges = CurrentMaxChanges;
							}
							else
							{
								PerforceMonitor.PendingMaxChanges = CurrentMaxChanges + BuildListExpandCount;
							}
							BuildList.Invalidate();
						}
					}
					else
					{
						PerforceChangeSummary Change = (PerforceChangeSummary)HitTest.Item.Tag;
						if(Workspace.PendingChangeNumber == Change.Number)
						{
							Rectangle SubItemRect = HitTest.Item.SubItems[StatusColumn.Index].Bounds;

							if(Workspace.IsBusy())
							{
								Rectangle CancelRect = new Rectangle(SubItemRect.Right - 16, SubItemRect.Top, 16, SubItemRect.Height);
								Rectangle InfoRect = new Rectangle(SubItemRect.Right - 32, SubItemRect.Top, 16, SubItemRect.Height);
								if(CancelRect.Contains(Args.Location))
								{
									CancelWorkspaceUpdate();
								}
								else if(InfoRect.Contains(Args.Location) && !Splitter.IsLogVisible())
								{
									ToggleLogVisibility();
								}
							}
							else
							{
								Rectangle HappyRect = new Rectangle(SubItemRect.Right - 32, SubItemRect.Top, 16, SubItemRect.Height);
								Rectangle FrownRect = new Rectangle(SubItemRect.Right - 16, SubItemRect.Top, 16, SubItemRect.Height);
								if(HappyRect.Contains(Args.Location))
								{
									EventMonitor.PostEvent(Change.Number, EventType.Good);
									BuildList.Invalidate();
								}
								else if(FrownRect.Contains(Args.Location))
								{
									EventMonitor.PostEvent(Change.Number, EventType.Bad);
									BuildList.Invalidate();
								}
							}
						}
						else
						{
							Rectangle SyncBadgeRectangle = GetSyncBadgeRectangle(HitTest.Item.SubItems[StatusColumn.Index].Bounds);
							if(SyncBadgeRectangle.Contains(Args.Location) && CanSyncChange(Change.Number))
							{
								StartSync(Change.Number, null);
							}
						}

						if(DescriptionColumn.Index < HitTest.Item.SubItems.Count && HitTest.Item.SubItems[DescriptionColumn.Index] == HitTest.SubItem)
						{
							ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo(Change);
							if(LayoutInfo.DescriptionBadges.Count > 0)
							{
								Point BuildListLocation = GetBadgeListLocation(LayoutInfo.DescriptionBadges, HitTest.SubItem.Bounds, HorizontalAlign.Right, VerticalAlignment.Middle);
								BuildListLocation.Offset(-2, 0);

								foreach (BadgeInfo Badge in LayoutInfo.DescriptionBadges)
								{
									Rectangle BadgeBounds = Badge.GetBounds(BuildListLocation);
									if(BadgeBounds.Contains(Args.Location) && Badge.ClickHandler != null)
									{
										Badge.ClickHandler();
										break;
									}
								}
							}
						}

						if(CISColumn.Index < HitTest.Item.SubItems.Count && HitTest.Item.SubItems[CISColumn.Index] == HitTest.SubItem)
						{
							ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo(Change);
							if(LayoutInfo.BuildBadges.Count > 0)
							{
								Point BuildListLocation = GetBadgeListLocation(LayoutInfo.BuildBadges, HitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
								BuildListLocation.X = Math.Max(BuildListLocation.X, HitTest.SubItem.Bounds.Left);

								BadgeInfo BadgeInfo = HitTestBadge(Args.Location, LayoutInfo.BuildBadges, BuildListLocation);
								if(BadgeInfo != null && BadgeInfo.ClickHandler != null)
								{
									BadgeInfo.ClickHandler();
								}
							}
						}

						foreach(ColumnHeader CustomColumn in CustomColumns)
						{
							if(CustomColumn.Index < HitTest.Item.SubItems.Count && HitTest.Item.SubItems[CustomColumn.Index] == HitTest.SubItem)
							{
								ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo((PerforceChangeSummary)HitTest.Item.Tag);

								List<BadgeInfo> Badges;
								if(LayoutInfo.CustomBadges.TryGetValue(CustomColumn.Text, out Badges) && Badges.Count > 0)
								{
									Point ListLocation = GetBadgeListLocation(Badges, HitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);

									BadgeInfo BadgeInfo = HitTestBadge(Args.Location, Badges, ListLocation);
									if(BadgeInfo != null && BadgeInfo.ClickHandler != null)
									{
										BadgeInfo.ClickHandler();
									}
								}
							}
						}
					}
				}
			}
			else if(Args.Button == MouseButtons.Right)
			{
				ListViewHitTestInfo HitTest = BuildList.HitTest(Args.Location);
				if(HitTest.Item != null && HitTest.Item.Tag != null)
				{
					if(BuildList.SelectedItems.Count > 1 && BuildList.SelectedItems.Contains(HitTest.Item))
					{
						bool bIsTimeColumn = (HitTest.Item.SubItems.IndexOf(HitTest.SubItem) == TimeColumn.Index);
						BuildListMultiContextMenu_TimeZoneSeparator.Visible = bIsTimeColumn;
						BuildListMultiContextMenu_ShowLocalTimes.Visible = bIsTimeColumn;
						BuildListMultiContextMenu_ShowLocalTimes.Checked = Settings.bShowLocalTimes;
						BuildListMultiContextMenu_ShowServerTimes.Visible = bIsTimeColumn;
						BuildListMultiContextMenu_ShowServerTimes.Checked = !Settings.bShowLocalTimes;

						BuildListMultiContextMenu.Show(BuildList, Args.Location);
					}
					else
					{
						ContextMenuChange = (PerforceChangeSummary)HitTest.Item.Tag;

						BuildListContextMenu_WithdrawReview.Visible = (EventMonitor.GetReviewByCurrentUser(ContextMenuChange.Number) != null);
						BuildListContextMenu_StartInvestigating.Visible = !EventMonitor.IsUnderInvestigationByCurrentUser(ContextMenuChange.Number);
						BuildListContextMenu_FinishInvestigating.Visible = EventMonitor.IsUnderInvestigation(ContextMenuChange.Number);

						string CommentText;
						bool bHasExistingComment = EventMonitor.GetCommentByCurrentUser(ContextMenuChange.Number, out CommentText);
						BuildListContextMenu_LeaveComment.Visible = !bHasExistingComment;
						BuildListContextMenu_EditComment.Visible = bHasExistingComment;

						bool bIsBusy = Workspace.IsBusy();
						bool bIsCurrentChange = (ContextMenuChange.Number == Workspace.CurrentChangeNumber);
						BuildListContextMenu_Sync.Visible = !bIsBusy;
						BuildListContextMenu_Sync.Font = new Font(SystemFonts.MenuFont, bIsCurrentChange? FontStyle.Regular : FontStyle.Bold);
						BuildListContextMenu_SyncContentOnly.Visible = !bIsBusy && ShouldSyncPrecompiledEditor;
						BuildListContextMenu_SyncOnlyThisChange.Visible = !bIsBusy && !bIsCurrentChange && ContextMenuChange.Number > Workspace.CurrentChangeNumber && Workspace.CurrentChangeNumber != -1;
						BuildListContextMenu_Build.Visible = !bIsBusy && bIsCurrentChange && !ShouldSyncPrecompiledEditor && Settings.bUseIncrementalBuilds;
						BuildListContextMenu_Rebuild.Visible = !bIsBusy && bIsCurrentChange && !ShouldSyncPrecompiledEditor;
						BuildListContextMenu_GenerateProjectFiles.Visible = !bIsBusy && bIsCurrentChange;
						BuildListContextMenu_LaunchEditor.Visible = !bIsBusy && ContextMenuChange.Number == Workspace.CurrentChangeNumber;
						BuildListContextMenu_LaunchEditor.Font = new Font(SystemFonts.MenuFont, FontStyle.Bold);
						BuildListContextMenu_OpenVisualStudio.Visible = !bIsBusy && bIsCurrentChange;
						BuildListContextMenu_Cancel.Visible = bIsBusy;

						BisectState State; 
						WorkspaceSettings.ChangeNumberToBisectState.TryGetValue(ContextMenuChange.Number, out State);
						bool bIsBisectMode = IsBisectModeEnabled();
						BuildListContextMenu_Bisect_Pass.Visible = bIsBisectMode && State != BisectState.Pass;
						BuildListContextMenu_Bisect_Fail.Visible = bIsBisectMode && State != BisectState.Fail;
						BuildListContextMenu_Bisect_Exclude.Visible = bIsBisectMode && State != BisectState.Exclude;
						BuildListContextMenu_Bisect_Include.Visible = bIsBisectMode && State != BisectState.Include;
						BuildListContextMenu_Bisect_Separator.Visible = bIsBisectMode;

						BuildListContextMenu_MarkGood.Visible = !bIsBisectMode;
						BuildListContextMenu_MarkBad.Visible = !bIsBisectMode;
						BuildListContextMenu_WithdrawReview.Visible = !bIsBisectMode;

						EventSummary Summary = EventMonitor.GetSummaryForChange(ContextMenuChange.Number);
						bool bStarred = (Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred);
						BuildListContextMenu_AddStar.Visible = !bStarred;
						BuildListContextMenu_RemoveStar.Visible = bStarred;

						bool bIsTimeColumn = (HitTest.Item.SubItems.IndexOf(HitTest.SubItem) == TimeColumn.Index);
						BuildListContextMenu_TimeZoneSeparator.Visible = bIsTimeColumn;
						BuildListContextMenu_ShowLocalTimes.Visible = bIsTimeColumn;
						BuildListContextMenu_ShowLocalTimes.Checked = Settings.bShowLocalTimes;
						BuildListContextMenu_ShowServerTimes.Visible = bIsTimeColumn;
						BuildListContextMenu_ShowServerTimes.Checked = !Settings.bShowLocalTimes;

						int CustomToolStart = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_Start) + 1;
						int CustomToolEnd = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_End);
						while(CustomToolEnd > CustomToolStart)
						{
							BuildListContextMenu.Items.RemoveAt(CustomToolEnd - 1);
							CustomToolEnd--;
						}

						ConfigFile ProjectConfigFile = PerforceMonitor.LatestProjectConfigFile;
						if(ProjectConfigFile != null)
						{
							Dictionary<string, string> Variables = GetWorkspaceVariables(ContextMenuChange.Number);

							string[] ChangeContextMenuEntries = ProjectConfigFile.GetValues("Options.ContextMenu", new string[0]);
							foreach(string ChangeContextMenuEntry in ChangeContextMenuEntries)
							{
								ConfigObject Object = new ConfigObject(ChangeContextMenuEntry);

								string Label = Object.GetValue("Label");
								string Execute = Object.GetValue("Execute");
								string Arguments = Object.GetValue("Arguments");

								if(Label != null && Execute != null)
								{
									Label = Utility.ExpandVariables(Label, Variables);
									Execute = Utility.ExpandVariables(Execute, Variables);
									Arguments = Utility.ExpandVariables(Arguments ?? "", Variables);

									ToolStripMenuItem Item = new ToolStripMenuItem(Label, null, new EventHandler((o, a) => Process.Start(Execute, Arguments)));
	
									BuildListContextMenu.Items.Insert(CustomToolEnd, Item);
									CustomToolEnd++;
								}
							}
						}

						BuildListContextMenu_CustomTool_End.Visible = (CustomToolEnd > CustomToolStart);

						BuildListContextMenu.Show(BuildList, Args.Location);
					}
				}
			}
		}

		private void BuildList_MouseMove(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo HitTest = BuildList.HitTest(e.Location);

			bool bNewMouseOverExpandLink = HitTest.Item != null && HitTestExpandLink(e.Location);
			if(bMouseOverExpandLink != bNewMouseOverExpandLink)
			{
				bMouseOverExpandLink = bNewMouseOverExpandLink;
				Cursor = bMouseOverExpandLink? NativeCursors.Hand : Cursors.Arrow;
				BuildList.Invalidate();
			}

			string NewHoverBadgeUniqueId = null;
			if(HitTest.Item != null && HitTest.Item.Tag is PerforceChangeSummary)
			{
				int ColumnIndex = HitTest.Item.SubItems.IndexOf(HitTest.SubItem);
				if(ColumnIndex == DescriptionColumn.Index)
				{
					ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo((PerforceChangeSummary)HitTest.Item.Tag);
					if(LayoutInfo.DescriptionBadges.Count > 0)
					{
						Point ListLocation = GetBadgeListLocation(LayoutInfo.DescriptionBadges, HitTest.SubItem.Bounds, HorizontalAlign.Right, VerticalAlignment.Middle);
						NewHoverBadgeUniqueId = HitTestBadge(e.Location, LayoutInfo.DescriptionBadges, ListLocation)?.UniqueId;
					}
				}
				else if(ColumnIndex == CISColumn.Index)
				{
					ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo((PerforceChangeSummary)HitTest.Item.Tag);
					if(LayoutInfo.BuildBadges.Count > 0)
					{
						Point BuildListLocation = GetBadgeListLocation(LayoutInfo.BuildBadges, HitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
						BuildListLocation.X = Math.Max(BuildListLocation.X, HitTest.SubItem.Bounds.Left);

						BadgeInfo Badge = HitTestBadge(e.Location, LayoutInfo.BuildBadges, BuildListLocation);
						NewHoverBadgeUniqueId = (Badge != null)? Badge.UniqueId : null;

						if(HoverBadgeUniqueId != NewHoverBadgeUniqueId)
						{
							if(Badge != null && Badge.ToolTip != null && Badge.BackgroundColor.A != 0)
							{
								BuildListToolTip.Show(Badge.ToolTip, BuildList, new Point(BuildListLocation.X + Badge.Offset, HitTest.Item.Bounds.Bottom + 2));
							}
							else
							{
								BuildListToolTip.Hide(BuildList);
							}
						}
					}
				}
				else if(CustomColumns.Contains(BuildList.Columns[ColumnIndex]))
				{
					ColumnHeader Column = BuildList.Columns[ColumnIndex];

					ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo((PerforceChangeSummary)HitTest.Item.Tag);

					List<BadgeInfo> Badges;
					if(LayoutInfo.CustomBadges.TryGetValue(Column.Text, out Badges) && Badges.Count > 0)
					{
						Point ListLocation = GetBadgeListLocation(Badges, HitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
						NewHoverBadgeUniqueId = HitTestBadge(e.Location, Badges, ListLocation)?.UniqueId;
					}
				}
			}
			if(HoverBadgeUniqueId != NewHoverBadgeUniqueId)
			{
				HoverBadgeUniqueId = NewHoverBadgeUniqueId;
				BuildList.Invalidate();
			}

			bool bNewHoverSync = false;
			if(HitTest.Item != null)
			{
				bNewHoverSync = GetSyncBadgeRectangle(HitTest.Item.SubItems[StatusColumn.Index].Bounds).Contains(e.Location);
			}
			if(bNewHoverSync != bHoverSync)
			{
				bHoverSync = bNewHoverSync;
				BuildList.Invalidate();
			}
		}

		private BadgeInfo HitTestBadge(Point Location, List<BadgeInfo> BadgeList, Point ListLocation)
		{
			foreach(BadgeInfo Badge in BadgeList)
			{
				Rectangle BadgeBounds = Badge.GetBounds(ListLocation);
				if(BadgeBounds.Contains(Location))
				{
					return Badge;
				}
			}
			return null;
		}

		private void OptionsButton_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked = Settings.bAutoResolveConflicts;
			OptionsContextMenu_SyncPrecompiledEditor.Enabled = PerforceMonitor != null && PerforceMonitor.HasZippedBinaries;
			OptionsContextMenu_SyncPrecompiledEditor.Checked = Settings.bSyncPrecompiledEditor;
			OptionsContextMenu_SyncPrecompiledEditor.ToolTipText = PerforceMonitor.ZippedBinariesStatus;
			OptionsContextMenu_EditorBuildConfiguration.Enabled = !ShouldSyncPrecompiledEditor;
			UpdateCheckedBuildConfig();
			OptionsContextMenu_UseIncrementalBuilds.Enabled = !ShouldSyncPrecompiledEditor;
			OptionsContextMenu_UseIncrementalBuilds.Checked = Settings.bUseIncrementalBuilds;
			OptionsContextMenu_CustomizeBuildSteps.Enabled = (Workspace != null);
			OptionsContextMenu_EditorArguments.Checked = Settings.EditorArguments.Any(x => x.Item2);
			OptionsContextMenu_ScheduledSync.Checked = Settings.bScheduleEnabled;
			OptionsContextMenu_TimeZone_Local.Checked = Settings.bShowLocalTimes;
			OptionsContextMenu_TimeZone_PerforceServer.Checked = !Settings.bShowLocalTimes;
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Checked = Settings.bShowUnreviewedChanges;
			OptionsContextMenu_ShowChanges_ShowAutomated.Checked = Settings.bShowAutomatedChanges;
			OptionsContextMenu_TabNames_Stream.Checked = Settings.TabLabels == TabLabels.Stream;
			OptionsContextMenu_TabNames_WorkspaceName.Checked = Settings.TabLabels == TabLabels.WorkspaceName;
			OptionsContextMenu_TabNames_WorkspaceRoot.Checked = Settings.TabLabels == TabLabels.WorkspaceRoot;
			OptionsContextMenu_TabNames_ProjectFile.Checked = Settings.TabLabels == TabLabels.ProjectFile;
			OptionsContextMenu.Show(OptionsButton, new Point(OptionsButton.Width - OptionsContextMenu.Size.Width, OptionsButton.Height));
		}

		private void BuildAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bBuildAfterSync = BuildAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void RunAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bRunAfterSync = RunAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void OpenSolutionAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			Settings.bOpenSolutionAfterSync = OpenSolutionAfterSyncCheckBox.Checked;
			Settings.Save();

			UpdateSyncActionCheckboxes();
		}

		private void UpdateSyncActionCheckboxes()
		{
			BuildAfterSyncCheckBox.Checked = Settings.bBuildAfterSync;
			RunAfterSyncCheckBox.Checked = Settings.bRunAfterSync;
			OpenSolutionAfterSyncCheckBox.Checked = Settings.bOpenSolutionAfterSync;

			if(Workspace == null || Workspace.IsBusy())
			{
				AfterSyncingLabel.Enabled = false;
				BuildAfterSyncCheckBox.Enabled = false;
				RunAfterSyncCheckBox.Enabled = false;
				OpenSolutionAfterSyncCheckBox.Enabled = false;
			}
			else
			{
				AfterSyncingLabel.Enabled = true;
				BuildAfterSyncCheckBox.Enabled = bHasBuildSteps;
				RunAfterSyncCheckBox.Enabled = BuildAfterSyncCheckBox.Checked;
				OpenSolutionAfterSyncCheckBox.Enabled = true;
			}
		}

		private void BuildList_FontChanged(object sender, EventArgs e)
		{
			if(BuildFont != null)
			{
				BuildFont.Dispose();
			}
			BuildFont = BuildList.Font;

			if(SelectedBuildFont != null)
			{
				SelectedBuildFont.Dispose();
			}
			SelectedBuildFont = new Font(BuildFont, FontStyle.Bold);

			if(BadgeFont != null)
			{
				BadgeFont.Dispose();
			}
			BadgeFont = new Font(BuildFont.FontFamily, BuildFont.SizeInPoints - 2, FontStyle.Bold);
		}

		public void SyncLatestChange()
		{
			SyncLatestChange(null);
		}

		public void SyncLatestChange(WorkspaceUpdateCallback Callback)
		{
			if(Workspace != null)
			{
				int ChangeNumber;
				if(!FindChangeToSync(Settings.SyncType, out ChangeNumber))
				{
					ShowErrorDialog(String.Format("Couldn't find any {0}changelist. Double-click on the change you want to sync manually.", (Settings.SyncType == LatestChangeType.Starred)? "starred " : (Settings.SyncType == LatestChangeType.Good)? "good " : ""));
				}
				else if(ChangeNumber < Workspace.CurrentChangeNumber)
				{
					ShowErrorDialog("Workspace is already synced to a later change ({0} vs {1}).", Workspace.CurrentChangeNumber, ChangeNumber);
				}
				else if(ChangeNumber >= PerforceMonitor.LastChangeByCurrentUser || MessageBox.Show(String.Format("The changelist that would be synced is before the last change you submitted.\n\nIf you continue, your changes submitted after CL {0} will be locally removed from your workspace until you can sync past them again.\n\nAre you sure you want to continue?", ChangeNumber), "Local changes will be removed", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					Owner.ShowAndActivate();
					SelectChange(ChangeNumber);
					StartSync(ChangeNumber, Callback);
				}
			}
		}

		private void BuildListContextMenu_MarkGood_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Good);
		}

		private void BuildListContextMenu_MarkBad_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Bad);
		}

		private void BuildListContextMenu_WithdrawReview_Click(object sender, EventArgs e)
		{
			EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Unknown);
		}

		private void BuildListContextMenu_Sync_Click(object sender, EventArgs e)
		{
			StartSync(ContextMenuChange.Number);
		}

		private void BuildListContextMenu_SyncContentOnly_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(ContextMenuChange.Number, WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.ContentOnly);
		}

		private void BuildListContextMenu_SyncOnlyThisChange_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(ContextMenuChange.Number, WorkspaceUpdateOptions.SyncSingleChange);
		}

		private void BuildListContextMenu_Build_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.UseIncrementalBuilds);
		}

		private void BuildListContextMenu_Rebuild_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build);
		}

		private void BuildListContextMenu_GenerateProjectFiles_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.GenerateProjectFiles);
		}

		private void BuildListContextMenu_CancelSync_Click(object sender, EventArgs e)
		{
			CancelWorkspaceUpdate();
		}

		private void BuildListContextMenu_MoreInfo_Click(object sender, EventArgs e)
		{
			if(!Utility.SpawnHiddenProcess("p4vc.exe", String.Format("-p\"{0}\" -c{1} change {2}", Workspace.Perforce.ServerAndPort ?? "perforce:1666", Workspace.ClientName, ContextMenuChange.Number)))
			{
				MessageBox.Show("Unable to spawn p4vc. Check you have P4V installed.");
			}
		}

		private void BuildListContextMenu_AddStar_Click(object sender, EventArgs e)
		{
			if(MessageBox.Show("Starred builds are meant to convey a stable, verified build to the rest of the team. Do not star a build unless it has been fully tested.\n\nAre you sure you want to star this build?", "Confirm star", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Starred);
			}
		}

		private void BuildListContextMenu_RemoveStar_Click(object sender, EventArgs e)
		{
			EventSummary Summary = EventMonitor.GetSummaryForChange(ContextMenuChange.Number);
			if(Summary != null && Summary.LastStarReview != null && Summary.LastStarReview.Type == EventType.Starred)
			{
				string Message = String.Format("This change was starred by {0}. Are you sure you want to remove it?", FormatUserName(Summary.LastStarReview.UserName));
				if(MessageBox.Show(Message, "Confirm removing star", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					EventMonitor.PostEvent(ContextMenuChange.Number, EventType.Unstarred);
				}
			}
		}

		private void BuildListContextMenu_LaunchEditor_Click(object sender, EventArgs e)
		{
			LaunchEditor();
		}

		private void BuildListContextMenu_StartInvestigating_Click(object sender, EventArgs e)
		{
			string Message = String.Format("All changes from {0} onwards will be marked as bad while you are investigating an issue.\n\nAre you sure you want to continue?", ContextMenuChange.Number);
			if(MessageBox.Show(Message, "Confirm investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				EventMonitor.StartInvestigating(ContextMenuChange.Number);
			}
		}

		private void BuildListContextMenu_FinishInvestigating_Click(object sender, EventArgs e)
		{
			int StartChangeNumber = EventMonitor.GetInvestigationStartChangeNumber(ContextMenuChange.Number);
			if(StartChangeNumber != -1)
			{
				if(ContextMenuChange.Number > StartChangeNumber)
				{
					string Message = String.Format("Mark all changes between {0} and {1} as bad?", StartChangeNumber, ContextMenuChange.Number);
					if(MessageBox.Show(Message, "Finish investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
					{
						foreach(PerforceChangeSummary Change in PerforceMonitor.GetChanges())
						{
							if (Change.Number >= StartChangeNumber && Change.Number < ContextMenuChange.Number)
							{
								EventMonitor.PostEvent(Change.Number, EventType.Bad);
							}
						}
					}
				}
				EventMonitor.FinishInvestigating(ContextMenuChange.Number);
			}
		}

		private void OnlyShowReviewedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}

		public void Activate()
		{
			UpdateSyncActionCheckboxes();

			if(PerforceMonitor != null)
			{
				PerforceMonitor.Refresh();
			}

			if(DesiredTaskbarState.Item1 == TaskbarState.Error)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				Owner.UpdateProgress();
			}
		}

		public void Deactivate()
		{
			ShrinkNumRequestedBuilds();
		}

		private void BuildList_ItemMouseHover(object sender, ListViewItemMouseHoverEventArgs Args)
		{
			Point ClientPoint = BuildList.PointToClient(Cursor.Position);
			if(Args.Item.SubItems.Count >= 6)
			{
				if(Args.Item.Bounds.Contains(ClientPoint))
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)Args.Item.Tag;
					if(Change == null)
					{
						return;
					}

					Rectangle CISBounds = Args.Item.SubItems[CISColumn.Index].Bounds;
					if(CISBounds.Contains(ClientPoint))
					{
						return;
					}

					Rectangle DescriptionBounds = Args.Item.SubItems[DescriptionColumn.Index].Bounds;
					if(DescriptionBounds.Contains(ClientPoint))
					{
						ChangeLayoutInfo LayoutInfo = GetChangeLayoutInfo(Change);
						if(LayoutInfo.DescriptionBadges.Count == 0 || ClientPoint.X < GetBadgeListLocation(LayoutInfo.DescriptionBadges, DescriptionBounds, HorizontalAlign.Right, VerticalAlignment.Middle).X - 2)
						{
							BuildListToolTip.Show(Change.Description, BuildList, new Point(DescriptionBounds.Left, DescriptionBounds.Bottom + 2));
							return;
						}
					}

					EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
					if(Summary != null)
					{
						StringBuilder SummaryText = new StringBuilder();
						if(Summary.Comments.Count > 0)
						{
							foreach(CommentData Comment in Summary.Comments)
							{
								if(!String.IsNullOrWhiteSpace(Comment.Text))
								{
									SummaryText.AppendFormat("{0}: \"{1}\"\n", FormatUserName(Comment.UserName), Comment.Text);
								}
							}
							if(SummaryText.Length > 0)
							{
								SummaryText.Append("\n");
							}
						}
						AppendUserList(SummaryText, "\n", "Compiled by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Compiles));
						AppendUserList(SummaryText, "\n", "Failed to compile for {0}.", Summary.Reviews.Where(x => x.Type == EventType.DoesNotCompile));
						AppendUserList(SummaryText, "\n", "Marked good by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Good));
						AppendUserList(SummaryText, "\n", "Marked bad by {0}.", Summary.Reviews.Where(x => x.Type == EventType.Bad));
						if(Summary.LastStarReview != null)
						{
							AppendUserList(SummaryText, "\n", "Starred by {0}.", new EventData[]{ Summary.LastStarReview });
						}
						if(SummaryText.Length > 0)
						{
							Rectangle SummaryBounds = Args.Item.SubItems[StatusColumn.Index].Bounds;
							BuildListToolTip.Show(SummaryText.ToString(), BuildList, new Point(SummaryBounds.Left, SummaryBounds.Bottom));
							return;
						}
					}
				}
			}

			BuildListToolTip.Hide(BuildList);
		}

		private void BuildList_MouseLeave(object sender, EventArgs e)
		{
			BuildListToolTip.Hide(BuildList);

			if(HoverBadgeUniqueId != null)
			{
				HoverBadgeUniqueId = null;
				BuildList.Invalidate();
			}

			if(bMouseOverExpandLink)
			{
				Cursor = Cursors.Arrow;
				bMouseOverExpandLink = false;
				BuildList.Invalidate();
			}
		}

		private void OptionsContextMenu_ShowLog_Click(object sender, EventArgs e)
		{
			ToggleLogVisibility();
		}

		private void ToggleLogVisibility()
		{
			Splitter.SetLogVisibility(!Splitter.IsLogVisible());
		}

		private void Splitter_OnVisibilityChanged(bool bVisible)
		{
			Settings.bShowLogWindow = bVisible;
			Settings.Save();

			UpdateStatusPanel();
		}

		private void OptionsContextMenu_AutoResolveConflicts_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked ^= true;
			Settings.bAutoResolveConflicts = OptionsContextMenu_AutoResolveConflicts.Checked;
			Settings.Save();
		}

		private void OptionsContextMenu_EditorArguments_Click(object sender, EventArgs e)
		{
			ModifyEditorArguments();
		}

		private bool ModifyEditorArguments()
		{
			ArgumentsWindow Arguments = new ArgumentsWindow(Settings.EditorArguments, Settings.bEditorArgumentsPrompt);
			if(Arguments.ShowDialog(this) == DialogResult.OK)
			{
				Settings.EditorArguments = Arguments.GetItems();
				Settings.bEditorArgumentsPrompt = Arguments.PromptBeforeLaunch;
				Settings.Save();
				return true;
			}
			return false;
		}

		private void BuildListContextMenu_OpenVisualStudio_Click(object sender, EventArgs e)
		{
			OpenSolution();
		}

		private void OpenSolution()
		{
			string MasterProjectName = "UE4";

			string MasterProjectNameFileName = Path.Combine(BranchDirectoryName, "Engine", "Intermediate", "ProjectFiles", "MasterProjectName.txt");
			if(File.Exists(MasterProjectNameFileName))
			{
				try
				{
					MasterProjectName = File.ReadAllText(MasterProjectNameFileName).Trim();
				}
				catch(Exception Ex)
				{
					Log.WriteException(Ex, "Unable to read '{0}'", MasterProjectNameFileName);
				}
			}

			string SolutionFileName = Path.Combine(BranchDirectoryName, MasterProjectName + ".sln");
			if(!File.Exists(SolutionFileName))
			{
				MessageBox.Show(String.Format("Couldn't find solution at {0}", SolutionFileName));
			}
			else
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo(SolutionFileName);
				StartInfo.WorkingDirectory = BranchDirectoryName;
				Process.Start(StartInfo);
			}
		}

		private void OptionsContextMenu_BuildConfig_Debug_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Debug);
		}

		private void OptionsContextMenu_BuildConfig_DebugGame_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.DebugGame);
		}

		private void OptionsContextMenu_BuildConfig_Development_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Development);
		}

		void UpdateBuildConfig(BuildConfig NewConfig)
		{
			Settings.CompiledEditorBuildConfig = NewConfig;
			Settings.Save();
			UpdateCheckedBuildConfig();
		}

		void UpdateCheckedBuildConfig()
		{
			BuildConfig EditorBuildConfig = GetEditorBuildConfig();

			OptionsContextMenu_BuildConfig_Debug.Checked = (EditorBuildConfig == BuildConfig.Debug);
			OptionsContextMenu_BuildConfig_Debug.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.Debug);

			OptionsContextMenu_BuildConfig_DebugGame.Checked = (EditorBuildConfig == BuildConfig.DebugGame);
			OptionsContextMenu_BuildConfig_DebugGame.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.DebugGame);

			OptionsContextMenu_BuildConfig_Development.Checked = (EditorBuildConfig == BuildConfig.Development);
			OptionsContextMenu_BuildConfig_Development.Enabled = (!ShouldSyncPrecompiledEditor || EditorBuildConfig == BuildConfig.Development);
		}

		private void OptionsContextMenu_UseIncrementalBuilds_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_UseIncrementalBuilds.Checked ^= true;
			Settings.bUseIncrementalBuilds = OptionsContextMenu_UseIncrementalBuilds.Checked;
			Settings.Save();
		}

		private void OptionsContextMenu_ScheduleSync_Click(object sender, EventArgs e)
		{
			Owner.SetupScheduledSync();
		}

		public void ScheduleTimerElapsed()
		{
			if(Settings.bScheduleEnabled)
			{
				Log.WriteLine("Scheduled sync at {0} for {1}.", DateTime.Now, Settings.ScheduleChange);

				int ChangeNumber;
				if(!FindChangeToSync(Settings.ScheduleChange, out ChangeNumber))
				{
					Log.WriteLine("Couldn't find any matching change");
				}
				else if(Workspace.CurrentChangeNumber >= ChangeNumber)
				{
					Log.WriteLine("Sync ignored; already at or ahead of CL ({0} >= {1})", Workspace.CurrentChangeNumber, ChangeNumber);
				}
				else
				{
					WorkspaceUpdateOptions Options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.ScheduledBuild;
					if(Settings.bAutoResolveConflicts)
					{
						Options |= WorkspaceUpdateOptions.AutoResolveChanges;
					}
					StartWorkspaceUpdate(ChangeNumber, Options);
				}
			}
		}

		bool FindChangeToSync(LatestChangeType ChangeType, out int ChangeNumber)
		{
			for(int Idx = 0; Idx < BuildList.Items.Count; Idx++)
			{
				PerforceChangeSummary Change = (PerforceChangeSummary)BuildList.Items[Idx].Tag;
				if(Change != null)
				{
					if(ChangeType == LatestChangeType.Any)
					{
						if(CanSyncChange(Change.Number))
						{
							ChangeNumber = FindNewestGoodContentChange(Change.Number);
							return true;
						}
					}
					else if(ChangeType == LatestChangeType.Good)
					{
						EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
						if(Summary != null && Summary.Verdict == ReviewVerdict.Good && CanSyncChange(Change.Number))
						{
							ChangeNumber = FindNewestGoodContentChange(Change.Number);
							return true;
						}
					}
					else if(ChangeType == LatestChangeType.Starred)
					{
						EventSummary Summary = EventMonitor.GetSummaryForChange(Change.Number);
						if(((Summary != null && Summary.LastStarReview != null) || PromotedChangeNumbers.Contains(Change.Number)) && CanSyncChange(Change.Number))
						{
							ChangeNumber = FindNewestGoodContentChange(Change.Number);
							return true;
						}
					}
				}
			}

			ChangeNumber = -1;
			return false;
		}
		
		int FindNewestGoodContentChange(int ChangeNumber)
		{
			int Index = SortedChangeNumbers.BinarySearch(ChangeNumber);
			if(Index <= 0)
			{
				return ChangeNumber;
			}

			for(int NextIndex = Index + 1; NextIndex < SortedChangeNumbers.Count; NextIndex++)
			{
				int NextChangeNumber = SortedChangeNumbers[NextIndex];

				PerforceChangeDetails Details;
				if(!PerforceMonitor.TryGetChangeDetails(NextChangeNumber, out Details) || Details.bContainsCode)
				{
					break;
				}

				EventSummary Summary = EventMonitor.GetSummaryForChange(NextChangeNumber);
				if(Summary != null && Summary.Verdict == ReviewVerdict.Bad)
				{
					break;
				}

				Index = NextIndex;
			}

			return SortedChangeNumbers[Index];
		}

		private void BuildListContextMenu_LeaveOrEditComment_Click(object sender, EventArgs e)
		{
			if(ContextMenuChange != null)
			{
				LeaveCommentWindow LeaveComment = new LeaveCommentWindow();

				string CommentText;
				if(EventMonitor.GetCommentByCurrentUser(ContextMenuChange.Number, out CommentText))
				{
					LeaveComment.CommentTextBox.Text = CommentText;
				}

				if(LeaveComment.ShowDialog() == System.Windows.Forms.DialogResult.OK)
				{
					EventMonitor.PostComment(ContextMenuChange.Number, LeaveComment.CommentTextBox.Text);
				}
			}
		}

		private void BuildListContextMenu_ShowServerTimes_Click(object sender, EventArgs e)
		{
			Settings.bShowLocalTimes = false;
			Settings.Save();
			UpdateBuildList();
		}

		private void BuildListContextMenu_ShowLocalTimes_Click(object sender, EventArgs e)
		{
			Settings.bShowLocalTimes = true;
			Settings.Save();
			UpdateBuildList();
		}

		private void MoreToolsContextMenu_CleanWorkspace_Click(object sender, EventArgs e)
		{
			if(!WaitForProgramsToFinish())
			{
				return;
			}

			string ExtraSafeToDeleteFolders;
			if(!TryGetProjectSetting(Workspace.ProjectConfigFile, "SafeToDeleteFolders", out ExtraSafeToDeleteFolders))
			{
				ExtraSafeToDeleteFolders = "";
			}

			string ExtraSafeToDeleteExtensions;
			if(!TryGetProjectSetting(Workspace.ProjectConfigFile, "SafeToDeleteExtensions", out ExtraSafeToDeleteExtensions))
			{
				ExtraSafeToDeleteExtensions = "";
			}

			string[] CombinedSyncFilter = UserSettings.GetCombinedSyncFilter(Workspace.GetSyncCategories(), Settings.SyncView, Settings.SyncExcludedCategories, WorkspaceSettings.SyncView, WorkspaceSettings.SyncIncludedCategories, WorkspaceSettings.SyncExcludedCategories);
			List<string> SyncPaths = Workspace.GetSyncPaths(WorkspaceSettings.bSyncAllProjects ?? Settings.bSyncAllProjects, CombinedSyncFilter);

			CleanWorkspaceWindow.DoClean(ParentForm, Workspace.Perforce, BranchDirectoryName, Workspace.ClientRootPath, SyncPaths, ExtraSafeToDeleteFolders.Split('\n'), ExtraSafeToDeleteExtensions.Split('\n'), Log);
		}

		private void UpdateBuildSteps()
		{
			bHasBuildSteps = false;

			foreach(ToolStripMenuItem CustomToolMenuItem in CustomToolMenuItems)
			{
				MoreToolsContextMenu.Items.Remove(CustomToolMenuItem);
			}

			CustomToolMenuItems.Clear();

			if(Workspace != null)
			{
				ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
				if(ProjectConfigFile != null)
				{
					Dictionary<Guid, ConfigObject> ProjectBuildStepObjects = GetProjectBuildStepObjects(ProjectConfigFile);

					int InsertIdx = 0;

					List<BuildStep> UserSteps = GetUserBuildSteps(ProjectBuildStepObjects);
					foreach(BuildStep Step in UserSteps)
					{
						if(Step.bShowAsTool)
						{
							ToolStripMenuItem NewMenuItem = new ToolStripMenuItem(Step.Description.Replace("&", "&&"));
							NewMenuItem.Click += new EventHandler((sender, e) => { RunCustomTool(Step.UniqueId); });
							CustomToolMenuItems.Add(NewMenuItem);
							MoreToolsContextMenu.Items.Insert(InsertIdx++, NewMenuItem);
						}
						bHasBuildSteps |= Step.bNormalSync;
					}
				}
			}

			MoreActionsContextMenu_CustomToolSeparator.Visible = (CustomToolMenuItems.Count > 0);
		}

		private void RunCustomTool(Guid UniqueId)
		{
			if(Workspace != null)
			{
				if(Workspace.IsBusy())
				{
					MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
				}
				else
				{
					WorkspaceUpdateContext Context = new WorkspaceUpdateContext(Workspace.CurrentChangeNumber, WorkspaceUpdateOptions.Build, null, GetDefaultBuildStepObjects(), ProjectSettings.BuildSteps, new HashSet<Guid>{ UniqueId }, GetWorkspaceVariables(Workspace.CurrentChangeNumber));
					StartWorkspaceUpdate(Context, null);
				}
			}
		}

		private Dictionary<string, string> GetWorkspaceVariables(int ChangeNumber)
		{
			BuildConfig EditorBuildConfig = GetEditorBuildConfig();

			string SdkInstallerDir;
			TryGetProjectSetting(PerforceMonitor.LatestProjectConfigFile, "SdkInstallerDir", out SdkInstallerDir);

			Dictionary<string, string> Variables = new Dictionary<string,string>();
			Variables.Add("Stream", StreamName);
			Variables.Add("Change", ChangeNumber.ToString());
			Variables.Add("ClientName", ClientName);
			Variables.Add("BranchDir", BranchDirectoryName);
			Variables.Add("ProjectDir", Path.GetDirectoryName(SelectedFileName));
			Variables.Add("ProjectFile", SelectedFileName);
			Variables.Add("UE4EditorExe", GetEditorExePath(EditorBuildConfig));
			Variables.Add("UE4EditorCmdExe", GetEditorExePath(EditorBuildConfig).Replace(".exe", "-Cmd.exe"));
			Variables.Add("UE4EditorConfig", EditorBuildConfig.ToString());
			Variables.Add("UE4EditorDebugArg", (EditorBuildConfig == BuildConfig.Debug || EditorBuildConfig == BuildConfig.DebugGame)? " -debug" : "");
			Variables.Add("UseIncrementalBuilds", Settings.bUseIncrementalBuilds? "1" : "0");
			if(!String.IsNullOrEmpty(SdkInstallerDir))
			{
				Variables.Add("SdkInstallerDir", SdkInstallerDir);
			}
			return Variables;
		}

		private void OptionsContextMenu_EditBuildSteps_Click(object sender, EventArgs e)
		{
			ConfigFile ProjectConfigFile = Workspace.ProjectConfigFile;
			if(Workspace != null && ProjectConfigFile != null)
			{
				// Find all the target names for this project
				List<string> TargetNames = new List<string>();
				if(!String.IsNullOrEmpty(SelectedFileName) && SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
				{
					DirectoryInfo SourceDirectory = new DirectoryInfo(Path.Combine(Path.GetDirectoryName(SelectedFileName), "Source"));
					if(SourceDirectory.Exists)
					{
						foreach(FileInfo TargetFile in SourceDirectory.EnumerateFiles("*.target.cs", SearchOption.TopDirectoryOnly))
						{
							TargetNames.Add(TargetFile.Name.Substring(0, TargetFile.Name.IndexOf('.')));
						}
					}
				}

				// Get all the task objects
				Dictionary<Guid, ConfigObject> ProjectBuildStepObjects = GetProjectBuildStepObjects(ProjectConfigFile);
				List<BuildStep> UserSteps = GetUserBuildSteps(ProjectBuildStepObjects);

				// Show the dialog
				ModifyBuildStepsWindow EditStepsWindow = new ModifyBuildStepsWindow(TargetNames, UserSteps, new HashSet<Guid>(ProjectBuildStepObjects.Keys), BranchDirectoryName, GetWorkspaceVariables(Workspace.CurrentChangeNumber));
				EditStepsWindow.ShowDialog();

				// Update the user settings
				List<ConfigObject> ModifiedBuildSteps = new List<ConfigObject>();
				foreach(BuildStep Step in UserSteps)
				{
					if(Step.IsValid())
					{
						ConfigObject DefaultObject;
						ProjectBuildStepObjects.TryGetValue(Step.UniqueId, out DefaultObject);

						ConfigObject UserConfigObject = Step.ToConfigObject(DefaultObject);
						if(UserConfigObject != null && UserConfigObject.Pairs.Any(x => x.Key != "UniqueId"))
						{
							ModifiedBuildSteps.Add(UserConfigObject);
						}
					}
				}

				// Save the settings
				ProjectSettings.BuildSteps = ModifiedBuildSteps;
				Settings.Save();

				// Update the custom tools menu, because we might have changed it
				UpdateBuildSteps();
				UpdateSyncActionCheckboxes();
			}
		}

		private void AddOrUpdateBuildStep(Dictionary<Guid, ConfigObject> Steps, ConfigObject Object)
		{
			Guid UniqueId;
			if(Guid.TryParse(Object.GetValue(BuildStep.UniqueIdKey, ""), out UniqueId))
			{
				// Add or apply Object to the list of steps in Steps. Do not modify Object; make a copy first.
				ConfigObject NewObject = new ConfigObject(Object);

				ConfigObject DefaultObject;
				if(Steps.TryGetValue(UniqueId, out DefaultObject))
				{
					NewObject.SetDefaults(DefaultObject);
				}

				Steps[UniqueId] = NewObject;
			}
		}

		private bool ShouldSyncPrecompiledEditor
		{
			get { return Settings.bSyncPrecompiledEditor && PerforceMonitor != null && PerforceMonitor.HasZippedBinaries; }
		}

		public BuildConfig GetEditorBuildConfig()
		{
			return ShouldSyncPrecompiledEditor? BuildConfig.Development : Settings.CompiledEditorBuildConfig;
		}

		private Dictionary<Guid,ConfigObject> GetDefaultBuildStepObjects()
		{
			string ProjectArgument = "";
			if(SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase) && EditorTargetName != null)
			{
				ProjectArgument = String.Format("\"{0}\"", SelectedFileName);
			}

			string ActualEditorTargetName;
			if(EditorTargetName != null)
			{
				ActualEditorTargetName = EditorTargetName;
			}
			else if(SelectedFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase) && bIsEnterpriseProject)
			{
				ActualEditorTargetName = "StudioEditor";
			}
			else
			{
				ActualEditorTargetName = "UE4Editor";
			}

			List<BuildStep> DefaultBuildSteps = new List<BuildStep>();
			DefaultBuildSteps.Add(new BuildStep(new Guid("{01F66060-73FA-4CC8-9CB3-E217FBBA954E}"), 0, "Compile UnrealHeaderTool", "Compiling UnrealHeaderTool...", 1, "UnrealHeaderTool", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{F097FF61-C916-4058-8391-35B46C3173D5}"), 1, String.Format("Compile {0}", ActualEditorTargetName), String.Format("Compiling {0}...", ActualEditorTargetName), 10, ActualEditorTargetName, "Win64", Settings.CompiledEditorBuildConfig.ToString(), ProjectArgument, !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{C6E633A1-956F-4AD3-BC95-6D06D131E7B4}"), 2, "Compile ShaderCompileWorker", "Compiling ShaderCompileWorker...", 1, "ShaderCompileWorker", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{24FFD88C-7901-4899-9696-AE1066B4B6E8}"), 3, "Compile UnrealLightmass", "Compiling UnrealLightmass...", 1, "UnrealLightmass", "Win64", "Development", "", !ShouldSyncPrecompiledEditor));
			DefaultBuildSteps.Add(new BuildStep(new Guid("{FFF20379-06BF-4205-8A3E-C53427736688}"), 4, "Compile CrashReportClient", "Compiling CrashReportClient...", 1, "CrashReportClient", "Win64", "Shipping", "", !ShouldSyncPrecompiledEditor));
			return DefaultBuildSteps.ToDictionary(x => x.UniqueId, x => x.ToConfigObject());
		}

		private Dictionary<Guid, ConfigObject> GetProjectBuildStepObjects(ConfigFile ProjectConfigFile)
		{
			Dictionary<Guid, ConfigObject> ProjectBuildSteps = GetDefaultBuildStepObjects();
			foreach(string Line in ProjectConfigFile.GetValues("Build.Step", new string[0]))
			{
				AddOrUpdateBuildStep(ProjectBuildSteps, new ConfigObject(Line));
			}
			return ProjectBuildSteps;
		}
		
		private List<BuildStep> GetUserBuildSteps(Dictionary<Guid, ConfigObject> ProjectBuildStepObjects)
		{
			// Read all the user-defined build tasks and modifications to the default list
			Dictionary<Guid, ConfigObject> UserBuildStepObjects = ProjectBuildStepObjects.ToDictionary(x => x.Key, y => new ConfigObject(y.Value));
			foreach(ConfigObject UserBuildStep in ProjectSettings.BuildSteps)
			{
				AddOrUpdateBuildStep(UserBuildStepObjects, UserBuildStep);
			}

			// Create the expanded task objects
			return UserBuildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => x.OrderIndex).ToList();
		}

		private void OptionsContextMenu_SyncPrecompiledEditor_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_SyncPrecompiledEditor.Checked ^= true;

			Settings.bSyncPrecompiledEditor = OptionsContextMenu_SyncPrecompiledEditor.Checked;
			Settings.Save();

			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();

			BuildList.Invalidate();
		}

		private void BuildList_SelectedIndexChanged(object sender, EventArgs e)
		{
			PendingSelectedChangeNumber = -1;
		}

		private void OptionsContextMenu_Diagnostics_Click(object sender, EventArgs e)
		{
			StringBuilder DiagnosticsText = new StringBuilder();
			DiagnosticsText.AppendFormat("Application version: {0}\n", Assembly.GetExecutingAssembly().GetName().Version);
			DiagnosticsText.AppendFormat("Synced from: {0}\n", Program.SyncVersion ?? "(unknown)");
			DiagnosticsText.AppendFormat("Selected file: {0}\n", (SelectedFileName == null)? "(none)" : SelectedFileName);
			if(Workspace != null)
			{
				DiagnosticsText.AppendFormat("P4 server: {0}\n", Workspace.Perforce.ServerAndPort ?? "(default)");
				DiagnosticsText.AppendFormat("P4 user: {0}\n", Workspace.Perforce.UserName);
				DiagnosticsText.AppendFormat("P4 workspace: {0}\n", Workspace.Perforce.ClientName);
			}
			DiagnosticsText.AppendFormat("Perforce monitor: {0}\n", (PerforceMonitor == null)? "(inactive)" : PerforceMonitor.LastStatusMessage);
			DiagnosticsText.AppendFormat("Event monitor: {0}\n", (EventMonitor == null)? "(inactive)" : EventMonitor.LastStatusMessage);

			DiagnosticsWindow Diagnostics = new DiagnosticsWindow(DataFolder, DiagnosticsText.ToString());
			Diagnostics.ShowDialog(this);
		}

		private void OptionsContextMenu_SyncFilter_Click(object sender, EventArgs e)
		{
			SyncFilter Filter = new SyncFilter(Workspace.GetSyncCategories(), Settings.SyncView, Settings.SyncExcludedCategories, Settings.bSyncAllProjects, Settings.bIncludeAllProjectsInSolution, WorkspaceSettings.SyncView, WorkspaceSettings.SyncIncludedCategories, WorkspaceSettings.SyncExcludedCategories, WorkspaceSettings.bSyncAllProjects, WorkspaceSettings.bIncludeAllProjectsInSolution);
			if(Filter.ShowDialog() == DialogResult.OK)
			{
				Settings.SyncExcludedCategories = Filter.GlobalExcludedCategories;
				Settings.SyncView = Filter.GlobalView;
				Settings.bSyncAllProjects = Filter.bGlobalSyncAllProjects;
				Settings.bIncludeAllProjectsInSolution = Filter.bGlobalIncludeAllProjectsInSolution;
				WorkspaceSettings.SyncIncludedCategories = Filter.WorkspaceIncludedCategories;
				WorkspaceSettings.SyncExcludedCategories = Filter.WorkspaceExcludedCategories;
				WorkspaceSettings.SyncView = Filter.WorkspaceView;
				WorkspaceSettings.bSyncAllProjects = Filter.bWorkspaceSyncAllProjects;
				WorkspaceSettings.bIncludeAllProjectsInSolution = Filter.bWorkspaceIncludeAllProjectsInSolution;
                Settings.Save();
			}
		}

		private void ShowSyncMenu(Rectangle Bounds)
		{
			SyncContextMenu_LatestChange.Checked = (Settings.SyncType == LatestChangeType.Any);
			SyncContextMenu_LatestGoodChange.Checked = (Settings.SyncType == LatestChangeType.Good);
			SyncContextMenu_LatestStarredChange.Checked = (Settings.SyncType == LatestChangeType.Starred);
			SyncContextMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SyncContextMenu_LatestChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Any;
			Settings.Save();
		}

		private void SyncContextMenu_LatestGoodChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Good;
			Settings.Save();
		}

		private void SyncContextMenu_LatestStarredChange_Click(object sender, EventArgs e)
		{
			Settings.SyncType = LatestChangeType.Starred;
			Settings.Save();
		}

		private void SyncContextMenu_EnterChangelist_Click(object sender, EventArgs e)
		{
			if(!WaitForProgramsToFinish())
			{
				return;
			}

			ChangelistWindow ChildWindow = new ChangelistWindow((Workspace == null)? -1 : Workspace.CurrentChangeNumber);
			if(ChildWindow.ShowDialog() == DialogResult.OK)
			{
				StartSync(ChildWindow.ChangeNumber);
			}
		}

		private void BuildList_KeyDown(object Sender, KeyEventArgs Args)
		{
			if(Args.Control && Args.KeyCode == Keys.C && BuildList.SelectedItems.Count > 0)
			{
				int SelectedChange = ((PerforceChangeSummary)BuildList.SelectedItems[0].Tag).Number;
				Clipboard.SetText(String.Format("{0}", SelectedChange));
			}
		}

		private void OptionsContextMenu_ApplicationSettings_Click(object sender, EventArgs e)
		{
			Owner.ModifyApplicationSettings();
		}

		private void OptionsContextMenu_PerforceSettings_Click(object sender, EventArgs e)
		{
			PerforceSyncSettingsWindow Window = new PerforceSyncSettingsWindow(Settings);
			Window.ShowDialog();
		}

		private void OptionsContextMenu_TabNames_Stream_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.Stream);
		}

		private void OptionsContextMenu_TabNames_WorkspaceName_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.WorkspaceName);
		}

		private void OptionsContextMenu_TabNames_WorkspaceRoot_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.WorkspaceRoot);
		}

		private void OptionsContextMenu_TabNames_ProjectFile_Click(object sender, EventArgs e)
		{
			Owner.SetTabNames(TabLabels.ProjectFile);
		}

		private void SelectRecentProject(Rectangle Bounds)
		{
			while(RecentMenu.Items[2] != RecentMenu_Separator)
			{
				RecentMenu.Items.RemoveAt(2);
			}

			foreach(UserSelectedProjectSettings RecentProject in Settings.RecentProjects)
			{
				ToolStripMenuItem Item = new ToolStripMenuItem(RecentProject.ToString(), null, new EventHandler((o, e) => Owner.RequestProjectChange(this, RecentProject, true)));
				RecentMenu.Items.Insert(RecentMenu.Items.Count - 2, Item);
			}

			RecentMenu_Separator.Visible = (Settings.RecentProjects.Count > 0);
			RecentMenu.Show(StatusPanel, new Point(Bounds.Left, Bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void RecentMenu_Browse_Click(object sender, EventArgs e)
		{
			Owner.EditSelectedProject(this);
		}

		private void RecentMenu_ClearList_Click(object sender, EventArgs e)
		{
			Settings.RecentProjects.Clear();
			Settings.Save();
		}

		private void OptionsContextMenu_ShowChanges_ShowUnreviewed_Click(object sender, EventArgs e)
		{
			Settings.bShowUnreviewedChanges ^= true;
			Settings.Save();

			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}

		private void OptionsContextMenu_ShowChanges_ShowAutomated_Click(object sender, EventArgs e)
		{
			Settings.bShowAutomatedChanges ^= true;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void BuildList_ColumnWidthChanging(object sender, ColumnWidthChangingEventArgs e)
		{
			UpdateMaxBuildBadgeChars();
		}

		private void BuildList_ColumnWidthChanged(object sender, ColumnWidthChangedEventArgs e)
		{
			if(ColumnWidths != null && MinColumnWidths != null)
			{
				int NewWidth = BuildList.Columns[e.ColumnIndex].Width;
				if(NewWidth < MinColumnWidths[e.ColumnIndex])
				{
					NewWidth = MinColumnWidths[e.ColumnIndex];
					BuildList.Columns[e.ColumnIndex].Width = NewWidth;
				}
				ColumnWidths[e.ColumnIndex] = NewWidth;
			}
			UpdateMaxBuildBadgeChars();
		}

		private void BuildList_Resize(object sender, EventArgs e)
		{
			int PrevBuildListWidth = BuildListWidth;

			BuildListWidth = BuildList.Width;

			if (PrevBuildListWidth != 0 && BuildListWidth != PrevBuildListWidth && ColumnWidths != null)
			{
				float SafeWidth = ColumnWidths.Sum() + 50.0f;
				if(BuildListWidth < PrevBuildListWidth)
				{
					if(BuildListWidth <= SafeWidth)
					{
						ResizeColumns(ColumnWidths.Sum() + (BuildListWidth - Math.Max(PrevBuildListWidth, SafeWidth)));
					}
				}
				else
				{
					if(BuildListWidth >= SafeWidth)
					{
						ResizeColumns(ColumnWidths.Sum() + (BuildListWidth - Math.Max(PrevBuildListWidth, SafeWidth)));
					}
				}
			}
		}

		void ResizeColumns(float NextTotalWidth)
		{
			float[] TargetColumnWidths = GetTargetColumnWidths(NextTotalWidth);

			// Get the current total width of the columns, and the new space that we'll aim to fill
			float PrevTotalWidth = ColumnWidths.Sum();
			float TotalDelta = Math.Abs(NextTotalWidth - PrevTotalWidth);

			float TotalColumnDelta = 0.0f;
			for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
			{
				TotalColumnDelta += Math.Abs(TargetColumnWidths[Idx] - ColumnWidths[Idx]);
			}

			if(TotalColumnDelta > 0.5f)
			{
				float[] NextColumnWidths = new float[BuildList.Columns.Count];
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					float MaxColumnDelta = TotalDelta * Math.Abs(TargetColumnWidths[Idx] - ColumnWidths[Idx]) / TotalColumnDelta;
					NextColumnWidths[Idx] = Math.Max(Math.Min(TargetColumnWidths[Idx], ColumnWidths[Idx] + MaxColumnDelta), ColumnWidths[Idx] - MaxColumnDelta);
				}

				// Update the control
				BuildList.BeginUpdate();
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					BuildList.Columns[Idx].Width = (int)NextColumnWidths[Idx];
				}
				ColumnWidths = NextColumnWidths;
				BuildList.EndUpdate();
			}
		}

		float[] GetTargetColumnWidths(float NextTotalWidth)
		{
			// Array to store the output list
			float[] TargetColumnWidths = new float[BuildList.Columns.Count];

			// Array of flags to store columns which are clamped into position. We try to respect proportional resizing as well as clamping to min/max sizes,
			// and remaining space can be distributed via non-clamped columns via another iteration.
			bool[] ConstrainedColumns = new bool[BuildList.Columns.Count];

			// Keep track of the remaining width that we have to distribute between columns. Does not include the required minimum size of each column.
			float RemainingWidth = Math.Max(NextTotalWidth - MinColumnWidths.Sum(), 0.0f);

			// Keep track of the sum of the remaining column weights. Used to proportionally allocate remaining space.
			float RemainingTotalWeight = ColumnWeights.Sum();

			// Handle special cases for shrinking/expanding
			float PrevTotalWidth = ColumnWidths.Sum();
			if(NextTotalWidth < PrevTotalWidth)
			{
				// If target size is less than current size, keep it at the current size
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					if(!ConstrainedColumns[Idx])
					{
						float TargetColumnWidth = MinColumnWidths[Idx] + (RemainingWidth * ColumnWeights[Idx] / RemainingTotalWeight);
						if(TargetColumnWidth > ColumnWidths[Idx])
						{
							TargetColumnWidths[Idx] = ColumnWidths[Idx];
							ConstrainedColumns[Idx] = true;

							RemainingWidth -= Math.Max(TargetColumnWidths[Idx] - MinColumnWidths[Idx], 0.0f);
							RemainingTotalWeight -= ColumnWeights[Idx];

							Idx = -1;
							continue;
						}
					}
				}
			}
			else
			{
				// If target size is greater than desired size, clamp it to that
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					if(!ConstrainedColumns[Idx])
					{
						float TargetColumnWidth = MinColumnWidths[Idx] + (RemainingWidth * ColumnWeights[Idx] / RemainingTotalWeight);
						if(TargetColumnWidth > DesiredColumnWidths[Idx])
						{
							// Don't allow this column to expand above the maximum desired size
							TargetColumnWidths[Idx] = DesiredColumnWidths[Idx];
							ConstrainedColumns[Idx] = true;

							RemainingWidth -= Math.Max(TargetColumnWidths[Idx] - MinColumnWidths[Idx], 0.0f);
							RemainingTotalWeight -= ColumnWeights[Idx];

							Idx = -1;
							continue;
						}
					}
				}

				// If current size is greater than target size, keep it that way
				for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
				{
					if(!ConstrainedColumns[Idx])
					{
						float TargetColumnWidth = MinColumnWidths[Idx] + (RemainingWidth * ColumnWeights[Idx] / RemainingTotalWeight);
						if(TargetColumnWidth < ColumnWidths[Idx])
						{
							TargetColumnWidths[Idx] = ColumnWidths[Idx];
							ConstrainedColumns[Idx] = true;

							RemainingWidth -= Math.Max(TargetColumnWidths[Idx] - MinColumnWidths[Idx], 0.0f);
							RemainingTotalWeight -= ColumnWeights[Idx];

							Idx = -1;
							continue;
						}
					}
				}
			}

			// Allocate the remaining space equally
			for(int Idx = 0; Idx < BuildList.Columns.Count; Idx++)
			{
				if(!ConstrainedColumns[Idx])
				{
					TargetColumnWidths[Idx] = MinColumnWidths[Idx] + (RemainingWidth * ColumnWeights[Idx] / RemainingTotalWeight);
				}
			}

			return TargetColumnWidths;
		}

		private void BuidlListMultiContextMenu_Bisect_Click(object sender, EventArgs e)
		{
			EnableBisectMode();
		}

		private bool IsBisectModeEnabled()
		{
			return WorkspaceSettings.ChangeNumberToBisectState.Count >= 2;
		}

		private void EnableBisectMode()
		{
			if(BuildList.SelectedItems.Count >= 2)
			{
				Dictionary<int, BisectState> ChangeNumberToBisectState = new Dictionary<int, BisectState>();
				foreach(ListViewItem SelectedItem in BuildList.SelectedItems)
				{
					PerforceChangeSummary Change = (PerforceChangeSummary)SelectedItem.Tag;
					ChangeNumberToBisectState[Change.Number] = BisectState.Include;
				}

				ChangeNumberToBisectState[ChangeNumberToBisectState.Keys.Min()] = BisectState.Pass;
				ChangeNumberToBisectState[ChangeNumberToBisectState.Keys.Max()] = BisectState.Fail;

				WorkspaceSettings.ChangeNumberToBisectState = ChangeNumberToBisectState;
				Settings.Save();

				UpdateBuildList();
				UpdateStatusPanel();
			}
		}

		private void CancelBisectMode()
		{
			WorkspaceSettings.ChangeNumberToBisectState.Clear();
			Settings.Save();

			UpdateBuildList();
			UpdateStatusPanel();
		}

		private void SetBisectStateForSelection(BisectState State)
		{
			foreach(ListViewItem SelectedItem in BuildList.SelectedItems)
			{
				PerforceChangeSummary Change = (PerforceChangeSummary)SelectedItem.Tag;
				if(Change != null)
				{
					WorkspaceSettings.ChangeNumberToBisectState[Change.Number] = State;
				}
			}

			Settings.Save();

			ChangeNumberToLayoutInfo.Clear();
			BuildList.Invalidate();

			UpdateStatusPanel();
		}

		private int GetBisectChangeNumber()
		{
			int PassChangeNumber;
			int FailChangeNumber;
			GetRemainingBisectRange(out PassChangeNumber, out FailChangeNumber);

			List<int> ChangeNumbers = new List<int>();
			foreach(KeyValuePair<int, BisectState> Pair in WorkspaceSettings.ChangeNumberToBisectState)
			{
				if(Pair.Value == BisectState.Include && Pair.Key > PassChangeNumber && Pair.Key < FailChangeNumber)
				{
					ChangeNumbers.Add(Pair.Key);
				}
			}

			ChangeNumbers.Sort();

			return (ChangeNumbers.Count > 0)? ChangeNumbers[ChangeNumbers.Count / 2] : -1;
		}

		private void SyncBisectChange()
		{
			int BisectChange = GetBisectChangeNumber();
			if(BisectChange != -1)
			{
				Owner.ShowAndActivate();
				SelectChange(BisectChange);
				StartSync(BisectChange);
			}
		}

		private void BuildListContextMenu_Bisect_Pass_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Pass);
		}

		private void BuildListContextMenu_Bisect_Fail_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Fail);
		}

		private void BuildListContextMenu_Bisect_Exclude_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Exclude);
		}

		private void BuildListContextMenu_Bisect_Include_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Include);
		}

		private void WorkspaceControl_VisibleChanged(object sender, EventArgs e)
		{
			if(PerforceMonitor != null && PerforceMonitor.IsActive != Visible)
			{
				PerforceMonitor.IsActive = Visible;
			}
		}

		private void FilterButton_Click(object sender, EventArgs e)
		{
			FilterContextMenu_Default.Checked = !Settings.bShowAutomatedChanges && ProjectSettings.FilterType == FilterType.None && ProjectSettings.FilterBadges.Count == 0;

			FilterContextMenu_Type.Checked = ProjectSettings.FilterType != FilterType.None;
			FilterContextMenu_Type_ShowAll.Checked = ProjectSettings.FilterType == FilterType.None;
			FilterContextMenu_Type_Code.Checked = ProjectSettings.FilterType == FilterType.Code;
			FilterContextMenu_Type_Content.Checked = ProjectSettings.FilterType == FilterType.Content;

			FilterContextMenu_Badges.DropDownItems.Clear();
			FilterContextMenu_Badges.Checked = ProjectSettings.FilterBadges.Count > 0;

			HashSet<string> BadgeNames = new HashSet<string>(ProjectSettings.FilterBadges, StringComparer.OrdinalIgnoreCase);
			BadgeNames.ExceptWith(BadgeNameAndGroupPairs.Select(x => x.Key));

			List<KeyValuePair<string, string>> DisplayBadgeNameAndGroupPairs = new List<KeyValuePair<string, string>>(BadgeNameAndGroupPairs);
			DisplayBadgeNameAndGroupPairs.AddRange(BadgeNames.Select(x => new KeyValuePair<string, string>(x, "User")));

			string LastGroup = null;
			foreach(KeyValuePair<string, string> BadgeNameAndGroupPair in DisplayBadgeNameAndGroupPairs)
			{
				if(LastGroup != BadgeNameAndGroupPair.Value)
				{
					if(LastGroup != null)
					{
						FilterContextMenu_Badges.DropDownItems.Add(new ToolStripSeparator());
					}
					LastGroup = BadgeNameAndGroupPair.Value;
				}

				ToolStripMenuItem Item = new ToolStripMenuItem(BadgeNameAndGroupPair.Key);
				Item.Checked = ProjectSettings.FilterBadges.Contains(BadgeNameAndGroupPair.Key, StringComparer.OrdinalIgnoreCase);
				Item.Click += (Sender, Args) => FilterContextMenu_Badge_Click(BadgeNameAndGroupPair.Key);
				FilterContextMenu_Badges.DropDownItems.Add(Item);
			}

			FilterContextMenu_Badges.Enabled = FilterContextMenu_Badges.DropDownItems.Count > 0;

			FilterContextMenu_ShowBuildMachineChanges.Checked = Settings.bShowAutomatedChanges;
			FilterContextMenu.Show(FilterButton, new Point(0, FilterButton.Height));
		}

		private void FilterContextMenu_Badge_Click(string BadgeName)
		{
			if(ProjectSettings.FilterBadges.Contains(BadgeName))
			{
				ProjectSettings.FilterBadges.Remove(BadgeName);
			}
			else
			{
				ProjectSettings.FilterBadges.Add(BadgeName);
			}

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Default_Click(object sender, EventArgs e)
		{
			ProjectSettings.FilterBadges.Clear();
			ProjectSettings.FilterType = FilterType.None;

			Settings.bShowAutomatedChanges = false;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_ShowBuildMachineChanges_Click(object sender, EventArgs e)
		{
			Settings.bShowAutomatedChanges ^= true;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_ShowAll_Click(object sender, EventArgs e)
		{
			ProjectSettings.FilterType = FilterType.None;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_Code_Click(object sender, EventArgs e)
		{
			ProjectSettings.FilterType = FilterType.Code;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_Content_Click(object sender, EventArgs e)
		{
			ProjectSettings.FilterType = FilterType.Content;
			Settings.Save();

			UpdateBuildListFilter();
		}

		private void UpdateBuildListFilter()
		{
			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}
	}
}
