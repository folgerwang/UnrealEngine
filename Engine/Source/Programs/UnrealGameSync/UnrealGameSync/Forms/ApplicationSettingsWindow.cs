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
	partial class ApplicationSettingsWindow : Form
	{
		class GetDefaultSettingsTask : IModalTask
		{
			TextWriter Log;

			public string ServerAndPort;
			public string UserName;

			public GetDefaultSettingsTask(TextWriter Log)
			{
				this.Log = Log;
			}

			public bool Run(out string ErrorMessage)
			{
				if(PerforceModalTask.TryGetServerSettings(null, ref ServerAndPort, ref UserName, Log))
				{
					ErrorMessage = null;
					return true;
				}
				else
				{
					ErrorMessage = "Unable to query server settings";
					return false;
				}
			}
		}

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

		TextWriter Log;

		string InitialServerAndPort;
		string InitialUserName;
		string InitialDepotPath;
		bool bInitialUnstable;

		private ApplicationSettingsWindow(string DefaultServerAndPort, string DefaultUserName, bool bUnstable, TextWriter Log)
		{
			InitializeComponent();

			this.Log = Log;

			Utility.ReadGlobalPerforceSettings(ref InitialServerAndPort, ref InitialUserName, ref InitialDepotPath);
			bInitialUnstable = bUnstable;
			
			this.ServerTextBox.Text = InitialServerAndPort;
			this.ServerTextBox.Select(ServerTextBox.TextLength, 0);
			this.ServerTextBox.CueBanner = (DefaultServerAndPort == null)? "Default" : String.Format("Default ({0})", DefaultServerAndPort);

			this.UserNameTextBox.Text = InitialUserName;
			this.UserNameTextBox.Select(UserNameTextBox.TextLength, 0);
			this.UserNameTextBox.CueBanner = (DefaultUserName == null)? "Default" : String.Format("Default ({0})", DefaultUserName);

			this.DepotPathTextBox.Text = InitialDepotPath;
			this.DepotPathTextBox.Select(DepotPathTextBox.TextLength, 0);
			this.DepotPathTextBox.CueBanner = DeploymentSettings.DefaultDepotPath;

			this.UseUnstableBuildCheckBox.Checked = bUnstable;
		}

		public static bool? ShowModal(IWin32Window Owner, bool bUnstable, TextWriter Log)
		{
			GetDefaultSettingsTask DefaultSettings = new GetDefaultSettingsTask(Log);

			string ErrorMessage;
			ModalTask.Execute(Owner, DefaultSettings, "Checking Settings", "Checking settings, please wait...", out ErrorMessage);

			ApplicationSettingsWindow ApplicationSettings = new ApplicationSettingsWindow(DefaultSettings.ServerAndPort, DefaultSettings.UserName, bUnstable, Log);
			if(ApplicationSettings.ShowDialog() == DialogResult.OK)
			{
				return ApplicationSettings.UseUnstableBuildCheckBox.Checked;
			}
			else
			{
				return null;
			}
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
	}
}
