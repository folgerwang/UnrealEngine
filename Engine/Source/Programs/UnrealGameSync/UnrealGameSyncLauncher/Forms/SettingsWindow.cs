// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	partial class SettingsWindow : Form
	{
		[DllImport("user32.dll")]
		private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, int wParam, [MarshalAs(UnmanagedType.LPWStr)] string lParam);

		public delegate bool SyncAndRunDelegate(PerforceConnection Perforce, string DepotPath, bool bUnstable, TextWriter LogWriter);

		const int EM_SETCUEBANNER = 0x1501;

		string LogText;
		SyncAndRunDelegate SyncAndRun;

		public SettingsWindow(string Prompt, string LogText, string ServerAndPort, string UserName, string DepotPath, bool bUnstable, SyncAndRunDelegate SyncAndRun)
		{
			InitializeComponent();

			if(Prompt != null)
			{
				this.PromptLabel.Text = Prompt;
			}

			this.LogText = LogText;
			this.ServerTextBox.Text = ServerAndPort;
			this.UserNameTextBox.Text = UserName;
			this.DepotPathTextBox.Text = DepotPath;
			this.UseUnstableBuildCheckBox.Checked = bUnstable;
			this.SyncAndRun = SyncAndRun;

			ViewLogBtn.Visible = LogText != null;
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			SendMessage(ServerTextBox.Handle, EM_SETCUEBANNER, 1, "Default Server");
			SendMessage(UserNameTextBox.Handle, EM_SETCUEBANNER, 1, "Default User");
		}

		private void ViewLogBtn_Click(object sender, EventArgs e)
		{
			LogWindow Log = new LogWindow(LogText);
			Log.ShowDialog(this);
		}

		private void ConnectBtn_Click(object sender, EventArgs e)
		{
			// Update the settings
			string ServerAndPort = ServerTextBox.Text.Trim();
			if(ServerAndPort.Length == 0)
			{
				ServerAndPort = null;
			}

			string UserName = UserNameTextBox.Text.Trim();
			if(UserName.Length == 0)
			{
				UserName = null;
			}

			string DepotPath = DepotPathTextBox.Text.Trim();
			if(DepotPath.Length == 0)
			{
				DepotPath = null;
			}

			Program.SaveSettings(ServerAndPort, UserName, DepotPath);

			// Create the task for connecting to this server
			StringWriter Log = new StringWriter();
			SyncAndRunPerforceTask SyncApplication = new SyncAndRunPerforceTask((Perforce, LogWriter) => SyncAndRun(Perforce, DepotPath, UseUnstableBuildCheckBox.Checked, LogWriter));

			// Attempt to sync through a modal dialog
			string ErrorMessage;
			ModalTaskResult Result = PerforceModalTask.Execute(this, null, ServerAndPort, UserName, SyncApplication, "Updating", "Checking for updates, please wait...", Log, out ErrorMessage);
			if(Result == ModalTaskResult.Succeeded)
			{
				Program.SaveSettings(ServerAndPort, UserName, DepotPath);
				DialogResult = DialogResult.OK;
				Close();
			}

			if(ErrorMessage != null)
			{
				PromptLabel.Text = ErrorMessage;
			}

			LogText = Log.ToString();
			ViewLogBtn.Visible = true;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
