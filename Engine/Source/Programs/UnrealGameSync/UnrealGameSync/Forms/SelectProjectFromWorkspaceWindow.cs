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
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class SelectProjectFromWorkspaceWindow : Form
	{
		class EnumerateWorkspaceProjectsTask : IPerforceModalTask
		{
			static readonly string[] Patterns =
			{
				"....uproject",
				"....uprojectdirs",
			};

			public string ClientName;
			public List<string> Paths;

			public EnumerateWorkspaceProjectsTask(string ClientName)
			{
				this.ClientName = ClientName;
			}

			public bool Run(PerforceConnection Perforce, TextWriter Log, out string ErrorMessage)
			{
				PerforceConnection PerforceClient = new PerforceConnection(Perforce.UserName, ClientName, Perforce.ServerAndPort);

				PerforceSpec ClientSpec;
				if(!PerforceClient.TryGetClientSpec(Perforce.ClientName, out ClientSpec, Log))
				{
					ErrorMessage = String.Format("Unable to get client spec for {0}", ClientName);
					return false;
				}

				string ClientRoot = ClientSpec.GetField("Root");
				if(String.IsNullOrEmpty(ClientRoot))
				{
					ErrorMessage = String.Format("Client '{0}' does not have a valid root directory.", ClientName);
					return false;
				}

				List<PerforceFileRecord> FileRecords = new List<PerforceFileRecord>();
				foreach(string Pattern in Patterns)
				{
					string Filter = String.Format("//{0}/{1}", ClientName, Pattern);

					List<PerforceFileRecord> WildcardFileRecords = new List<PerforceFileRecord>();
					if(!PerforceClient.FindFiles(Filter, out WildcardFileRecords, Log))
					{
						ErrorMessage = String.Format("Unable to enumerate files matching {0}", Filter);
						return false;
					}

					FileRecords.AddRange(WildcardFileRecords);
				}

				string ClientPrefix = ClientRoot + Path.DirectorySeparatorChar;

				Paths = new List<string>();
				foreach(PerforceFileRecord FileRecord in FileRecords)
				{
					if(FileRecord.ClientPath.StartsWith(ClientPrefix, StringComparison.InvariantCultureIgnoreCase))
					{
						Paths.Add(FileRecord.ClientPath.Substring(ClientRoot.Length).Replace(Path.DirectorySeparatorChar, '/'));
					}
				}

				ErrorMessage = null;
				return true;
			}
		}

		[DllImport("Shell32.dll", EntryPoint = "ExtractIconExW", CharSet = CharSet.Unicode, ExactSpelling = true, CallingConvention = CallingConvention.StdCall)]
		private static extern int ExtractIconEx(string sFile, int iIndex, IntPtr piLargeVersion, out IntPtr piSmallVersion, int amountIcons);

		string SelectedProjectFile;

		class ProjectNode
		{
			public string FullName;
			public string Folder;
			public string Name;

			public ProjectNode(string FullName)
			{
				this.FullName = FullName;

				int SlashIdx = FullName.LastIndexOf('/');
				Folder = FullName.Substring(0, SlashIdx);
				Name = FullName.Substring(SlashIdx + 1);
			}
		}
		
		public SelectProjectFromWorkspaceWindow(string WorkspaceName, List<string> ProjectFiles, string SelectedProjectFile)
		{
			InitializeComponent();
			
			this.SelectedProjectFile = SelectedProjectFile;

			// Make the image strip containing icons for nodes in the tree
			IntPtr FolderIconPtr;
			ExtractIconEx("imageres.dll", 3, IntPtr.Zero, out FolderIconPtr, 1);

			Icon[] Icons = new Icon[]{ Icon.FromHandle(FolderIconPtr), Properties.Resources.Icon };

			Bitmap TypeImageListBitmap = new Bitmap(Icons.Length * 16, 16, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
			using(Graphics Graphics = Graphics.FromImage(TypeImageListBitmap))
			{
				for(int IconIdx = 0; IconIdx < Icons.Length; IconIdx++)
				{
					Graphics.DrawIcon(Icons[IconIdx], new Rectangle(IconIdx * 16, 0, 16, 16));
				}
			}

			ImageList TypeImageList = new ImageList();
			TypeImageList.ImageSize = new Size(16, 16);
			TypeImageList.ColorDepth = ColorDepth.Depth32Bit;
			TypeImageList.Images.AddStrip(TypeImageListBitmap);
			ProjectTreeView.ImageList = TypeImageList;

			// Create the root node
			TreeNode RootNode = new TreeNode();
			RootNode.Text = WorkspaceName;
			RootNode.Expand();
			ProjectTreeView.Nodes.Add(RootNode);

			// Sort by paths, then files
			List<ProjectNode> ProjectNodes = ProjectFiles.Select(x => new ProjectNode(x)).OrderBy(x => x.Folder).ThenBy(x => x.Name).ToList();

			// Add the folders for each project
			TreeNode[] ProjectParentNodes = new TreeNode[ProjectNodes.Count];
			for(int Idx = 0; Idx < ProjectNodes.Count; Idx++)
			{
				TreeNode ParentNode = RootNode;
				if(ProjectNodes[Idx].Folder.Length > 0)
				{
					string[] Fragments = ProjectNodes[Idx].Folder.Split(new char[]{ '/' }, StringSplitOptions.RemoveEmptyEntries);
					foreach(string Fragment in Fragments)
					{
						ParentNode = FindOrAddChildNode(ParentNode, Fragment, 0);
					}
				}
				ProjectParentNodes[Idx] = ParentNode;
			}

			// Add the actual project nodes themselves
			for(int Idx = 0; Idx < ProjectNodes.Count; Idx++)
			{
				TreeNode Node = FindOrAddChildNode(ProjectParentNodes[Idx], ProjectNodes[Idx].Name, 1);
				Node.Tag = ProjectNodes[Idx].FullName;

				if(String.Compare(ProjectNodes[Idx].FullName, SelectedProjectFile, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					ProjectTreeView.SelectedNode = Node;
					for(TreeNode ParentNode = Node.Parent; ParentNode != RootNode; ParentNode = ParentNode.Parent)
					{
						ParentNode.Expand();
					}
				}
			}
		}

		static TreeNode FindOrAddChildNode(TreeNode ParentNode, string Text, int ImageIndex)
		{
			foreach(TreeNode ChildNode in ParentNode.Nodes)
			{
				if(String.Compare(ChildNode.Text, Text, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					return ChildNode;
				}
			}

			TreeNode NextNode = new TreeNode(Text);
			NextNode.ImageIndex = ImageIndex;
			NextNode.SelectedImageIndex = ImageIndex;
			ParentNode.Nodes.Add(NextNode);
			return NextNode;
		}

		public static bool ShowModal(IWin32Window Owner, string ServerAndPort, string UserName, string WorkspaceName, string WorkspacePath, TextWriter Log, out string NewWorkspacePath)
		{
			EnumerateWorkspaceProjectsTask EnumerateProjectsTask = new EnumerateWorkspaceProjectsTask(WorkspaceName);

			string ErrorMessage;
			if(PerforceModalTask.Execute(Owner, null, ServerAndPort, UserName, EnumerateProjectsTask, "Finding Projects", "Finding projects, please wait...", Log, out ErrorMessage) != ModalTaskResult.Succeeded)
			{
				if(!String.IsNullOrEmpty(ErrorMessage))
				{
					MessageBox.Show(Owner, ErrorMessage);
				}
				NewWorkspacePath = null;
				return false;
			}

			SelectProjectFromWorkspaceWindow SelectProjectWindow = new SelectProjectFromWorkspaceWindow(WorkspaceName, EnumerateProjectsTask.Paths, WorkspacePath);
			if(SelectProjectWindow.ShowDialog() == DialogResult.OK && !String.IsNullOrEmpty(SelectProjectWindow.SelectedProjectFile))
			{
				NewWorkspacePath = SelectProjectWindow.SelectedProjectFile;
				return true;
			}
			else
			{
				NewWorkspacePath = null;
				return false;
			}
		}

		private void ProjectTreeView_AfterSelect(object sender, TreeViewEventArgs e)
		{
			OkBtn.Enabled = (e.Node.Tag != null);
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if(ProjectTreeView.SelectedNode != null && ProjectTreeView.SelectedNode.Tag != null)
			{
				SelectedProjectFile = (string)ProjectTreeView.SelectedNode.Tag;
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
