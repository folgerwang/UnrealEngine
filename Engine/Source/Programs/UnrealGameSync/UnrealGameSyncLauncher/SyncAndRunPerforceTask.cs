// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	class SyncAndRunPerforceTask : IPerforceModalTask
	{
		public delegate bool SyncAndRunDelegate(PerforceConnection Perforce, TextWriter LogWriter);

		SyncAndRunDelegate SyncAndRun;

		public SyncAndRunPerforceTask(SyncAndRunDelegate SyncAndRun)
		{
			this.SyncAndRun = SyncAndRun;
		}

		public bool Run(PerforceConnection Perforce, TextWriter LogWriter, out string ErrorMessage)
		{
			if(SyncAndRun(Perforce, LogWriter))
			{
				ErrorMessage = null;
				return true;
			}
			else
			{
				ErrorMessage = "Failed to sync application.";
				return false;
			}
		}
	}
}
