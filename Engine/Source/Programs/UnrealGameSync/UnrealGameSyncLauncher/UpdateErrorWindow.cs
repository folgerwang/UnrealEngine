// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSyncLauncher
{
	public partial class UpdateErrorWindow : Form
	{
		[DllImport("user32.dll")]
		private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, int wParam, [MarshalAs(UnmanagedType.LPWStr)] string lParam);

		const int EM_SETCUEBANNER = 0x1501;

		public string LogText;
		public string Server;
		public string DepotPath;

		public UpdateErrorWindow(string LogText, string Server, string DepotPath)
		{
			InitializeComponent();

			this.LogText = LogText;
			this.Server = Server;
			this.DepotPath = DepotPath;

			ServerTextBox.Text = Server;
			DepotPathTextBox.Text = DepotPath;
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			SendMessage(ServerTextBox.Handle, EM_SETCUEBANNER, 1, "Perforce Default");
		}

		private void ViewLogBtn_Click(object sender, EventArgs e)
		{
			UpdateLogWindow Log = new UpdateLogWindow(LogText);
			Log.ShowDialog(this);
		}

		private void RetryBtn_Click(object sender, EventArgs e)
		{
			Server = ServerTextBox.Text;
			DepotPath = DepotPathTextBox.Text;
			DialogResult = DialogResult.OK;
			Close();
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
