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
	partial class NewWorkspaceWindow : Form
	{
		class NewWorkspaceSettings
		{
			public string Name;
			public string Stream;
			public string RootDir;
		}

		class FindWorkspaceSettingsTask : IPerforceModalTask
		{
			string CurrentWorkspaceName;
			public string CurrentStream;
			public PerforceInfoRecord Info;
			public List<PerforceClientRecord> Clients;

			public FindWorkspaceSettingsTask(string WorkspaceName)
			{
				this.CurrentWorkspaceName = WorkspaceName;
			}

			public bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage)
			{
				if(!Perforce.Info(out Info, Log))
				{
					ErrorMessage = "Unable to get Perforce info.";
					return false;
				}
				if(!Perforce.FindClients(Info.UserName, out Clients, Log))
				{
					ErrorMessage = "Unable to enumerate Perforce clients.";
					return false;
				}
				if(!String.IsNullOrEmpty(CurrentWorkspaceName))
				{
					PerforceConnection PerforceClient = new PerforceConnection(Perforce.UserName, CurrentWorkspaceName, Perforce.ServerAndPort);
					PerforceClient.GetActiveStream(out CurrentStream, Log);
				}

				ErrorMessage = null;
				return true;
			}
		}

		class NewWorkspaceTask : IPerforceModalTask
		{
			NewWorkspaceSettings Settings;
			string Owner;
			string HostName;

			public NewWorkspaceTask(NewWorkspaceSettings Settings, string Owner, string HostName)
			{
				this.Settings = Settings;
				this.Owner = Owner;
				this.HostName = HostName;
			}

			public bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage)
			{
				bool bExists;
				if(!Perforce.ClientExists(Settings.Name, out bExists, Log))
				{
					ErrorMessage = String.Format("Unable to determine if client already exists.\n\n{0}", Log.ToString());
					return false;
				}
				if(bExists)
				{
					ErrorMessage = String.Format("Client '{0}' already exists.", Settings.Name);
					return false;
				}

				PerforceSpec Client = new PerforceSpec();
				Client.SetField("Client", Settings.Name);
				Client.SetField("Owner", Owner);
				Client.SetField("Host", HostName);
				Client.SetField("Stream", Settings.Stream);
				Client.SetField("Root", Settings.RootDir);
				Client.SetField("Options", "rmdir");

				string Message;
				if(!Perforce.CreateClient(Client, out Message, Log))
				{
					ErrorMessage = Message;
					return false;
				}

				ErrorMessage = null;
				return true;
			}
		}

		string ServerAndPort;
		string UserName;
		PerforceInfoRecord Info;
		List<PerforceClientRecord> Clients;
		TextWriter Log;
		NewWorkspaceSettings Settings;
		string DefaultRootPath;

		private NewWorkspaceWindow(string ServerAndPort, string UserName, string DefaultStream, PerforceInfoRecord Info, List<PerforceClientRecord> Clients, TextWriter Log)
		{
			InitializeComponent();

			this.ServerAndPort = ServerAndPort;
			this.UserName = UserName;
			this.Info = Info;
			this.Clients = Clients;
			this.Log = Log;

			Dictionary<string, int> RootPathToCount = new Dictionary<string, int>(StringComparer.InvariantCultureIgnoreCase);
			foreach(PerforceClientRecord Client in Clients)
			{
				if(Client.Host == null || String.Compare(Client.Host, Info.HostName, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					if(!String.IsNullOrEmpty(Client.Root) && Client.Root != ".")
					{
						string ParentDir;
						try
						{
							ParentDir = Path.GetFullPath(Path.GetDirectoryName(Client.Root));
						}
						catch
						{
							ParentDir = null;
						}

						if(ParentDir != null)
						{
							int Count;
							RootPathToCount.TryGetValue(ParentDir, out Count);
							RootPathToCount[ParentDir] = Count + 1;
						}
					}
				}
			}

			int RootPathMaxCount = 0;
			foreach(KeyValuePair<string, int> RootPathPair in RootPathToCount)
			{
				if(RootPathPair.Value > RootPathMaxCount)
				{
					DefaultRootPath = RootPathPair.Key;
					RootPathMaxCount = RootPathPair.Value;
				}
			}

			StreamTextBox.Text = DefaultStream ?? "";
			StreamTextBox.SelectionStart = StreamTextBox.Text.Length;
			StreamTextBox.SelectionLength = 0;
			StreamTextBox.Focus();

			UpdateOkButton();
			UpdateNameCueBanner();
			UpdateRootDirCueBanner();
		}

		public static bool ShowModal(IWin32Window Owner, string ServerAndPort, string UserName, string CurrentWorkspaceName, TextWriter Log, out string WorkspaceName)
		{
			FindWorkspaceSettingsTask FindSettings = new FindWorkspaceSettingsTask(CurrentWorkspaceName);

			string ErrorMessage;
			if(PerforceModalTask.Execute(Owner, null, ServerAndPort, UserName, FindSettings, "Checking settings", "Checking settings, please wait...", Log, out ErrorMessage) != ModalTaskResult.Succeeded)
			{
				if(!String.IsNullOrEmpty(ErrorMessage))
				{
					MessageBox.Show(ErrorMessage);
				}
			}

			NewWorkspaceWindow Window = new NewWorkspaceWindow(ServerAndPort, UserName, FindSettings.CurrentStream, FindSettings.Info, FindSettings.Clients, Log);
			if(Window.ShowDialog(Owner) == DialogResult.OK)
			{
				WorkspaceName = Window.Settings.Name;
				return true;
			}
			else
			{
				WorkspaceName = null;
				return false;
			}
		}

		private void RootDirBrowseBtn_Click(object sender, EventArgs e)
		{
			FolderBrowserDialog Dialog = new FolderBrowserDialog();
			Dialog.ShowNewFolderButton = true;
			Dialog.SelectedPath = RootDirTextBox.Text;
			if (Dialog.ShowDialog() == DialogResult.OK)
			{
				RootDirTextBox.Text = Dialog.SelectedPath;
				UpdateOkButton();
			}
		}

		private string GetDefaultWorkspaceName()
		{
			string BaseName = Sanitize(String.Format("{0}_{1}_{2}", Info.UserName, Info.HostName, StreamTextBox.Text.Replace('/', '_').Trim('_'))).Trim('_');

			string Name = BaseName;
			for(int Idx = 2; Clients.Any(x => x.Name != null && String.Compare(x.Name, Name, StringComparison.InvariantCultureIgnoreCase) == 0); Idx++)
			{
				Name = String.Format("{0}_{1}", BaseName, Idx);
			}
			return Name;
		}

		private string GetDefaultWorkspaceRootDir()
		{
			string RootDir = "";
			if(!String.IsNullOrEmpty(DefaultRootPath))
			{
				string Suffix = String.Join("_", StreamTextBox.Text.Split(new char[]{ '/' }, StringSplitOptions.RemoveEmptyEntries).Select(x => Sanitize(x)).Where(x => x.Length > 0));
				if(Suffix.Length > 0)
				{
					RootDir = Path.Combine(DefaultRootPath, Suffix);
				}
			}
			return RootDir;
		}

		private string Sanitize(string Text)
		{
			StringBuilder Result = new StringBuilder();
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				if(Char.IsLetterOrDigit(Text[Idx]) || Text[Idx] == '_' || Text[Idx] == '.' || Text[Idx] == '-')
				{
					Result.Append(Text[Idx]);
				}
			}
			return Result.ToString();
		}

		private void UpdateNameCueBanner()
		{
			NameTextBox.CueBanner = GetDefaultWorkspaceName();
		}

		private void UpdateRootDirCueBanner()
		{
			RootDirTextBox.CueBanner = GetDefaultWorkspaceRootDir();
		}

		private void UpdateOkButton()
		{
			NewWorkspaceSettings Settings;
			OkBtn.Enabled = TryGetWorkspaceSettings(out Settings);
		}

		private bool TryGetWorkspaceSettings(out NewWorkspaceSettings Settings)
		{
			string NewWorkspaceName = NameTextBox.Text.Trim();
			if(NewWorkspaceName.Length == 0)
			{
				NewWorkspaceName = GetDefaultWorkspaceName();
				if(NewWorkspaceName.Length == 0)
				{
					Settings = null;
					return false;
				}
			}

			string NewStream = StreamTextBox.Text.Trim();
			if(!NewStream.StartsWith("//") || NewStream.IndexOf('/', 2) == -1)
			{
				Settings = null;
				return false;
			}

			string NewRootDir = RootDirTextBox.Text.Trim();
			if(NewRootDir.Length == 0)
			{
				NewRootDir = GetDefaultWorkspaceRootDir();
				if(NewRootDir.Length == 0)
				{
					Settings = null;
					return false;
				}
			}

			try
			{
				NewRootDir = Path.GetFullPath(NewRootDir);
			}
			catch
			{
				Settings = null;
				return false;
			}

			Settings = new NewWorkspaceSettings{ Name = NewWorkspaceName, Stream = NewStream, RootDir = NewRootDir };
			return true;
		}

		private void NameTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
		}

		private void StreamTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
			UpdateNameCueBanner();
			UpdateRootDirCueBanner();
		}

		private void RootDirTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if(TryGetWorkspaceSettings(out Settings))
			{
				DirectoryInfo RootDir = new DirectoryInfo(Settings.RootDir);
				if(RootDir.Exists && RootDir.EnumerateFileSystemInfos().Any(x => x.Name != "." && x.Name != ".."))
				{
					if(MessageBox.Show(this, String.Format("The directory '{0}' is not empty. Are you sure you want to create a workspace there?", RootDir.FullName), "Directory not empty", MessageBoxButtons.YesNo) != DialogResult.Yes)
					{
						return;
					}
				}

				NewWorkspaceTask NewWorkspace = new NewWorkspaceTask(Settings, Info.UserName, Info.HostName);

				string ErrorMessage;
				if(PerforceModalTask.Execute(Owner, null, ServerAndPort, UserName, NewWorkspace, "Creating workspace", "Creating workspace, please wait...", Log, out ErrorMessage) != ModalTaskResult.Succeeded)
				{
					MessageBox.Show(ErrorMessage);
					return;
				}

				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void StreamBrowseBtn_Click(object sender, EventArgs e)
		{
			string StreamName = StreamTextBox.Text.Trim();
			if(SelectStreamWindow.ShowModal(this, ServerAndPort, UserName, StreamName, Log, out StreamName))
			{
				StreamTextBox.Text = StreamName;
			}
		}

		private void RootDirTextBox_Enter(object sender, EventArgs e)
		{
			if(RootDirTextBox.Text.Length == 0)
			{
				RootDirTextBox.Text = RootDirTextBox.CueBanner;
			}
		}

		private void RootDirTextBox_Leave(object sender, EventArgs e)
		{
			if(RootDirTextBox.Text == RootDirTextBox.CueBanner)
			{
				RootDirTextBox.Text = "";
			}
		}

		private void NameTextBox_Enter(object sender, EventArgs e)
		{
			if(NameTextBox.Text.Length == 0)
			{
				NameTextBox.Text = NameTextBox.CueBanner;
			}
		}

		private void NameTextBox_Leave(object sender, EventArgs e)
		{
			if(NameTextBox.Text == NameTextBox.CueBanner)
			{
				NameTextBox.Text = "";
			}
		}
	}
}
