// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	public partial class ProgramsRunningWindow : Form
	{
		object SyncObject = new object();
		string[] Programs;
		Func<string[]> EnumeratePrograms;
		ManualResetEvent TerminateEvent;
		Thread BackgroundThread;

		public ProgramsRunningWindow(Func<string[]> EnumeratePrograms, string[] Programs)
		{
			InitializeComponent();

			this.Programs = Programs.OrderBy(x => x).ToArray();
			this.EnumeratePrograms = EnumeratePrograms;
			this.ProgramListBox.Items.AddRange(Programs);
		}

		private void ProgramsRunningWindow_Load(object sender, EventArgs e)
		{
			TerminateEvent = new ManualResetEvent(false);

			BackgroundThread = new Thread(() => ExecuteBackgroundWork());
			BackgroundThread.Start();
		}

		private void ExecuteBackgroundWork()
		{
			for(;;)
			{
				if(TerminateEvent.WaitOne(TimeSpan.FromSeconds(2.0)))
				{
					break;
				}

				string[] NewPrograms = EnumeratePrograms().OrderBy(x => x).ToArray();
				lock(SyncObject)
				{
					if(!Enumerable.SequenceEqual(Programs, NewPrograms))
					{
						Programs = NewPrograms;
						BeginInvoke(new MethodInvoker(() => UpdatePrograms()));
					}
				}
			}
		}

		private void UpdatePrograms()
		{
			ProgramListBox.Items.Clear();
			ProgramListBox.Items.AddRange(Programs);

			if(Programs.Length == 0)
			{
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void ProgramsRunningWindow_FormClosed(object sender, FormClosedEventArgs e)
		{
			if(BackgroundThread != null)
			{
				TerminateEvent.Set();

				BackgroundThread.Join();
				BackgroundThread = null;

				TerminateEvent.Dispose();
				TerminateEvent = null;
			}
		}
	}
}
