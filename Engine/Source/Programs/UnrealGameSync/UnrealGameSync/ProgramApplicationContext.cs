// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	class ProgramApplicationContext : ApplicationContext
	{
		SynchronizationContext MainThreadSynchronizationContext;

		UpdateMonitor UpdateMonitor;
		string ApiUrl;
		string DataFolder;
		bool bRestoreState;
		string UpdateSpawn;
		bool bUnstable;
		bool bIsClosing;

		TimestampLogWriter Log;
		UserSettings Settings;
		ActivationListener ActivationListener;

		Container Components = new Container();
		NotifyIcon NotifyIcon;
		ContextMenuStrip NotifyMenu;
		ToolStripMenuItem NotifyMenu_OpenUnrealGameSync;
		ToolStripSeparator NotifyMenu_OpenUnrealGameSync_Separator;
		ToolStripMenuItem NotifyMenu_SyncNow;
		ToolStripMenuItem NotifyMenu_LaunchEditor;
		ToolStripSeparator NotifyMenu_ExitSeparator;
		ToolStripMenuItem NotifyMenu_Exit;

		DetectMultipleProjectSettingsTask DetectStartupProjectSettingsTask;
		ModalTaskWindow DetectStartupProjectSettingsWindow;
		MainWindow MainWindowInstance;

		public ProgramApplicationContext(UpdateMonitor UpdateMonitor, string ApiUrl, string DataFolder, EventWaitHandle ActivateEvent, bool bRestoreState, string UpdateSpawn, string ProjectFileName, bool bUnstable)
		{
			this.UpdateMonitor = UpdateMonitor;
			this.ApiUrl = ApiUrl;
			this.DataFolder = DataFolder;
			this.bRestoreState = bRestoreState;
			this.UpdateSpawn = UpdateSpawn;
			this.bUnstable = bUnstable;

			// Make sure a synchronization context is set. We spawn a bunch of threads (eg. UpdateMonitor) at startup, and need to make sure we can post messages 
			// back to the main thread at any time.
			if(SynchronizationContext.Current == null)
			{
				SynchronizationContext.SetSynchronizationContext(new WindowsFormsSynchronizationContext());
			}

			// Capture the main thread's synchronization context for callbacks
			MainThreadSynchronizationContext = WindowsFormsSynchronizationContext.Current;

			// Create the log file
			Log = new TimestampLogWriter(new BoundedLogWriter(Path.Combine(DataFolder, "UnrealGameSync.log")));
			Log.WriteLine("Application version: {0}", Assembly.GetExecutingAssembly().GetName().Version);
			Log.WriteLine("Started at {0}", DateTime.Now.ToString());

			// Read the user's settings
			Settings = new UserSettings(Path.Combine(DataFolder, "UnrealGameSync.ini"));
			if(!String.IsNullOrEmpty(ProjectFileName))
			{
				string FullProjectFileName = Path.GetFullPath(ProjectFileName);
				if(!Settings.OpenProjects.Any(x => x.LocalPath != null && String.Compare(x.LocalPath, FullProjectFileName, StringComparison.InvariantCultureIgnoreCase) == 0))
				{
					Settings.OpenProjects.Add(new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, FullProjectFileName));
				}
			}

			// Register the update listener
			UpdateMonitor.OnUpdateAvailable += OnUpdateAvailableCallback;

			// Create the activation listener
			ActivationListener = new ActivationListener(ActivateEvent);
			ActivationListener.Start();
			ActivationListener.OnActivate += OnActivationListenerAsyncCallback;

			// Create the notification menu items
			NotifyMenu_OpenUnrealGameSync = new ToolStripMenuItem();
			NotifyMenu_OpenUnrealGameSync.Name = nameof(NotifyMenu_OpenUnrealGameSync);
			NotifyMenu_OpenUnrealGameSync.Size = new Size(196, 22);
			NotifyMenu_OpenUnrealGameSync.Text = "Open UnrealGameSync";
			NotifyMenu_OpenUnrealGameSync.Click += new EventHandler(NotifyMenu_OpenUnrealGameSync_Click);
			NotifyMenu_OpenUnrealGameSync.Font = new Font(NotifyMenu_OpenUnrealGameSync.Font, FontStyle.Bold);

			NotifyMenu_OpenUnrealGameSync_Separator = new ToolStripSeparator();
			NotifyMenu_OpenUnrealGameSync_Separator.Name = nameof(NotifyMenu_OpenUnrealGameSync_Separator);
			NotifyMenu_OpenUnrealGameSync_Separator.Size = new Size(193, 6);

			NotifyMenu_SyncNow = new ToolStripMenuItem();
			NotifyMenu_SyncNow.Name = nameof(NotifyMenu_SyncNow);
			NotifyMenu_SyncNow.Size = new Size(196, 22);
			NotifyMenu_SyncNow.Text = "Sync Now";
			NotifyMenu_SyncNow.Click += new EventHandler(NotifyMenu_SyncNow_Click);

			NotifyMenu_LaunchEditor = new ToolStripMenuItem();
			NotifyMenu_LaunchEditor.Name = nameof(NotifyMenu_LaunchEditor);
			NotifyMenu_LaunchEditor.Size = new Size(196, 22);
			NotifyMenu_LaunchEditor.Text = "Launch Editor";
			NotifyMenu_LaunchEditor.Click += new EventHandler(NotifyMenu_LaunchEditor_Click);

			NotifyMenu_ExitSeparator = new ToolStripSeparator();
			NotifyMenu_ExitSeparator.Name = nameof(NotifyMenu_ExitSeparator);
			NotifyMenu_ExitSeparator.Size = new Size(193, 6);

			NotifyMenu_Exit = new ToolStripMenuItem();
			NotifyMenu_Exit.Name = nameof(NotifyMenu_Exit);
			NotifyMenu_Exit.Size = new Size(196, 22);
			NotifyMenu_Exit.Text = "Exit";
			NotifyMenu_Exit.Click += new EventHandler(NotifyMenu_Exit_Click);

			// Create the notification menu
			NotifyMenu = new ContextMenuStrip(Components);
			NotifyMenu.Name = nameof(NotifyMenu);
			NotifyMenu.Size = new System.Drawing.Size(197, 104);
			NotifyMenu.SuspendLayout();
			NotifyMenu.Items.Add(NotifyMenu_OpenUnrealGameSync);
			NotifyMenu.Items.Add(NotifyMenu_OpenUnrealGameSync_Separator);
			NotifyMenu.Items.Add(NotifyMenu_SyncNow);
			NotifyMenu.Items.Add(NotifyMenu_LaunchEditor);
			NotifyMenu.Items.Add(NotifyMenu_ExitSeparator);
			NotifyMenu.Items.Add(NotifyMenu_Exit);
			NotifyMenu.ResumeLayout(false);

			// Create the notification icon
			NotifyIcon = new NotifyIcon(Components);
			NotifyIcon.ContextMenuStrip = NotifyMenu;
			NotifyIcon.Icon = Properties.Resources.Icon;
			NotifyIcon.Text = "UnrealGameSync";
			NotifyIcon.Visible = true;
			NotifyIcon.DoubleClick += new EventHandler(NotifyIcon_DoubleClick);
			NotifyIcon.MouseDown += new MouseEventHandler(NotifyIcon_MouseDown);

			// Find the initial list of projects to attempt to reopen
			List<DetectProjectSettingsTask> Tasks = new List<DetectProjectSettingsTask>();
			foreach(UserSelectedProjectSettings OpenProject in Settings.OpenProjects)
			{
				Log.WriteLine("Detecting settings for {0}", OpenProject);
				Tasks.Add(new DetectProjectSettingsTask(OpenProject, DataFolder, new PrefixedTextWriter("  ", Log)));
			}

			// Detect settings for the project we want to open
			DetectStartupProjectSettingsTask = new DetectMultipleProjectSettingsTask(Tasks);

			DetectStartupProjectSettingsWindow = new ModalTaskWindow(DetectStartupProjectSettingsTask, "Opening Projects", "Opening projects, please wait...", FormStartPosition.CenterScreen);
			if(bRestoreState)
			{
				if(Settings.bWindowVisible)
				{
					DetectStartupProjectSettingsWindow.Show();
				}
			}
			else
			{
				DetectStartupProjectSettingsWindow.Show();
				DetectStartupProjectSettingsWindow.Activate();
			}
			DetectStartupProjectSettingsWindow.Complete += OnDetectStartupProjectsComplete;
		}

		private void OnDetectStartupProjectsComplete()
		{
			// Close the startup window
			bool bVisible = DetectStartupProjectSettingsWindow.Visible;
			DetectStartupProjectSettingsWindow.Close();
			DetectStartupProjectSettingsWindow = null;

			// Create the main window
			MainWindowInstance = new MainWindow(ApiUrl, DataFolder, bRestoreState, UpdateSpawn ?? Assembly.GetExecutingAssembly().Location, DetectStartupProjectSettingsTask.Results, Log, Settings);
			if(bUnstable)
			{
				MainWindowInstance.Text += String.Format(" (UNSTABLE BUILD {0})", Assembly.GetExecutingAssembly().GetName().Version);
			}
			if(bVisible)
			{
				MainWindowInstance.Show();
				if(!bRestoreState)
				{
					MainWindowInstance.Activate();
				}
			}
			MainWindowInstance.FormClosed += MainWindowInstance_FormClosed;

			// Delete the project settings task
			DetectStartupProjectSettingsTask.Dispose();
			DetectStartupProjectSettingsTask = null;
		}

		private void MainWindowInstance_FormClosed(object sender, FormClosedEventArgs e)
		{
			ExitThread();
		}

		private void OnActivationListenerCallback()
		{
			// Check if we're trying to reopen with the unstable version; if so, trigger an update to trigger a restart with the new executable
			if(!bUnstable && (Control.ModifierKeys & Keys.Shift) != 0)
			{
				UpdateMonitor.TriggerUpdate();
			}
			else if(MainWindowInstance != null)
			{
				MainWindowInstance.ShowAndActivate();
			}
		}

		private void OnActivationListenerAsyncCallback()
		{
			MainThreadSynchronizationContext.Post((o) => OnActivationListenerCallback(), null);
		}

		private void OnUpdateAvailable()
		{
			if(MainWindowInstance != null && !bIsClosing && MainWindowInstance.CanPerformUpdate())
			{
				bIsClosing = true;
				MainWindowInstance.ForceClose();
			}
		}

		private void OnUpdateAvailableCallback()
		{ 
			MainThreadSynchronizationContext.Post((o) => OnUpdateAvailable(), null);
		}

		protected override void Dispose(bool bDisposing)
		{
			base.Dispose(bDisposing);

			if(Components != null)
			{
				Components.Dispose();
				Components = null;
			}

			if(NotifyIcon != null)
			{
				NotifyIcon.Dispose();
				NotifyIcon = null;
			}

			if(Log != null)
			{
				Log.Dispose();
				Log = null;
			}

			if(UpdateMonitor != null)
			{
				UpdateMonitor.OnUpdateAvailable -= OnUpdateAvailableCallback;
				UpdateMonitor.Close(); // prevent race condition
				UpdateMonitor = null;
			}

			if(ActivationListener != null)
			{
				ActivationListener.OnActivate -= OnActivationListenerAsyncCallback;
				ActivationListener.Stop();
				ActivationListener.Dispose();
				ActivationListener = null;
			}

			if(MainWindowInstance != null)
			{
				MainWindowInstance.ForceClose();
				MainWindowInstance = null;
			}

			if(DetectStartupProjectSettingsWindow != null)
			{
				DetectStartupProjectSettingsWindow.Close();
				DetectStartupProjectSettingsWindow = null;
			}
		}

		private void NotifyIcon_MouseDown(object sender, MouseEventArgs e)
		{
			// Have to set up this stuff here, because the menu is laid out before Opening() is called on it after mouse-up.
			bool bCanSyncNow = MainWindowInstance != null && MainWindowInstance.CanSyncNow();
			bool bCanLaunchEditor = MainWindowInstance != null && MainWindowInstance.CanLaunchEditor();
			NotifyMenu_SyncNow.Visible = bCanSyncNow;
			NotifyMenu_LaunchEditor.Visible = bCanLaunchEditor;
			NotifyMenu_ExitSeparator.Visible = bCanSyncNow || bCanLaunchEditor;

			// Show the startup window, if not already visible
			if(DetectStartupProjectSettingsWindow != null)
			{
				DetectStartupProjectSettingsWindow.Show();
			}
		}

		private void NotifyIcon_DoubleClick(object sender, EventArgs e)
		{
			if(MainWindowInstance != null)
			{
				MainWindowInstance.ShowAndActivate();
			}
		}

		private void NotifyMenu_OpenUnrealGameSync_Click(object sender, EventArgs e)
		{
			if(DetectStartupProjectSettingsWindow != null)
			{
				DetectStartupProjectSettingsWindow.ShowAndActivate();
			}
			if(MainWindowInstance != null)
			{
				MainWindowInstance.ShowAndActivate();
			}
		}

		private void NotifyMenu_SyncNow_Click(object sender, EventArgs e)
		{
			if(MainWindowInstance != null)
			{
				MainWindowInstance.SyncLatestChange();
			}
		}

		private void NotifyMenu_LaunchEditor_Click(object sender, EventArgs e)
		{
			if(MainWindowInstance != null)
			{
				MainWindowInstance.LaunchEditor();
			}
		}

		private void NotifyMenu_Exit_Click(object sender, EventArgs e)
		{
			if(DetectStartupProjectSettingsWindow != null)
			{
				DetectStartupProjectSettingsWindow.Close();
				DetectStartupProjectSettingsWindow = null;
			}

			if(MainWindowInstance != null)
			{
				MainWindowInstance.ForceClose();
				MainWindowInstance = null;
			}

			ExitThread();
		}

		protected override void ExitThreadCore()
		{
			base.ExitThreadCore();

			if(NotifyIcon != null)
			{
				NotifyIcon.Visible = false;
			}
		}
	}
}
