// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Threading;
using System.Reflection;
using System.Text;
using System.Diagnostics;

namespace UnrealGameSync
{
	interface IMainWindowTabPanel : IDisposable
	{
		void Activate();
		void Deactivate();
		void Hide();
		void Show();
		bool IsBusy();
		bool CanClose();
		bool CanSyncNow();
		void SyncLatestChange();
		bool CanLaunchEditor();
		void LaunchEditor();

		Tuple<TaskbarState, float> DesiredTaskbarState
		{
			get;
		}

		UserSelectedProjectSettings SelectedProject
		{
			get;
		}
	}

	partial class MainWindow : Form, IWorkspaceControlOwner
	{
		[Flags]
		enum OpenProjectOptions
		{
			None,
			Quiet,
		}

		[DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
		static extern int SetWindowTheme(IntPtr hWnd, string pszSubAppName, string pszSubIdList);

		[DllImport("user32.dll")]
		public static extern int SendMessage(IntPtr hWnd, Int32 wMsg, Int32 wParam, Int32 lParam);

		private const int WM_SETREDRAW = 11; 

		UpdateMonitor UpdateMonitor;
		SynchronizationContext MainThreadSynchronizationContext;

		string ApiUrl;
		string DataFolder;
		string CacheFolder;
		LineBasedTextWriter Log;
		UserSettings Settings;
		int TabMenu_TabIdx = -1;
		int ChangingWorkspacesRefCount;

		bool bAllowClose = false;

		bool bRestoreStateOnLoad;

		System.Threading.Timer ScheduleTimer;
		System.Threading.Timer ScheduleSettledTimer;

		string OriginalExecutableFileName;
		bool bUnstable;

		IMainWindowTabPanel CurrentTabPanel;

		AutomationServer AutomationServer;
		TextWriter AutomationLog;

		bool bAllowCreatingHandle;

		public MainWindow(UpdateMonitor InUpdateMonitor, string InApiUrl, string InDataFolder, string InCacheFolder, bool bInRestoreStateOnLoad, string InOriginalExecutableFileName, bool bInUnstable, DetectProjectSettingsResult[] StartupProjects, LineBasedTextWriter InLog, UserSettings InSettings)
		{
			InitializeComponent();

			UpdateMonitor = InUpdateMonitor;
			MainThreadSynchronizationContext = SynchronizationContext.Current;
			ApiUrl = InApiUrl;
			DataFolder = InDataFolder;
			CacheFolder = InCacheFolder;
			bRestoreStateOnLoad = bInRestoreStateOnLoad;
			OriginalExecutableFileName = InOriginalExecutableFileName;
			bUnstable = bInUnstable;
			Log = InLog;
			Settings = InSettings;

			// While creating tab controls during startup, we need to prevent layout calls resulting in the window handle being created too early. Disable layout calls here.
			SuspendLayout();
			TabPanel.SuspendLayout();

			TabControl.OnTabChanged += TabControl_OnTabChanged;
			TabControl.OnNewTabClick += TabControl_OnNewTabClick;
			TabControl.OnTabClicked += TabControl_OnTabClicked;
			TabControl.OnTabClosing += TabControl_OnTabClosing;
			TabControl.OnTabClosed += TabControl_OnTabClosed;
			TabControl.OnTabReorder += TabControl_OnTabReorder;
			TabControl.OnButtonClick += TabControl_OnButtonClick;

			SetupDefaultControl();

			int SelectTabIdx = -1;
			foreach(DetectProjectSettingsResult StartupProject in StartupProjects)
			{
				int TabIdx = -1;
				if(StartupProject.bSucceeded)
				{
					TabIdx = TryOpenProject(StartupProject.Task, -1, OpenProjectOptions.Quiet);
				}
				else if(StartupProject.ErrorMessage != null)
				{
					CreateErrorPanel(-1, StartupProject.Task.SelectedProject, StartupProject.ErrorMessage);
				}

				if (TabIdx != -1 && Settings.LastProject != null && StartupProject.Task.SelectedProject.Equals(Settings.LastProject))
				{
					SelectTabIdx = TabIdx;
				}
			}

			if(SelectTabIdx != -1)
			{
				TabControl.SelectTab(SelectTabIdx);
			}
			else if(TabControl.GetTabCount() > 0)
			{
				TabControl.SelectTab(0);
			}

			StartScheduleTimer();

			if(bUnstable)
			{
				Text += String.Format(" (UNSTABLE BUILD {0})", Assembly.GetExecutingAssembly().GetName().Version);
			}

			AutomationLog = new TimestampLogWriter(new BoundedLogWriter(Path.Combine(DataFolder, "Automation.log")));
			AutomationServer = new AutomationServer(Request => { MainThreadSynchronizationContext.Post(Obj => PostAutomationRequest(Request), null); }, AutomationLog);

			// Allow creating controls from now on
			TabPanel.ResumeLayout(false);
			ResumeLayout(false);

			bAllowCreatingHandle = true;
		}

		void PostAutomationRequest(AutomationRequest Request)
		{
			try
			{
				if(!CanFocus)
				{
					Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Busy));
				}
				else if(Request.Input.Type == AutomationRequestType.SyncProject)
				{
					AutomationRequestOutput Output = StartAutomatedSync(Request, true);
					if(Output != null)
					{
						Request.SetOutput(Output);
					}
				}
				else if(Request.Input.Type == AutomationRequestType.FindProject)
				{
					AutomationRequestOutput Output = FindProject(Request);
					Request.SetOutput(Output);
				}
				else if(Request.Input.Type == AutomationRequestType.OpenProject)
				{
					AutomationRequestOutput Output = StartAutomatedSync(Request, false);
					if(Output != null)
					{
						Request.SetOutput(Output);
					}
				}
				else
				{
					Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Invalid));
				}
			}
			catch(Exception Ex)
			{
				Log.WriteLine("Exception running automation request: {0}", Ex);
				Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Invalid));
			}
		}

		AutomationRequestOutput StartAutomatedSync(AutomationRequest Request, bool bForceSync)
		{
			ShowAndActivate();

			BinaryReader Reader = new BinaryReader(new MemoryStream(Request.Input.Data));
			string StreamName = Reader.ReadString();
			string ProjectPath = Reader.ReadString();

			AutomatedSyncWindow.WorkspaceInfo WorkspaceInfo;
			if(!AutomatedSyncWindow.ShowModal(this, StreamName, ProjectPath, out WorkspaceInfo, Log))
			{
				return new AutomationRequestOutput(AutomationRequestResult.Canceled);
			}

			if(WorkspaceInfo.bRequiresStreamSwitch)
			{
				// Close any tab containing this window
				for(int ExistingTabIdx = 0; ExistingTabIdx < TabControl.GetTabCount(); ExistingTabIdx++)
				{
					WorkspaceControl ExistingWorkspace = TabControl.GetTabData(ExistingTabIdx) as WorkspaceControl;
					if(ExistingWorkspace != null && ExistingWorkspace.ClientName.Equals(WorkspaceInfo.WorkspaceName))
					{
						TabControl.RemoveTab(ExistingTabIdx);
						break;
					}
				}

				// Switch the stream
				PerforceConnection Perforce = new PerforceConnection(WorkspaceInfo.UserName, WorkspaceInfo.WorkspaceName, WorkspaceInfo.ServerAndPort);
				if(!Perforce.SwitchStream(StreamName, Log))
				{
					Log.WriteLine("Unable to switch stream");
					return new AutomationRequestOutput(AutomationRequestResult.Error);
				}
			}

			UserSelectedProjectSettings SelectedProject = new UserSelectedProjectSettings(WorkspaceInfo.ServerAndPort, WorkspaceInfo.UserName, UserSelectedProjectType.Client, String.Format("//{0}{1}", WorkspaceInfo.WorkspaceName, ProjectPath), null);

			int TabIdx = TryOpenProject(SelectedProject, -1, OpenProjectOptions.None);
			if(TabIdx == -1)
			{
				Log.WriteLine("Unable to open project");
				return new AutomationRequestOutput(AutomationRequestResult.Error);
			}

			WorkspaceControl Workspace = TabControl.GetTabData(TabIdx) as WorkspaceControl;
			if(Workspace == null)
			{
				Log.WriteLine("Workspace was unable to open");
				return new AutomationRequestOutput(AutomationRequestResult.Error);
			}

			if(!bForceSync && Workspace.CanLaunchEditor())
			{
				return new AutomationRequestOutput(AutomationRequestResult.Ok, Encoding.UTF8.GetBytes(Workspace.SelectedFileName));
			}

			Workspace.AddStartupCallback((Control, bCancel) => StartAutomatedSyncAfterStartup(Control, bCancel, Request));
			return null;
		}

		private void StartAutomatedSyncAfterStartup(WorkspaceControl Workspace, bool bCancel, AutomationRequest Request)
		{
			if(bCancel)
			{
				Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Canceled));
			}
			else
			{
				Workspace.SyncLatestChange(Result => CompleteAutomatedSync(Result, Workspace.SelectedFileName, Request));
			}
		}

		void CompleteAutomatedSync(WorkspaceUpdateResult Result, string SelectedFileName, AutomationRequest Request)
		{
			if(Result == WorkspaceUpdateResult.Success)
			{
				Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Ok, Encoding.UTF8.GetBytes(SelectedFileName)));
			}
			else if(Result == WorkspaceUpdateResult.Canceled)
			{
				Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Canceled));
			}
			else
			{
				Request.SetOutput(new AutomationRequestOutput(AutomationRequestResult.Error));
			}
		}

		AutomationRequestOutput FindProject(AutomationRequest Request)
		{
			BinaryReader Reader = new BinaryReader(new MemoryStream(Request.Input.Data));
			string StreamName = Reader.ReadString();
			string ProjectPath = Reader.ReadString();

			for(int ExistingTabIdx = 0; ExistingTabIdx < TabControl.GetTabCount(); ExistingTabIdx++)
			{
				WorkspaceControl ExistingWorkspace = TabControl.GetTabData(ExistingTabIdx) as WorkspaceControl;
				if(ExistingWorkspace != null && String.Compare(ExistingWorkspace.StreamName, StreamName, StringComparison.OrdinalIgnoreCase) == 0 && ExistingWorkspace.SelectedProject != null)
				{
					string ClientPath = ExistingWorkspace.SelectedProject.ClientPath;
					if(ClientPath != null && ClientPath.StartsWith("//"))
					{
						int SlashIdx = ClientPath.IndexOf('/', 2);
						if(SlashIdx != -1)
						{
							string ExistingProjectPath = ClientPath.Substring(SlashIdx);
							if(String.Compare(ExistingProjectPath, ProjectPath, StringComparison.OrdinalIgnoreCase) == 0)
							{
								return new AutomationRequestOutput(AutomationRequestResult.Ok, Encoding.UTF8.GetBytes(ExistingWorkspace.SelectedFileName));
							}
						}
					}
				}
			}

			return new AutomationRequestOutput(AutomationRequestResult.NotFound);
		}

		protected override void OnHandleCreated(EventArgs e)
		{
			base.OnHandleCreated(e);

			Debug.Assert(bAllowCreatingHandle, "Window handle should not be created before constructor has run.");
		}

		void TabControl_OnButtonClick(int ButtonIdx, Point Location, MouseButtons Buttons)
		{
			if(ButtonIdx == 0)
			{
				EditSelectedProject(TabControl.GetSelectedTabIndex());
			}
		}

		void TabControl_OnTabClicked(object TabData, Point Location, MouseButtons Buttons)
		{
			if(Buttons == System.Windows.Forms.MouseButtons.Right)
			{
				Activate();

				int InsertIdx = 0;

				while(TabMenu_RecentProjects.DropDownItems[InsertIdx] != TabMenu_Recent_Separator)
				{
					TabMenu_RecentProjects.DropDownItems.RemoveAt(InsertIdx);
				}

				TabMenu_TabIdx = -1;
				for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
				{
					if(TabControl.GetTabData(Idx) == TabData)
					{
						TabMenu_TabIdx = Idx;
						break;
					}
				}

				foreach(UserSelectedProjectSettings RecentProject in Settings.RecentProjects)
				{
					ToolStripMenuItem Item = new ToolStripMenuItem(RecentProject.ToString(), null, new EventHandler((o, e) => TryOpenProject(RecentProject, TabMenu_TabIdx)));
					TabMenu_RecentProjects.DropDownItems.Insert(InsertIdx, Item);
					InsertIdx++;
				}

				TabMenu_RecentProjects.Visible = (Settings.RecentProjects.Count > 0);

				TabMenu_TabNames_Stream.Checked = Settings.TabLabels == TabLabels.Stream;
				TabMenu_TabNames_WorkspaceName.Checked = Settings.TabLabels == TabLabels.WorkspaceName;
				TabMenu_TabNames_WorkspaceRoot.Checked = Settings.TabLabels == TabLabels.WorkspaceRoot;
				TabMenu_TabNames_ProjectFile.Checked = Settings.TabLabels == TabLabels.ProjectFile;
				TabMenu.Show(TabControl, Location);

				TabControl.LockHover();
			}
		}

		void TabControl_OnTabReorder()
		{
			SaveTabSettings();
		}

		void TabControl_OnTabClosed(object Data)
		{
			IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)Data;
			if(CurrentTabPanel == TabPanel)
			{
				CurrentTabPanel = null;
			}
			TabPanel.Dispose();

			SaveTabSettings();
		}

		bool TabControl_OnTabClosing(object TabData)
		{
			IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabData;
			return TabPanel.CanClose();
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}

			for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
			{
				((IMainWindowTabPanel)TabControl.GetTabData(Idx)).Dispose();
			}

			StopScheduleTimer();

			if(AutomationServer != null)
			{
				AutomationServer.Dispose();
				AutomationServer = null;
			}

			if(AutomationLog != null)
			{
				AutomationLog.Close();
				AutomationLog = null;
			}

			base.Dispose(disposing);
		}

		private void MainWindow_FormClosing(object Sender, FormClosingEventArgs EventArgs)
		{
			if(!bAllowClose && Settings.bKeepInTray)
			{
				Hide();
				EventArgs.Cancel = true;
			}
			else
			{
				for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
				{
					IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabControl.GetTabData(Idx);
					if(!TabPanel.CanClose())
					{
						EventArgs.Cancel = true;
						return;
					}
				}

				StopScheduleTimer();
			}

			Settings.bWindowVisible = Visible;
			Settings.WindowState = WindowState;
			if(WindowState == FormWindowState.Normal)
			{
				Settings.WindowBounds = new Rectangle(Location, Size);
			}
			else
			{
				Settings.WindowBounds = RestoreBounds;
			}

			Settings.Save();
		}

		private void SetupDefaultControl()
		{
			List<StatusLine> Lines = new List<StatusLine>();

			StatusLine SummaryLine = new StatusLine();
			SummaryLine.AddText("To get started, open an existing Unreal project file on your hard drive.");
			Lines.Add(SummaryLine);

			StatusLine OpenLine = new StatusLine();
			OpenLine.AddLink("Open project...", FontStyle.Bold | FontStyle.Underline, () => { OpenNewProject(); });
			Lines.Add(OpenLine);

			DefaultControl.Set(Lines, null, null, null);
		}

		private void CreateErrorPanel(int ReplaceTabIdx, UserSelectedProjectSettings Project, string Message)
		{
			Log.WriteLine(Message ?? "Unknown error");

			ErrorPanel ErrorPanel = new ErrorPanel(Project);
			ErrorPanel.Parent = TabPanel;
			ErrorPanel.BorderStyle = BorderStyle.FixedSingle;
			ErrorPanel.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(250)))), ((int)(((byte)(250)))), ((int)(((byte)(250)))));
			ErrorPanel.Location = new Point(0, 0);
			ErrorPanel.Dock = DockStyle.Fill;
			ErrorPanel.Hide();

			string SummaryText = String.Format("Unable to open '{0}'.", Project.ToString());

			int NewContentWidth = Math.Max(TextRenderer.MeasureText(SummaryText, ErrorPanel.Font).Width, 400);
			if(!String.IsNullOrEmpty(Message))
			{
				NewContentWidth = Math.Max(NewContentWidth, TextRenderer.MeasureText(Message, ErrorPanel.Font).Width);
			}

			ErrorPanel.SetContentWidth(NewContentWidth);

			List<StatusLine> Lines = new List<StatusLine>();

			StatusLine SummaryLine = new StatusLine();
			SummaryLine.AddText(SummaryText);
			Lines.Add(SummaryLine);

			if(!String.IsNullOrEmpty(Message))
			{
				Lines.Add(new StatusLine(){ LineHeight = 0.5f });

				foreach(string MessageLine in Message.Split('\n'))
				{
					StatusLine ErrorLine = new StatusLine();
					ErrorLine.AddText(MessageLine);
					ErrorLine.LineHeight = 0.8f;
					Lines.Add(ErrorLine);
				}
			}

			Lines.Add(new StatusLine(){ LineHeight = 0.5f });

			StatusLine ActionLine = new StatusLine();
			ActionLine.AddLink("Retry", FontStyle.Bold | FontStyle.Underline, () => { BeginInvoke(new MethodInvoker(() => { TryOpenProject(Project, TabControl.FindTabIndex(ErrorPanel)); })); });
			ActionLine.AddText(" | ");
			ActionLine.AddLink("Settings", FontStyle.Bold | FontStyle.Underline, () => { BeginInvoke(new MethodInvoker(() => { EditSelectedProject(ErrorPanel); })); });
			ActionLine.AddText(" | ");
			ActionLine.AddLink("Close", FontStyle.Bold | FontStyle.Underline, () => { BeginInvoke(new MethodInvoker(() => { TabControl.RemoveTab(TabControl.FindTabIndex(ErrorPanel)); })); });
			Lines.Add(ActionLine);

			ErrorPanel.Set(Lines, null, null, null);

			string NewProjectName = "Unknown";
			if(Project.Type == UserSelectedProjectType.Client && Project.ClientPath != null)
			{
				NewProjectName = Project.ClientPath.Substring(Project.ClientPath.LastIndexOf('/') + 1);
			}
			if(Project.Type == UserSelectedProjectType.Local && Project.LocalPath != null)
			{
				NewProjectName = Project.LocalPath.Substring(Project.LocalPath.LastIndexOfAny(new char[]{ '/', '\\' }) + 1);
			}

			string NewTabName = String.Format("Error: {0}", NewProjectName);
			if (ReplaceTabIdx == -1)
			{
				int TabIdx = TabControl.InsertTab(-1, NewTabName, ErrorPanel);
				TabControl.SelectTab(TabIdx);
			}
			else
			{
				TabControl.InsertTab(ReplaceTabIdx + 1, NewTabName, ErrorPanel);
				TabControl.RemoveTab(ReplaceTabIdx);
				TabControl.SelectTab(ReplaceTabIdx);
			}

			UpdateProgress();
		}

		public void ShowAndActivate()
		{
			Show();
			if(WindowState == FormWindowState.Minimized)
			{
				WindowState = FormWindowState.Normal;
			}
			Activate();

			Settings.bWindowVisible = Visible;
			Settings.Save();
		}

		public bool CanPerformUpdate()
		{
			if(ContainsFocus || Form.ActiveForm == this)
			{
				return false;
			}

			for (int TabIdx = 0; TabIdx < TabControl.GetTabCount(); TabIdx++)
			{
				IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabControl.GetTabData(TabIdx);
				if(TabPanel.IsBusy())
				{
					return false;
				}
			}

			return true;
		}

		public bool CanSyncNow()
		{
			return CurrentTabPanel != null && CurrentTabPanel.CanSyncNow();
		}

		public bool CanLaunchEditor()
		{
			return CurrentTabPanel != null && CurrentTabPanel.CanLaunchEditor();
		}

		public void SyncLatestChange()
		{
			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.SyncLatestChange();
			}
		}

		public void LaunchEditor()
		{
			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.LaunchEditor();
			}
		}

		public void ForceClose()
		{
			bAllowClose = true;
			Close();
		}

		private void MainWindow_Activated(object sender, EventArgs e)
		{
			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.Activate();
			}
		}

		private void MainWindow_Deactivate(object sender, EventArgs e)
		{
			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.Deactivate();
			}
		}

		public void SetupScheduledSync()
		{
			StopScheduleTimer();

			List<UserSelectedProjectSettings> OpenProjects = new List<UserSelectedProjectSettings>();
			for(int TabIdx = 0; TabIdx < TabControl.GetTabCount(); TabIdx++)
			{
				IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabControl.GetTabData(TabIdx);
				OpenProjects.Add(TabPanel.SelectedProject);
			}

			ScheduleWindow Schedule = new ScheduleWindow(Settings.bScheduleEnabled, Settings.ScheduleChange, Settings.ScheduleTime, Settings.ScheduleAnyOpenProject, Settings.ScheduleProjects, OpenProjects);
			if(Schedule.ShowDialog() == System.Windows.Forms.DialogResult.OK)
			{
				Schedule.CopySettings(out Settings.bScheduleEnabled, out Settings.ScheduleChange, out Settings.ScheduleTime, out Settings.ScheduleAnyOpenProject, out Settings.ScheduleProjects);
				Settings.Save();
			}

			StartScheduleTimer();
		}

		private void StartScheduleTimer()
		{
			StopScheduleTimer();

			if(Settings.bScheduleEnabled)
			{
				DateTime CurrentTime = DateTime.Now;
				DateTime NextScheduleTime = new DateTime(CurrentTime.Year, CurrentTime.Month, CurrentTime.Day, Settings.ScheduleTime.Hours, Settings.ScheduleTime.Minutes, Settings.ScheduleTime.Seconds);

				if(NextScheduleTime < CurrentTime)
				{
					NextScheduleTime = NextScheduleTime.AddDays(1.0);
				}

				TimeSpan IntervalToFirstTick = NextScheduleTime - CurrentTime;
				ScheduleTimer = new System.Threading.Timer(x => MainThreadSynchronizationContext.Post((o) => { if(!IsDisposed){ ScheduleTimerElapsed(); } }, null), null, IntervalToFirstTick, TimeSpan.FromDays(1));

				Log.WriteLine("Schedule: Started ScheduleTimer for {0} ({1} remaining)", NextScheduleTime, IntervalToFirstTick);
			}
		}

		private void StopScheduleTimer()
		{
			if(ScheduleTimer != null)
			{
				ScheduleTimer.Dispose();
				ScheduleTimer = null;
				Log.WriteLine("Schedule: Stopped ScheduleTimer");
			}
			StopScheduleSettledTimer();
		}

		private void ScheduleTimerElapsed()
		{
			Log.WriteLine("Schedule: Timer Elapsed");

			// Try to open any missing tabs. 
			int NumInitialTabs = TabControl.GetTabCount();
			foreach (UserSelectedProjectSettings ScheduledProject in Settings.ScheduleProjects)
			{
				Log.WriteLine("Schedule: Attempting to open {0}", ScheduledProject);
				TryOpenProject(ScheduledProject, -1, OpenProjectOptions.Quiet);
			}

			// If we did open something, leave it for a while to populate with data before trying to start the sync.
			if(TabControl.GetTabCount() > NumInitialTabs)
			{
				StartScheduleSettledTimer();
			}
			else
			{
				ScheduleSettledTimerElapsed();
			}
		}

		private void StartScheduleSettledTimer()
		{
			StopScheduleSettledTimer();
			ScheduleSettledTimer = new System.Threading.Timer(x => MainThreadSynchronizationContext.Post((o) => { if(!IsDisposed){ ScheduleSettledTimerElapsed(); } }, null), null, TimeSpan.FromSeconds(20.0), TimeSpan.FromMilliseconds(-1.0));
			Log.WriteLine("Schedule: Started ScheduleSettledTimer");
		}

		private void StopScheduleSettledTimer()
		{
			if(ScheduleSettledTimer != null)
			{
				ScheduleSettledTimer.Dispose();
				ScheduleSettledTimer = null;

				Log.WriteLine("Schedule: Stopped ScheduleSettledTimer");
			}
		}

		private void ScheduleSettledTimerElapsed()
		{
			Log.WriteLine("Schedule: Starting Sync");
			for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
			{
				WorkspaceControl Workspace = TabControl.GetTabData(Idx) as WorkspaceControl;
				if(Workspace != null)
				{
					Log.WriteLine("Schedule: Considering {0}", Workspace.SelectedFileName);
					if(Settings.ScheduleAnyOpenProject || Settings.ScheduleProjects.Contains(Workspace.SelectedProject))
					{
						Log.WriteLine("Schedule: Starting Sync");
						Workspace.ScheduleTimerElapsed();
					}
				}
			}
		}

		void TabControl_OnTabChanged(object NewTabData)
		{
			if(IsHandleCreated)
			{
				SendMessage(Handle, WM_SETREDRAW, 0, 0);
			}

			SuspendLayout();

			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.Deactivate();
				CurrentTabPanel.Hide();
			}

			if(NewTabData == null)
			{
				CurrentTabPanel = null;
				Settings.LastProject = null;
				DefaultControl.Show();
			}
			else
			{
				CurrentTabPanel = (IMainWindowTabPanel)NewTabData;
				Settings.LastProject = CurrentTabPanel.SelectedProject;
				DefaultControl.Hide();
			}

			Settings.Save();

			if(CurrentTabPanel != null)
			{
				CurrentTabPanel.Activate();
				CurrentTabPanel.Show();
			}

			ResumeLayout();

			if(IsHandleCreated)
			{
				SendMessage(Handle, WM_SETREDRAW, 1, 0);
			}

			Refresh();
		}

		public void RequestProjectChange(WorkspaceControl Workspace, UserSelectedProjectSettings Project, bool bModal)
		{
			int TabIdx = TabControl.FindTabIndex(Workspace);
			if(TabIdx != -1 && !Workspace.IsBusy() && CanFocus)
			{
				if(bModal)
				{
					TryOpenProject(Project, TabIdx);
				}
				else
				{
					TryOpenProject(Project, TabIdx, OpenProjectOptions.Quiet);
				}
			}
		}

		public void OpenNewProject()
		{
			DetectProjectSettingsTask DetectedProjectSettings;
			if(OpenProjectWindow.ShowModal(this, null, out DetectedProjectSettings, Settings, DataFolder, CacheFolder, Log))
			{
				int NewTabIdx = TryOpenProject(DetectedProjectSettings, -1, OpenProjectOptions.None);
				if(NewTabIdx != -1)
				{
					TabControl.SelectTab(NewTabIdx);
					SaveTabSettings();

					Settings.RecentProjects.RemoveAll(x => x.LocalPath == DetectedProjectSettings.NewSelectedFileName);
					Settings.RecentProjects.Insert(0, DetectedProjectSettings.SelectedProject);
					Settings.Save();
				}
				DetectedProjectSettings.Dispose();
			}
		}

		public void EditSelectedProject(int TabIdx)
		{
			object TabData = TabControl.GetTabData(TabIdx);
			if(TabData is WorkspaceControl)
			{
				WorkspaceControl Workspace = (WorkspaceControl)TabData;
				EditSelectedProject(TabIdx, Workspace.SelectedProject);
			}
			else if(TabData is ErrorPanel)
			{
				ErrorPanel Error = (ErrorPanel)TabData;
				EditSelectedProject(TabIdx, Error.SelectedProject);
			}
		}

		public void EditSelectedProject(WorkspaceControl Workspace)
		{
			int TabIdx = TabControl.FindTabIndex(Workspace);
			if(TabIdx != -1)
			{
				EditSelectedProject(TabIdx, Workspace.SelectedProject);
			}
		}

		public void EditSelectedProject(ErrorPanel Panel)
		{
			int TabIdx = TabControl.FindTabIndex(Panel);
			if(TabIdx != -1)
			{
				EditSelectedProject(TabIdx, Panel.SelectedProject);
			}
		}

		public void EditSelectedProject(int TabIdx, UserSelectedProjectSettings SelectedProject)
		{
			DetectProjectSettingsTask DetectedProjectSettings;
			if(OpenProjectWindow.ShowModal(this, SelectedProject, out DetectedProjectSettings, Settings, DataFolder, CacheFolder, Log))
			{
				int NewTabIdx = TryOpenProject(DetectedProjectSettings, TabIdx, OpenProjectOptions.None);
				if(NewTabIdx != -1)
				{
					TabControl.SelectTab(NewTabIdx);
					SaveTabSettings();

					Settings.RecentProjects.RemoveAll(x => x.LocalPath == DetectedProjectSettings.NewSelectedFileName);
					Settings.RecentProjects.Insert(0, DetectedProjectSettings.SelectedProject);
					Settings.Save();
				}
			}
		}

		int TryOpenProject(UserSelectedProjectSettings Project, int ReplaceTabIdx, OpenProjectOptions Options = OpenProjectOptions.None)
		{
			Log.WriteLine("Detecting settings for {0}", Project);
			using(DetectProjectSettingsTask DetectProjectSettings = new DetectProjectSettingsTask(Project, DataFolder, CacheFolder, new PrefixedTextWriter("  ", Log)))
			{
				string ErrorMessage;

				ModalTaskResult Result;
				if((Options & OpenProjectOptions.Quiet) != 0)
				{
					Result = ModalTask.Execute(this, DetectProjectSettings, "Opening Project", "Opening project, please wait...", out ErrorMessage);
				}
				else
				{
					Result = PerforceModalTask.Execute(this, Project.LocalPath, Project.ServerAndPort, Project.UserName, DetectProjectSettings, "Opening Project", "Opening project, please wait...", Log, out ErrorMessage);
				}

				if(Result != ModalTaskResult.Succeeded)
				{
					CreateErrorPanel(ReplaceTabIdx, Project, ErrorMessage);
					return -1;
				}
				return TryOpenProject(DetectProjectSettings, ReplaceTabIdx, Options);
			}
		}

		int TryOpenProject(DetectProjectSettingsTask ProjectSettings, int ReplaceTabIdx, OpenProjectOptions Options)
		{
			Log.WriteLine("Trying to open project {0}", ProjectSettings.SelectedProject.ToString());

			// Check that none of the other tabs already have it open
			for(int TabIdx = 0; TabIdx < TabControl.GetTabCount(); TabIdx++)
			{
				if(ReplaceTabIdx != TabIdx)
				{
					WorkspaceControl Workspace = TabControl.GetTabData(TabIdx) as WorkspaceControl;
					if(Workspace != null)
					{
						if(Workspace.SelectedFileName.Equals(ProjectSettings.NewSelectedFileName, StringComparison.InvariantCultureIgnoreCase))
						{
							Log.WriteLine("  Already open in tab {0}", TabIdx);
							if((Options & OpenProjectOptions.Quiet) == 0)
							{
								TabControl.SelectTab(TabIdx);
							}
							return TabIdx;
						}
						else if(ProjectSettings.NewSelectedFileName.StartsWith(Workspace.BranchDirectoryName + Path.DirectorySeparatorChar, StringComparison.InvariantCultureIgnoreCase))
						{
							if((Options & OpenProjectOptions.Quiet) == 0 && MessageBox.Show(String.Format("{0} is already open under {1}.\n\nWould you like to close it?", Path.GetFileNameWithoutExtension(Workspace.SelectedFileName), Workspace.BranchDirectoryName, Path.GetFileNameWithoutExtension(ProjectSettings.NewSelectedFileName)), "Branch already open", MessageBoxButtons.YesNo) == System.Windows.Forms.DialogResult.Yes)
							{
								Log.WriteLine("  Another project already open in this workspace, tab {0}. Replacing.", TabIdx);
								TabControl.RemoveTab(TabIdx);
							}
							else
							{
								Log.WriteLine("  Another project already open in this workspace, tab {0}. Aborting.", TabIdx);
								return -1;
							}
						}
					}
				}
			}

			// Hide the default control if it's visible
			DefaultControl.Hide();

			// Remove the current tab. We need to ensure the workspace has been shut down before creating a new one with the same log files, etc...
			if(ReplaceTabIdx != -1)
			{
				WorkspaceControl OldWorkspace = TabControl.GetTabData(ReplaceTabIdx) as WorkspaceControl;
				if(OldWorkspace != null)
				{
					OldWorkspace.Hide();
					TabControl.SetTabData(ReplaceTabIdx, new ErrorPanel(ProjectSettings.SelectedProject));
					OldWorkspace.Dispose();
				}
			}

			// Now that we have the project settings, we can construct the tab
			WorkspaceControl NewWorkspace = new WorkspaceControl(this, ApiUrl, OriginalExecutableFileName, bUnstable, ProjectSettings, Log, Settings);
			NewWorkspace.Parent = TabPanel;
			NewWorkspace.Dock = DockStyle.Fill;
			NewWorkspace.Hide();

			// Add the tab
			string NewTabName = GetTabName(NewWorkspace);
			if(ReplaceTabIdx == -1)
			{
				int NewTabIdx = TabControl.InsertTab(-1, NewTabName, NewWorkspace);
				Log.WriteLine("  Inserted tab {0}", NewTabIdx);
				return NewTabIdx;
			}
			else
			{
				Log.WriteLine("  Replacing tab {0}", ReplaceTabIdx);
				TabControl.InsertTab(ReplaceTabIdx + 1, NewTabName, NewWorkspace);
				TabControl.RemoveTab(ReplaceTabIdx);
				return ReplaceTabIdx;
			}
		}

		public void StreamChanged(WorkspaceControl Workspace)
		{
			MainThreadSynchronizationContext.Post((o) => { if(!IsDisposed) { StreamChangedCallback(Workspace); } }, null);
		}

		public void StreamChangedCallback(WorkspaceControl Workspace)
		{
			if(ChangingWorkspacesRefCount == 0)
			{
				ChangingWorkspacesRefCount++;

				for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
				{
					if(TabControl.GetTabData(Idx) == Workspace)
					{
						UserSelectedProjectSettings Project = Workspace.SelectedProject;
						if(TryOpenProject(Project, Idx) == -1)
						{
							TabControl.RemoveTab(Idx);
						}
						break;
					}
				}

				ChangingWorkspacesRefCount--;
			}
		}

		void SaveTabSettings()
		{
			Settings.OpenProjects.Clear();
			for(int TabIdx = 0; TabIdx < TabControl.GetTabCount(); TabIdx++)
			{
				IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabControl.GetTabData(TabIdx);
				Settings.OpenProjects.Add(TabPanel.SelectedProject);
			}
			Settings.Save();
		}

		void TabControl_OnNewTabClick(Point Location, MouseButtons Buttons)
		{
			if(Buttons == MouseButtons.Left)
			{
				OpenNewProject();
			}
		}

		string GetTabName(WorkspaceControl Workspace)
		{
			switch(Settings.TabLabels)
			{
				case TabLabels.Stream:
					return Workspace.StreamName;
				case TabLabels.ProjectFile:
					return Workspace.SelectedFileName;
				case TabLabels.WorkspaceName:
					return Workspace.ClientName;
				case TabLabels.WorkspaceRoot:
				default:
					return Workspace.BranchDirectoryName;
			}
		}

		public void SetTabNames(TabLabels NewTabNames)
		{
			if(Settings.TabLabels != NewTabNames)
			{
				Settings.TabLabels = NewTabNames;
				Settings.Save();

				for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
				{
					WorkspaceControl Workspace = TabControl.GetTabData(Idx) as WorkspaceControl;
					if(Workspace != null)
					{
						TabControl.SetTabName(Idx, GetTabName(Workspace));
					}
				}
			}
		}

		private void TabMenu_OpenProject_Click(object sender, EventArgs e)
		{
			EditSelectedProject(TabMenu_TabIdx);
		}

		private void TabMenu_TabNames_Stream_Click(object sender, EventArgs e)
		{
			SetTabNames(TabLabels.Stream);
		}

		private void TabMenu_TabNames_WorkspaceName_Click(object sender, EventArgs e)
		{
			SetTabNames(TabLabels.WorkspaceName);
		}

		private void TabMenu_TabNames_WorkspaceRoot_Click(object sender, EventArgs e)
		{
			SetTabNames(TabLabels.WorkspaceRoot);
		}

		private void TabMenu_TabNames_ProjectFile_Click(object sender, EventArgs e)
		{
			SetTabNames(TabLabels.ProjectFile);
		}

		private void TabMenu_RecentProjects_ClearList_Click(object sender, EventArgs e)
		{
			Settings.RecentProjects.Clear();
			Settings.Save();
		}

		private void TabMenu_Closed(object sender, ToolStripDropDownClosedEventArgs e)
		{
			TabControl.UnlockHover();
		}

		private void RecentMenu_ClearList_Click(object sender, EventArgs e)
		{
			Settings.RecentProjects.Clear();
			Settings.Save();
		}

		public void UpdateProgress()
		{
			TaskbarState State = TaskbarState.NoProgress;
			float Progress = -1.0f;

			for(int Idx = 0; Idx < TabControl.GetTabCount(); Idx++)
			{
				IMainWindowTabPanel TabPanel = (IMainWindowTabPanel)TabControl.GetTabData(Idx);

				Tuple<TaskbarState, float> DesiredTaskbarState = TabPanel.DesiredTaskbarState;
				if(DesiredTaskbarState.Item1 == TaskbarState.Error)
				{
					State = TaskbarState.Error;
					TabControl.SetHighlight(Idx, Tuple.Create(Color.FromArgb(204, 64, 64), 1.0f));
				}
				else if(DesiredTaskbarState.Item1 == TaskbarState.Paused && State != TaskbarState.Error)
				{
					State = TaskbarState.Paused;
					TabControl.SetHighlight(Idx, Tuple.Create(Color.FromArgb(255, 242, 0), 1.0f));
				}
				else if(DesiredTaskbarState.Item1 == TaskbarState.Normal && State != TaskbarState.Error && State != TaskbarState.Paused)
				{
					State = TaskbarState.Normal;
					Progress = Math.Max(Progress, DesiredTaskbarState.Item2);
					TabControl.SetHighlight(Idx, Tuple.Create(Color.FromArgb(28, 180, 64), DesiredTaskbarState.Item2));
				}
				else
				{
					TabControl.SetHighlight(Idx, null);
				}
			}

			if(IsHandleCreated)
			{
				if(State == TaskbarState.Normal)
				{
					Taskbar.SetState(Handle, TaskbarState.Normal);
					Taskbar.SetProgress(Handle, (ulong)(Progress * 1000.0f), 1000);
				}
				else
				{
					Taskbar.SetState(Handle, State);
				}
			}
		}

		public void ModifyApplicationSettings()
		{
			bool? bRelaunchUnstable = ApplicationSettingsWindow.ShowModal(this, bUnstable, OriginalExecutableFileName, Settings, Log);
			if(bRelaunchUnstable.HasValue)
			{
				UpdateMonitor.TriggerUpdate(UpdateType.UserInitiated, bRelaunchUnstable);
			}
		}

		private void MainWindow_Load(object sender, EventArgs e)
		{
			if(Settings.WindowBounds != null)
			{
				Rectangle WindowBounds = Settings.WindowBounds.Value;
				if(WindowBounds.Width > MinimumSize.Width && WindowBounds.Height > MinimumSize.Height)
				{
					foreach (Screen Screen in Screen.AllScreens)
					{
						if(WindowBounds.IntersectsWith(Screen.Bounds))
						{
							Location = Settings.WindowBounds.Value.Location;
							Size = Settings.WindowBounds.Value.Size;
							break;
						}
					}
				}
			}
			WindowState = Settings.WindowState;
		}
	}
}
