// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	partial class ConnectWindow : Form
	{
		string ServerAndPort;
		string UserName;

		public ConnectWindow(string ServerAndPort, string UserName)
		{
			InitializeComponent();

			if(String.IsNullOrEmpty(ServerAndPort))
			{
				ServerAndPort = null;
			}
			if(String.IsNullOrEmpty(UserName))
			{
				UserName = null;
			}

			if(ServerAndPort != null)
			{
				ServerAndPortTextBox.Text = ServerAndPort;
			}
			if(UserName != null)
			{
				UserNameTextBox.Text = UserName;
			}
			UseDefaultConnectionSettings.Checked = ServerAndPort == null && UserName == null;

			UpdateEnabledControls();
		}

		public static bool ShowModal(IWin32Window Owner, string ServerAndPort, string UserName, out string NewServerAndPort, out string NewUserName)
		{
			ConnectWindow Connect = new ConnectWindow(ServerAndPort, UserName);
			if(Connect.ShowDialog() == DialogResult.OK)
			{
				NewServerAndPort = Connect.ServerAndPort;
				NewUserName = Connect.UserName;
				return true;
			}
			else
			{
				NewServerAndPort = null;
				NewUserName = null;
				return false;
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if(UseDefaultConnectionSettings.Checked)
			{
				ServerAndPort = null;
				UserName = null;
			}
			else
			{
				ServerAndPort = ServerAndPortTextBox.Text.Trim();
				if(ServerAndPort.Length == 0)
				{
					ServerAndPort = null;
				}

				UserName = UserNameTextBox.Text.Trim();
				if(UserName.Length == 0)
				{
					UserName = null;
				}
			}

			DialogResult = DialogResult.OK;
			Close();
		}

		private void UseCustomSettings_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void UpdateEnabledControls()
		{
			bool bUseDefaultSettings = UseDefaultConnectionSettings.Checked;

			ServerAndPortLabel.Enabled = !bUseDefaultSettings;
			ServerAndPortTextBox.Enabled = !bUseDefaultSettings;

			UserNameLabel.Enabled = !bUseDefaultSettings;
			UserNameTextBox.Enabled = !bUseDefaultSettings;
		}
	}
}
