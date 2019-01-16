// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ModalTaskWindow : Form
	{
		IModalTask Task;
		SynchronizationContext SyncContext;
		Thread BackgroundThread;
		ModalTaskResult? PendingResult;

		public ModalTaskResult Result
		{
			get { return PendingResult.Value; }
		}

		public string ErrorMessage
		{
			get;
			private set;
		}

		public event Action Complete;

		public ModalTaskWindow(IModalTask InTask, string InTitle, string InMessage, FormStartPosition InStartPosition)
		{
			InitializeComponent();

			Task = InTask;
			Text = InTitle;
			MessageLabel.Text = InMessage;
			StartPosition = InStartPosition;

			SyncContext = SynchronizationContext.Current;

			BackgroundThread = new Thread(x => ThreadProc());
			BackgroundThread.Start();
		}

		public void ShowAndActivate()
		{
			Show();
			Activate();
		}

		private void ThreadProc()
		{
			string ErrorMessageValue;
			if(Task.Run(out ErrorMessageValue))
			{
				PendingResult = ModalTaskResult.Succeeded;
			}
			else
			{
				ErrorMessage =  ErrorMessageValue;
				PendingResult = ModalTaskResult.Failed;
			}
			SyncContext.Post((o) => ThreadCompleteCallback(), null);
		}

		private void ThreadCompleteCallback()
		{
			if(BackgroundThread != null)
			{
				BackgroundThread.Abort();
				BackgroundThread.Join();
				BackgroundThread = null;

				if(!PendingResult.HasValue)
				{
					PendingResult = ModalTaskResult.Aborted;
				}

				if(Complete != null)
				{
					Complete();
				}
			}
		}

		private void ModalTaskWindow_FormClosing(object sender, FormClosingEventArgs e)
		{
			ThreadCompleteCallback();
		}
	}
}
