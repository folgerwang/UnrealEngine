// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	class SyncAndRunModalTask : IModalTask
	{
		public delegate bool SyncAndRunDelegate(PerforceConnection Perforce, TextWriter LogWriter);

		public string ServerAndPort;
		public string UserName;
		SyncAndRunDelegate SyncAndRun;
		public StringWriter LogWriter;

		public SyncAndRunModalTask(string ServerAndPort, string UserName, SyncAndRunDelegate SyncAndRun)
		{
			this.ServerAndPort = ServerAndPort;
			this.UserName = UserName;
			this.SyncAndRun = SyncAndRun;
			this.LogWriter = new StringWriter();
		}

		public bool Run(out string ErrorMessage)
		{
			if(!PerforceModalTask.TryGetServerSettings(null, ref ServerAndPort, ref UserName, LogWriter))
			{
				ErrorMessage = "Unable to get Perforce settings.";
				return false;
			}

			PerforceConnection Perforce = new PerforceConnection(UserName, null, ServerAndPort);
			if(!SyncAndRun(Perforce, LogWriter))
			{
				ErrorMessage = "Unable to sync and run application.";
				return false;
			}

			ErrorMessage = null;
			return true;
		}
	}
}
