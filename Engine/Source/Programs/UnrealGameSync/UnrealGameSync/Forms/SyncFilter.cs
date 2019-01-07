// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class SyncFilter : Form
	{
		Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToCategory;
		public string[] GlobalView;
		public Guid[] GlobalExcludedCategories;
		public bool bGlobalSyncAllProjects;
		public bool bGlobalIncludeAllProjectsInSolution;
		public string[] WorkspaceView;
		public Guid[] WorkspaceIncludedCategories;
		public Guid[] WorkspaceExcludedCategories;
		public bool? bWorkspaceSyncAllProjects;
		public bool? bWorkspaceIncludeAllProjectsInSolution;

		public SyncFilter(Dictionary<Guid, WorkspaceSyncCategory> InUniqueIdToCategory, string[] InGlobalView, Guid[] InGlobalExcludedCategories, bool bInGlobalProjectOnly, bool bInGlobalIncludeAllProjectsInSolution, string[] InWorkspaceView, Guid[] InWorkspaceIncludedCategories, Guid[] InWorkspaceExcludedCategories, bool? bInWorkspaceProjectOnly, bool? bInWorkspaceIncludeAllProjectsInSolution)
		{
			InitializeComponent();

			UniqueIdToCategory = InUniqueIdToCategory;
			GlobalExcludedCategories = InGlobalExcludedCategories;
			GlobalView = InGlobalView;
			bGlobalSyncAllProjects = bInGlobalProjectOnly;
			bGlobalIncludeAllProjectsInSolution = bInGlobalIncludeAllProjectsInSolution;
			WorkspaceIncludedCategories = InWorkspaceIncludedCategories;
			WorkspaceExcludedCategories = InWorkspaceExcludedCategories;
			WorkspaceView = InWorkspaceView;
			bWorkspaceSyncAllProjects = bInWorkspaceProjectOnly;
			bWorkspaceIncludeAllProjectsInSolution = bInWorkspaceIncludeAllProjectsInSolution;

			GlobalControl.SetView(GlobalView);
			SetExcludedCategories(GlobalControl.CategoriesCheckList, UniqueIdToCategory, GlobalExcludedCategories);
			GlobalControl.SyncAllProjects.Checked = bGlobalSyncAllProjects;
			GlobalControl.IncludeAllProjectsInSolution.Checked = bGlobalIncludeAllProjectsInSolution;

			WorkspaceControl.SetView(WorkspaceView);
			SetExcludedCategories(WorkspaceControl.CategoriesCheckList, UniqueIdToCategory, UserSettings.GetEffectiveExcludedCategories(GlobalExcludedCategories, WorkspaceIncludedCategories, WorkspaceExcludedCategories));
			WorkspaceControl.SyncAllProjects.Checked = bWorkspaceSyncAllProjects ?? bGlobalSyncAllProjects;
			WorkspaceControl.IncludeAllProjectsInSolution.Checked = bWorkspaceIncludeAllProjectsInSolution ?? bGlobalIncludeAllProjectsInSolution;

			GlobalControl.CategoriesCheckList.ItemCheck += GlobalControl_CategoriesCheckList_ItemCheck;
			GlobalControl.SyncAllProjects.CheckStateChanged += GlobalControl_SyncAllProjects_CheckStateChanged;
			GlobalControl.IncludeAllProjectsInSolution.CheckStateChanged += GlobalControl_IncludeAllProjectsInSolution_CheckStateChanged;
		}

		private void GlobalControl_CategoriesCheckList_ItemCheck(object sender, ItemCheckEventArgs e)
		{
			WorkspaceControl.CategoriesCheckList.SetItemCheckState(e.Index, e.NewValue);
		}

		private void GlobalControl_SyncAllProjects_CheckStateChanged(object sender, EventArgs e)
		{
			WorkspaceControl.SyncAllProjects.Checked = GlobalControl.SyncAllProjects.Checked;
		}

		private void GlobalControl_IncludeAllProjectsInSolution_CheckStateChanged(object sender, EventArgs e)
		{
			WorkspaceControl.IncludeAllProjectsInSolution.Checked = GlobalControl.IncludeAllProjectsInSolution.Checked;
		}

		private static void SetExcludedCategories(CheckedListBox ListBox, Dictionary<Guid, WorkspaceSyncCategory> UniqueIdToFilter, Guid[] ExcludedCategories)
		{
			ListBox.Items.Clear();
			foreach(WorkspaceSyncCategory Filter in UniqueIdToFilter.Values)
			{
				CheckState State = CheckState.Checked;
				if(ExcludedCategories.Contains(Filter.UniqueId))
				{
					State = CheckState.Unchecked;
				}
				ListBox.Items.Add(Filter, State);
			}
		}

		private void GetExcludedCategories(out Guid[] NewGlobalExcludedCategories, out Guid[] NewWorkspaceIncludedCategories, out Guid[] NewWorkspaceExcludedCategories)
		{
			NewGlobalExcludedCategories = GetExcludedCategories(GlobalControl.CategoriesCheckList);

			Guid[] AllWorkspaceExcludedCategories = GetExcludedCategories(WorkspaceControl.CategoriesCheckList);
			NewWorkspaceIncludedCategories = NewGlobalExcludedCategories.Except(AllWorkspaceExcludedCategories).ToArray();
			NewWorkspaceExcludedCategories = AllWorkspaceExcludedCategories.Except(NewGlobalExcludedCategories).ToArray();
		}

		private static Guid[] GetExcludedCategories(CheckedListBox ListBox)
		{
			HashSet<Guid> ExcludedCategories = new HashSet<Guid>();
			for(int Idx = 0; Idx < ListBox.Items.Count; Idx++)
			{
				Guid UniqueId = ((WorkspaceSyncCategory)ListBox.Items[Idx]).UniqueId;
				if(ListBox.GetItemCheckState(Idx) == CheckState.Unchecked)
				{
					ExcludedCategories.Add(UniqueId);
				}
				else
				{
					ExcludedCategories.Remove(UniqueId);
				}
			}
			return ExcludedCategories.ToArray();
		}

		private static string[] GetView(TextBox FilterText)
		{
			List<string> NewLines = new List<string>(FilterText.Lines);
			while (NewLines.Count > 0 && NewLines.Last().Trim().Length == 0)
			{
				NewLines.RemoveAt(NewLines.Count - 1);
			}
			return NewLines.Count > 0 ? FilterText.Lines : new string[0];
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			string[] NewGlobalView = GlobalControl.GetView();
			string[] NewWorkspaceView = WorkspaceControl.GetView();

			if(NewGlobalView.Any(x => x.Contains("//")) || NewWorkspaceView.Any(x => x.Contains("//")))
			{
				if(MessageBox.Show(this, "Custom views should be relative to the stream root (eg. -/Engine/...).\r\n\r\nFull depot paths (eg. //depot/...) will not match any files.\r\n\r\nAre you sure you want to continue?", "Invalid view", MessageBoxButtons.OKCancel) != System.Windows.Forms.DialogResult.OK)
				{
					return;
				}
			}
		
			GlobalView = NewGlobalView;
			bGlobalSyncAllProjects = GlobalControl.SyncAllProjects.Checked;
			bGlobalIncludeAllProjectsInSolution = GlobalControl.IncludeAllProjectsInSolution.Checked;

			WorkspaceView = NewWorkspaceView;
			bWorkspaceSyncAllProjects = (WorkspaceControl.SyncAllProjects.Checked == bGlobalSyncAllProjects)? (bool?)null : WorkspaceControl.SyncAllProjects.Checked;
			bWorkspaceIncludeAllProjectsInSolution = (WorkspaceControl.IncludeAllProjectsInSolution.Checked == bGlobalIncludeAllProjectsInSolution)? (bool?)null : WorkspaceControl.IncludeAllProjectsInSolution.Checked;

			GetExcludedCategories(out GlobalExcludedCategories, out WorkspaceIncludedCategories, out WorkspaceExcludedCategories);

			DialogResult = DialogResult.OK;
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
		}

		private void ShowCombinedView_Click(object sender, EventArgs e)
		{
			Guid[] NewGlobalExcludedCategories;
			Guid[] NewWorkspaceIncludedCategories;
			Guid[] NewWorkspaceExcludedCategories;
			GetExcludedCategories(out NewGlobalExcludedCategories, out NewWorkspaceIncludedCategories, out NewWorkspaceExcludedCategories);

			string[] Filter = UserSettings.GetCombinedSyncFilter(UniqueIdToCategory, GlobalControl.GetView(), NewGlobalExcludedCategories, WorkspaceControl.GetView(), NewWorkspaceIncludedCategories, NewWorkspaceExcludedCategories);
			if(Filter.Length == 0)
			{
				Filter = new string[]{ "All files will be synced." };
			}
			MessageBox.Show(String.Join("\r\n", Filter), "Combined View");
		}
	}
}
