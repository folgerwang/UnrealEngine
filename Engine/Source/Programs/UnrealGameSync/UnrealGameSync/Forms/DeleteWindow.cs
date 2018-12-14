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
	public partial class DeleteWindow : Form
	{
		Dictionary<string, bool> FilesToDelete;

		public DeleteWindow(Dictionary<string, bool> InFilesToDelete)
		{
			InitializeComponent();

			FilesToDelete = InFilesToDelete;

			foreach(KeyValuePair<string, bool> FileToDelete in FilesToDelete)
			{
				ListViewItem Item = new ListViewItem(FileToDelete.Key);
				Item.Tag = FileToDelete.Key;
				Item.Checked = FileToDelete.Value;
				FileList.Items.Add(Item);
			}
		}

		private void UncheckAll_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in FileList.Items)
			{
				Item.Checked = false;
			}
		}

		private void CheckAll_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in FileList.Items)
			{
				Item.Checked = true;
			}
		}

		private void ContinueButton_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem Item in FileList.Items)
			{
				FilesToDelete[(string)Item.Tag] = Item.Checked;
			}
		}
	}
}
