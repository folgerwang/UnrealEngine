// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class DeleteFilesTask : IModalTask
	{
		PerforceConnection Perforce;
		List<FileInfo> FilesToSync;
		List<FileInfo> FilesToDelete;
		List<DirectoryInfo> DirectoriesToDelete;

		public DeleteFilesTask(PerforceConnection Perforce, List<FileInfo> FilesToSync, List<FileInfo> FilesToDelete, List<DirectoryInfo> DirectoriesToDelete)
		{
			this.Perforce = Perforce;
			this.FilesToSync = FilesToSync;
			this.FilesToDelete = FilesToDelete;
			this.DirectoriesToDelete = DirectoriesToDelete;
		}

		public bool Run(out string ErrorMessage)
		{
			StringBuilder FailMessage = new StringBuilder();

			foreach(FileInfo FileToSync in FilesToSync)
			{
				StringWriter Log = new StringWriter();
				if(!Perforce.ForceSync(String.Format("{0}#have", FileToSync.FullName), Log))
				{
					FailMessage.AppendFormat("{0} ({1})\r\n", FileToSync.FullName, Log.ToString().Trim().Replace('\n', ' '));
				}
			}

			foreach(FileInfo FileToDelete in FilesToDelete)
			{
				try
				{
					FileToDelete.IsReadOnly = false;
					FileToDelete.Delete();
				}
				catch(Exception Ex)
				{
					FailMessage.AppendFormat("{0} ({1})\r\n", FileToDelete.FullName, Ex.Message.Trim());
				}
			}
			foreach(DirectoryInfo DirectoryToDelete in DirectoriesToDelete)
			{
				try
				{
					DirectoryToDelete.Delete(true);
				}
				catch(Exception Ex)
				{
					FailMessage.AppendFormat("{0} ({1})\r\n", DirectoryToDelete.FullName, Ex.Message.Trim());
				}
			}

			if(FailMessage.Length == 0)
			{
				ErrorMessage = null;
				return true;
			}
			else 
			{
				ErrorMessage = FailMessage.ToString();
				return false;
			}
		}
	}
}
