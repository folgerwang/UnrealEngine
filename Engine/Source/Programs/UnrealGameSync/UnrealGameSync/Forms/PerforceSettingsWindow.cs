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

namespace UnrealGameSync
{
	partial class PerforceSettingsWindow : Form
	{
		class PerforceTestConnectionTask : IPerforceModalTask
		{
			string DepotPath;

			public PerforceTestConnectionTask(string DepotPath)
			{
				this.DepotPath = DepotPath ?? DeploymentSettings.DefaultDepotPath;
			}

			public bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage)
			{
				string CheckFilePath = String.Format("{0}/Release/UnrealGameSync.exe", DepotPath);

				List<PerforceFileRecord> FileRecords;
				if(!Perforce.FindFiles(CheckFilePath, out FileRecords, Log) || FileRecords.Count == 0)
				{
					ErrorMessage = String.Format("Unable to find {0}", CheckFilePath);
					return false;
				}

				ErrorMessage = null;
				return true;
			}
		}

		UserSettings Settings;
		TextWriter Log;

		string InitialServerAndPort;
		string InitialUserName;
		string InitialDepotPath;
		bool bInitialUnstable;

		public PerforceSettingsWindow(bool bUnstable, UserSettings Settings, TextWriter Log)
		{
			InitializeComponent();

			this.Settings = Settings;
			this.Log = Log;

			Utility.ReadGlobalPerforceSettings(ref InitialServerAndPort, ref InitialUserName, ref InitialDepotPath);
			bInitialUnstable = bUnstable;

			this.ServerTextBox.Text = InitialServerAndPort;
			this.ServerTextBox.Select(ServerTextBox.TextLength, 0);

			this.UserNameTextBox.Text = InitialUserName;
			this.UserNameTextBox.Select(UserNameTextBox.TextLength, 0);

			this.DepotPathTextBox.Text = InitialDepotPath;
			this.DepotPathTextBox.Select(DepotPathTextBox.TextLength, 0);
			this.DepotPathTextBox.CueBanner = DeploymentSettings.DefaultDepotPath;

			this.UseUnstableBuildCheckBox.Checked = bUnstable;
		}

		private void OkBtn_Click(object sender, EventArgs e)
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
			if(DepotPath.Length == 0 || DepotPath == DeploymentSettings.DefaultDepotPath)
			{
				DepotPath = null;
			}

			bool bUnstable = UseUnstableBuildCheckBox.Checked;

			if(ServerAndPort == InitialServerAndPort && UserName == InitialUserName && DepotPath == InitialDepotPath && bUnstable == bInitialUnstable)
			{
				DialogResult = DialogResult.Cancel;
				Close();
			}
			else
			{
				// Try to log in to the new server, and check the application is there
				if(ServerAndPort != InitialServerAndPort || UserName != InitialUserName || DepotPath != InitialDepotPath)
				{
					string ErrorMessage;
					ModalTaskResult Result = PerforceModalTask.Execute(this, null, ServerAndPort, UserName, new PerforceTestConnectionTask(DepotPath), "Connecting", "Checking connection, please wait...", Log, out ErrorMessage);
					if(Result != ModalTaskResult.Succeeded)
					{
						if(Result == ModalTaskResult.Failed)
						{
							MessageBox.Show(ErrorMessage, "Unable to connect");
						}
						return;
					}
				}

				if(MessageBox.Show("UnrealGameSync must be restarted to apply these settings.\n\nWould you like to restart now?", "Restart Required", MessageBoxButtons.OKCancel) == DialogResult.OK)
				{
					Utility.SaveGlobalPerforceSettings(ServerAndPort, UserName, DepotPath);

					DialogResult = DialogResult.OK;
					Close();
				}
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void AdvancedBtn_Click(object sender, EventArgs e)
		{
			PerforceSyncSettingsWindow Window = new PerforceSyncSettingsWindow(Settings);
			Window.ShowDialog();
		}
	}
}
