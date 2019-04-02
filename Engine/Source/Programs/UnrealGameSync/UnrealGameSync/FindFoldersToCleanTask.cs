// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class FolderToClean
	{
		public DirectoryInfo Directory;
		public Dictionary<string, FolderToClean> NameToSubFolder = new Dictionary<string, FolderToClean>(StringComparer.InvariantCultureIgnoreCase);
		public Dictionary<string, FileInfo> NameToFile = new Dictionary<string, FileInfo>(StringComparer.InvariantCultureIgnoreCase);
		public List<FileInfo> FilesToDelete = new List<FileInfo>();
		public List<FileInfo> FilesToSync = new List<FileInfo>();
		public bool bEmptyLeaf = false;
		public bool bEmptyAfterClean = true;

		public FolderToClean(DirectoryInfo InDirectory)
		{
			Directory = InDirectory;
		}

		public string Name
		{
			get { return Directory.Name; }
		}

		public override string ToString()
		{
			return Directory.FullName;
		}
	}

	class FindFoldersToCleanTask : IModalTask, IDisposable
	{
		class PerforceHaveFolder
		{
			public Dictionary<string, PerforceHaveFolder> NameToSubFolder = new Dictionary<string,PerforceHaveFolder>(StringComparer.InvariantCultureIgnoreCase);
			public Dictionary<string, PerforceFileRecord> NameToFile = new Dictionary<string,PerforceFileRecord>(StringComparer.InvariantCultureIgnoreCase);
		}

		PerforceConnection PerforceClient;
		string ClientRootPath;
		IReadOnlyList<string> SyncPaths;
		TextWriter Log;
		FolderToClean RootFolderToClean;

		int RemainingFoldersToScan;
		ManualResetEvent FinishedScan = new ManualResetEvent(false);
		bool bAbortScan;

		public List<string> FileNames = new List<string>();

		public FindFoldersToCleanTask(PerforceConnection InPerforceClient, FolderToClean InRootFolderToClean, string InClientRootPath, IReadOnlyList<string> InSyncPaths, TextWriter InLog)
		{
			PerforceClient = InPerforceClient;
			ClientRootPath = InClientRootPath.TrimEnd('/') + "/";
			SyncPaths = new List<string>(InSyncPaths);
			Log = InLog;
			RootFolderToClean = InRootFolderToClean;
			FinishedScan = new ManualResetEvent(true);
		}

		void QueueFolderToPopulate(FolderToClean Folder)
		{
			if(Interlocked.Increment(ref RemainingFoldersToScan) == 1)
			{
				FinishedScan.Reset();
			}
			ThreadPool.QueueUserWorkItem(x => PopulateFolder(Folder));
		}

		void PopulateFolder(FolderToClean Folder)
		{
			if(!bAbortScan)
			{
				if((Folder.Directory.Attributes & FileAttributes.ReparsePoint) == 0)
				{
					foreach(DirectoryInfo SubDirectory in Folder.Directory.EnumerateDirectories())
					{
						FolderToClean SubFolder = new FolderToClean(SubDirectory);
						Folder.NameToSubFolder[SubFolder.Name] = SubFolder;
						QueueFolderToPopulate(SubFolder);
					}
					foreach(FileInfo File in Folder.Directory.EnumerateFiles())
					{
						FileAttributes Attributes = File.Attributes; // Force the value to be cached.
						Folder.NameToFile[File.Name] = File;
					}
				}
			}

			if(Interlocked.Decrement(ref RemainingFoldersToScan) == 0)
			{
				FinishedScan.Set();
			}
		}

		void MergeTrees(FolderToClean LocalFolder, PerforceHaveFolder PerforceFolder, HashSet<string> OpenClientPaths, string PerforceConfigFile)
		{
			if(PerforceFolder == null)
			{
				// Loop through all the local sub-folders
				foreach(FolderToClean LocalSubFolder in LocalFolder.NameToSubFolder.Values)
				{
					MergeTrees(LocalSubFolder, null, OpenClientPaths, PerforceConfigFile);
				}

				// Delete everything
				LocalFolder.FilesToDelete.AddRange(LocalFolder.NameToFile.Values);
			}
			else
			{
				// Loop through all the local sub-folders
				foreach(FolderToClean LocalSubFolder in LocalFolder.NameToSubFolder.Values)
				{
					PerforceHaveFolder PerforceSubFolder;
					PerforceFolder.NameToSubFolder.TryGetValue(LocalSubFolder.Name, out PerforceSubFolder);
					MergeTrees(LocalSubFolder, PerforceSubFolder, OpenClientPaths, PerforceConfigFile);
				}

				// Also merge all the Perforce folders that no longer exist
				foreach(KeyValuePair<string, PerforceHaveFolder> PerforceSubFolderPair in PerforceFolder.NameToSubFolder)
				{
					FolderToClean LocalSubFolder;
					if(!LocalFolder.NameToSubFolder.TryGetValue(PerforceSubFolderPair.Key, out LocalSubFolder))
					{
						LocalSubFolder = new FolderToClean(new DirectoryInfo(Path.Combine(LocalFolder.Directory.FullName, PerforceSubFolderPair.Key)));
						MergeTrees(LocalSubFolder, PerforceSubFolderPair.Value, OpenClientPaths, PerforceConfigFile);
						LocalFolder.NameToSubFolder.Add(LocalSubFolder.Name, LocalSubFolder);
					}
				}

				// Find all the files that need to be re-synced
				foreach(KeyValuePair<string, PerforceFileRecord> FilePair in PerforceFolder.NameToFile)
				{
					FileInfo LocalFile;
					if(!LocalFolder.NameToFile.TryGetValue(FilePair.Key, out LocalFile))
					{
						LocalFolder.FilesToSync.Add(new FileInfo(Path.Combine(LocalFolder.Directory.FullName, FilePair.Key)));
					}
					else if((FilePair.Value.Flags & PerforceFileFlags.AlwaysWritable) == 0 && (LocalFile.Attributes & FileAttributes.ReadOnly) == 0 && !OpenClientPaths.Contains(FilePair.Value.ClientPath))
					{
						LocalFolder.FilesToSync.Add(LocalFile);
					}
				}

				// Find all the files that should be deleted
				foreach(FileInfo LocalFileInfo in LocalFolder.NameToFile.Values)
				{
					if(!PerforceFolder.NameToFile.ContainsKey(LocalFileInfo.Name) && !OpenClientPaths.Contains(LocalFileInfo.FullName))
					{
						LocalFolder.FilesToDelete.Add(LocalFileInfo);
					}
				}
			}

			// Remove any config files
			if(PerforceConfigFile != null)
			{
				LocalFolder.FilesToDelete.RemoveAll(x => String.Compare(x.Name, PerforceConfigFile, StringComparison.OrdinalIgnoreCase) == 0);
			}

			// Figure out if this folder is just an empty directory that needs to be removed
			LocalFolder.bEmptyLeaf = LocalFolder.NameToFile.Count == 0 && LocalFolder.NameToSubFolder.Count == 0 && LocalFolder.FilesToSync.Count == 0;

			// Figure out if it the folder will be empty after the clean operation
			LocalFolder.bEmptyAfterClean = LocalFolder.NameToSubFolder.Values.All(x => x.bEmptyAfterClean) && LocalFolder.FilesToDelete.Count == LocalFolder.NameToFile.Count && LocalFolder.FilesToSync.Count == 0;
		}

		void RemoveEmptyFolders(FolderToClean Folder)
		{
			foreach(FolderToClean SubFolder in Folder.NameToSubFolder.Values)
			{
				RemoveEmptyFolders(SubFolder);
			}

			Folder.NameToSubFolder = Folder.NameToSubFolder.Values.Where(x => x.NameToSubFolder.Count > 0 || x.FilesToSync.Count > 0 || x.FilesToDelete.Count > 0 || x.bEmptyLeaf).ToDictionary(x => x.Name, x => x, StringComparer.InvariantCultureIgnoreCase);
		}

		public void Dispose()
		{
			bAbortScan = true;

			if(FinishedScan != null)
			{
				FinishedScan.WaitOne();
				FinishedScan.Dispose();
				FinishedScan = null;
			}
		}

		public bool Run(out string ErrorMessage)
		{
			Log.WriteLine("Finding files in workspace...");
			Log.WriteLine();

			// Start enumerating all the files that exist locally
			foreach(string SyncPath in SyncPaths)
			{
				Debug.Assert(SyncPath.StartsWith(ClientRootPath));
				if(SyncPath.StartsWith(ClientRootPath, StringComparison.InvariantCultureIgnoreCase))
				{
					string[] Fragments = SyncPath.Substring(ClientRootPath.Length).Split('/');

					FolderToClean SyncFolder = RootFolderToClean;
					for(int Idx = 0; Idx < Fragments.Length - 1; Idx++)
					{
						FolderToClean NextSyncFolder;
						if(!SyncFolder.NameToSubFolder.TryGetValue(Fragments[Idx], out NextSyncFolder))
						{
							NextSyncFolder = new FolderToClean(new DirectoryInfo(Path.Combine(SyncFolder.Directory.FullName, Fragments[Idx])));
							SyncFolder.NameToSubFolder[NextSyncFolder.Name] = NextSyncFolder;
						}
						SyncFolder = NextSyncFolder;
					}

					string Wildcard = Fragments[Fragments.Length - 1];
					if(Wildcard == "...")
					{
						QueueFolderToPopulate(SyncFolder);
					}
					else
					{
						if(SyncFolder.Directory.Exists)
						{
							foreach(FileInfo File in SyncFolder.Directory.EnumerateFiles(Wildcard))
							{
								SyncFolder.NameToFile[File.Name] = File;
							}
						}
					}
				}
			}

			// Get the prefix for any local file
			string LocalRootPrefix = RootFolderToClean.Directory.FullName + Path.DirectorySeparatorChar;

			// Query the have table and build a separate tree from it
			PerforceHaveFolder RootHaveFolder = new PerforceHaveFolder();
			foreach(string SyncPath in SyncPaths)
			{
				List<PerforceFileRecord> FileRecords;
				if(!PerforceClient.Stat(String.Format("{0}#have", SyncPath), out FileRecords, Log))
				{
					ErrorMessage = "Couldn't query have table from Perforce.";
					return false;
				}
				foreach(PerforceFileRecord FileRecord in FileRecords)
				{
					if(!FileRecord.ClientPath.StartsWith(LocalRootPrefix, StringComparison.InvariantCultureIgnoreCase))
					{
						ErrorMessage = String.Format("Failed to get have table; file '{0}' doesn't start with root path ('{1}')", FileRecord.ClientPath, RootFolderToClean.Directory.FullName);
						return false;
					}

					string[] Tokens = FileRecord.ClientPath.Substring(LocalRootPrefix.Length).Split('/', '\\');

					PerforceHaveFolder FileFolder = RootHaveFolder;
					for(int Idx = 0; Idx < Tokens.Length - 1; Idx++)
					{
						PerforceHaveFolder NextFileFolder;
						if(!FileFolder.NameToSubFolder.TryGetValue(Tokens[Idx], out NextFileFolder))
						{
							NextFileFolder = new PerforceHaveFolder();
							FileFolder.NameToSubFolder.Add(Tokens[Idx], NextFileFolder);
						}
						FileFolder = NextFileFolder;
					}
					FileFolder.NameToFile[Tokens[Tokens.Length - 1]] = FileRecord;
				}
			}

			// Find all the files which are currently open for edit. We don't want to force sync these.
			List<PerforceFileRecord> OpenFileRecords;
			if(!PerforceClient.GetOpenFiles("//...", out OpenFileRecords, Log))
			{
				ErrorMessage = "Couldn't query open files from Perforce.";
				return false;
			}

			// Build a set of all the open local files
			HashSet<string> OpenLocalFiles = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
			foreach(PerforceFileRecord OpenFileRecord in OpenFileRecords)
			{
				if(!OpenFileRecord.ClientPath.StartsWith(ClientRootPath, StringComparison.InvariantCultureIgnoreCase))
				{
					ErrorMessage = String.Format("Failed to get open file list; file '{0}' doesn't start with client root path ('{1}')", OpenFileRecord.ClientPath, ClientRootPath);
					return false;
				}
				OpenLocalFiles.Add(LocalRootPrefix + PerforceUtils.UnescapePath(OpenFileRecord.ClientPath).Substring(ClientRootPath.Length).Replace('/', Path.DirectorySeparatorChar));
			}

			// Wait to finish scanning the directory
			FinishedScan.WaitOne();

			// Find the value of the P4CONFIG variable
			string PerforceConfigFile;
			PerforceClient.GetSetting("P4CONFIG", out PerforceConfigFile, Log);

			// Merge the trees
			MergeTrees(RootFolderToClean, RootHaveFolder, OpenLocalFiles, PerforceConfigFile);

			// Remove all the empty folders
			RemoveEmptyFolders(RootFolderToClean);
			ErrorMessage = null;
			return true;
		}
	}
}
