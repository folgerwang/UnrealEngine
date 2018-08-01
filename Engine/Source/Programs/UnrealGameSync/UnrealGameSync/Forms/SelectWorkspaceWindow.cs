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
	partial class SelectWorkspaceWindow : Form
	{
		class EnumerateWorkspacesTask : IPerforceModalTask
		{
			public PerforceInfoRecord Info;
			public List<PerforceClientRecord> Clients;

			public bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage)
			{
				if(!Perforce.Info(out Info, Log))
				{
					ErrorMessage = "Unable to query Perforce info.";
					return false;
				}
				if(!Perforce.FindClients(Info.UserName, out Clients, Log))
				{
					ErrorMessage = "Unable to enumerate clients from Perforce";
					return false;
				}

				ErrorMessage = null;
				return true;
			}
		}

		PerforceInfoRecord Info;
		List<PerforceClientRecord> Clients;
		string WorkspaceName;

		private SelectWorkspaceWindow(PerforceInfoRecord Info, List<PerforceClientRecord> Clients, string WorkspaceName)
		{
			InitializeComponent();

			this.Info = Info;
			this.Clients = Clients;
			this.WorkspaceName = WorkspaceName;

			UpdateListView();
			UpdateOkButton();
		}

		private void UpdateListView()
		{
			if(WorkspaceListView.SelectedItems.Count > 0)
			{
				WorkspaceName = WorkspaceListView.SelectedItems[0].Text;
			}
			else
			{
				WorkspaceName = null;
			}

			WorkspaceListView.Items.Clear();

			foreach(PerforceClientRecord Client in Clients.OrderBy(x => x.Name))
			{
				if(!OnlyForThisComputer.Checked || String.Compare(Client.Host, Info.HostName, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					ListViewItem Item = new ListViewItem(Client.Name);
					Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, Client.Host));
					Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, Client.Stream));
					Item.SubItems.Add(new ListViewItem.ListViewSubItem(Item, Client.Root));
					Item.Selected = (WorkspaceName == Client.Name);
					WorkspaceListView.Items.Add(Item);
				}
			}
		}

		private void UpdateOkButton()
		{
			OkBtn.Enabled = (WorkspaceListView.SelectedItems.Count == 1);
		}

		public static bool ShowModal(IWin32Window Owner, string ServerAndPort, string UserName, string WorkspaceName, TextWriter Log, out string NewWorkspaceName)
		{
			EnumerateWorkspacesTask EnumerateWorkspaces = new EnumerateWorkspacesTask();

			string ErrorMessage;
			ModalTaskResult Result = PerforceModalTask.Execute(Owner, null, ServerAndPort, UserName, EnumerateWorkspaces, "Finding workspaces", "Finding workspaces, please wait...", Log, out ErrorMessage);
			if(Result != ModalTaskResult.Succeeded)
			{
				if(!String.IsNullOrEmpty(ErrorMessage))
				{
					MessageBox.Show(Owner, ErrorMessage);
				}

				NewWorkspaceName = null;
				return false;
			}

			SelectWorkspaceWindow SelectWorkspace = new SelectWorkspaceWindow(EnumerateWorkspaces.Info, EnumerateWorkspaces.Clients, WorkspaceName);
			if(SelectWorkspace.ShowDialog(Owner) == DialogResult.OK)
			{
				NewWorkspaceName = SelectWorkspace.WorkspaceName;
				return true;
			}
			else
			{
				NewWorkspaceName = null;
				return false;
			}
		}

		private void OnlyForThisComputer_CheckedChanged(object sender, EventArgs e)
		{
			UpdateListView();
		}

		private void WorkspaceListView_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if(WorkspaceListView.SelectedItems.Count > 0)
			{
				WorkspaceName = WorkspaceListView.SelectedItems[0].Text;
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void WorkspaceListView_MouseDoubleClick(object sender, MouseEventArgs e)
		{
			if(WorkspaceListView.SelectedItems.Count > 0)
			{
				WorkspaceName = WorkspaceListView.SelectedItems[0].Text;
				DialogResult = DialogResult.OK;
				Close();
			}
		}
	}
}
