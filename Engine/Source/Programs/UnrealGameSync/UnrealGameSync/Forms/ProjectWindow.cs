// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ProjectWindow : Form
	{
		UserSelectedProject SelectedProject;
		UserSettings Settings;

		private ProjectWindow(UserSelectedProject SelectedProject, UserSettings Settings)
		{
			InitializeComponent();

			this.SelectedProject = SelectedProject;
			this.Settings = Settings;

			UpdateServerLabel();

//			WorkspaceNameTextBox.Text = SelectedProject.WorkspaceName ?? "";
//			WorkspaceNameTextBox.Text = SelectedProject.WorkspacePath ?? "";
		}

		public static bool ShowModal(IWin32Window Owner, UserSelectedProject SelectedProject, UserSettings Settings)
		{
			ProjectWindow Window = new ProjectWindow(SelectedProject, Settings);
			return (Window.ShowDialog(Owner) == DialogResult.OK);
		}

		private void UpdateServerLabel()
		{
//			if(SelectedProject.bUseDefaultServer)
//			{
//				ServerLabel.Text = "Using default Perforce server settings.";
//			}
//			else
//			{
//				ServerLabel.Text = String.Format("Using Perforce server '{0}' and user '{1}'.", SelectedProject.ServerAndPort, SelectedProject.UserName);
//			}

			ChangeLink.Location = new Point(ServerLabel.Right + 5, ChangeLink.Location.Y);
		}

		private void WorkspaceNewBtn_Click(object sender, EventArgs e)
		{

		}

		private void WorkspaceBrowseBtn_Click(object sender, EventArgs e)
		{

		}

		private void PathBrowseBtn_Click(object sender, EventArgs e)
		{

		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
//			UserSelectedProject NewSelectedProject = new UserSelectedProject();

	//		NewSelectedProject.WorkspaceName = WorkspaceNameTextBox.Text;
	//		NewSelectedProject.WorkspacePath = WorkspacePathTextBox.Text;

//			SelectedProject = NewSelectedProject;
		}

		private void ChangeLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			// Detect the default Perforce settings for the selected project
			string DefaultServerAndPort = "perforce:1666";
			string DefaultUserName = Environment.UserName;

			DetectConnectionSettingsTask DetectConnectionTask = new DetectConnectionSettingsTask(null);

			string ErrorMessage;
			if(ModalTaskWindow.Execute(Owner, DetectConnectionTask, "Connection Settings", "Detecting settings, please wait...", out ErrorMessage) == ModalTaskResult.Succeeded)
			{
				DefaultServerAndPort = DetectConnectionTask.ServerAndPort ?? DetectConnectionTask.PerforceInfo.HostName ?? DefaultServerAndPort;
				DefaultUserName = DetectConnectionTask.PerforceInfo.UserName ?? DefaultUserName;
			}

			// Show the connection window
/*			ConnectWindow Connect = new ConnectWindow(SelectedProject.bUseDefaultServer, SelectedProject.ServerAndPort, SelectedProject.UserName, DefaultServerAndPort, DefaultUserName);
			if(Connect.ShowDialog() == DialogResult.OK)
			{
				SelectedProject.bUseDefaultServer = Connect.bUseDefaultSettings;
				SelectedProject.ServerAndPort = Connect.ServerAndPort;
				SelectedProject.UserName = Connect.UserName;
			}*/
		}
	}
}
