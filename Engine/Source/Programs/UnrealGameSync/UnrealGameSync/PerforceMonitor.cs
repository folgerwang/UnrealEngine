// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class PerforceChangeDetails
	{
		public string Description;
		public bool bContainsCode;
		public bool bContainsContent;
	}

	class PerforceMonitor : IDisposable
	{
		class PerforceChangeSorter : IComparer<PerforceChangeSummary>
		{
			public int Compare(PerforceChangeSummary SummaryA, PerforceChangeSummary SummaryB)
			{
				return SummaryB.Number - SummaryA.Number;
			}
		}

		PerforceConnection Perforce;
		readonly string BranchClientPath;
		readonly string SelectedClientFileName;
		readonly string SelectedProjectIdentifier;
		Thread WorkerThread;
		int PendingMaxChangesValue;
		SortedSet<PerforceChangeSummary> Changes = new SortedSet<PerforceChangeSummary>(new PerforceChangeSorter());
		SortedDictionary<int, PerforceChangeDetails> ChangeDetails = new SortedDictionary<int,PerforceChangeDetails>();
		SortedSet<int> PromotedChangeNumbers = new SortedSet<int>();
		SortedList<int, string> ChangeNumberToZippedBinaries = new SortedList<int,string>();
		AutoResetEvent RefreshEvent = new AutoResetEvent(false);
		BoundedLogWriter LogWriter;
		bool bIsEnterpriseProject;
		bool bDisposing;
		List<KeyValuePair<string, DateTime>> LocalConfigFiles;

		public event Action OnUpdate;
		public event Action OnUpdateMetadata;
		public event Action OnStreamChange;
		public event Action OnLoginExpired;

		public PerforceMonitor(PerforceConnection InPerforce, string InBranchClientPath, string InSelectedClientFileName, string InSelectedProjectIdentifier, string InLogPath, bool bInIsEnterpriseProject, ConfigFile InProjectConfigFile, List<KeyValuePair<string, DateTime>> InLocalConfigFiles)
		{
			Perforce = InPerforce;
			BranchClientPath = InBranchClientPath;
			SelectedClientFileName = InSelectedClientFileName;
			SelectedProjectIdentifier = InSelectedProjectIdentifier;
			PendingMaxChangesValue = 100;
			LastChangeByCurrentUser = -1;
			LastCodeChangeByCurrentUser = -1;
			OtherStreamNames = new List<string>();
			bIsEnterpriseProject = bInIsEnterpriseProject;
			LatestProjectConfigFile = InProjectConfigFile;
			LocalConfigFiles = InLocalConfigFiles;

			LogWriter = new BoundedLogWriter(InLogPath);
		}

		public void Start()
		{
			WorkerThread = new Thread(() => PollForUpdates());
			WorkerThread.Start();
		}

		public void Dispose()
		{
			bDisposing = true;
			if(WorkerThread != null)
			{
				RefreshEvent.Set();
				if(!WorkerThread.Join(100))
				{
					WorkerThread.Abort();
				}
				WorkerThread = null;
			}
			if(LogWriter != null)
			{
				LogWriter.Dispose();
				LogWriter = null;
			}
		}

		public bool IsActive
		{
			get;
			set;
		}

		public string LastStatusMessage
		{
			get;
			private set;
		}

		public int CurrentMaxChanges
		{
			get;
			private set;
		}

		public int PendingMaxChanges
		{
			get { return PendingMaxChangesValue; }
			set { lock(this){ if(value != PendingMaxChangesValue){ PendingMaxChangesValue = value; RefreshEvent.Set(); } } }
		}

		public IReadOnlyList<string> OtherStreamNames
		{
			get;
			private set;
		}

		void PollForUpdates()
		{
			string StreamName;
			if(!Perforce.GetActiveStream(out StreamName, LogWriter))
			{
				StreamName = null;
			}

			// Try to update the zipped binaries list before anything else, because it causes a state change in the UI
			UpdateZippedBinaries();

			while(!bDisposing)
			{
				Stopwatch Timer = Stopwatch.StartNew();

				// Check we still have a valid login ticket
				bool bLoggedIn;
				if(Perforce.GetLoggedInState(out bLoggedIn, LogWriter))
				{
					if(!bLoggedIn)
					{
						LastStatusMessage = "User is not logged in";
						OnLoginExpired();
					}
					else
					{
						// Check we haven't switched streams
						string NewStreamName;
						if(Perforce.GetActiveStream(out NewStreamName, LogWriter) && NewStreamName != StreamName)
						{
							OnStreamChange();
						}

						// Update the stream list
						if(StreamName != null)
						{
							List<string> NewOtherStreamNames;
							if(!Perforce.FindStreams(PerforceUtils.GetClientOrDepotDirectoryName(StreamName) + "/*", out NewOtherStreamNames, LogWriter))
							{
								NewOtherStreamNames = new List<string>();
							}
							OtherStreamNames = NewOtherStreamNames;
						}

						// Check for any p4 changes
						if(!UpdateChanges())
						{
							LastStatusMessage = "Failed to update changes";
						}
						else if(!UpdateChangeTypes())
						{
							LastStatusMessage = "Failed to update change types";
						}
						else if(!UpdateZippedBinaries())
						{
							LastStatusMessage = "Failed to update zipped binaries list";
						}
						else
						{
							LastStatusMessage = String.Format("Last update took {0}ms", Timer.ElapsedMilliseconds);
						}
					}
				}

				// Wait for another request, or scan for new builds after a timeout
				RefreshEvent.WaitOne((IsActive? 2 : 10) * 60 * 1000);
			}
		}

		bool UpdateChanges()
		{
			// Get the current status of the build
			int MaxChanges;
			int OldestChangeNumber = -1;
			int NewestChangeNumber = -1;
			HashSet<int> CurrentChangelists;
			SortedSet<int> PrevPromotedChangelists;
			lock(this)
			{
				MaxChanges = PendingMaxChanges;
				if(Changes.Count > 0)
				{
					NewestChangeNumber = Changes.First().Number;
					OldestChangeNumber = Changes.Last().Number;
				}
				CurrentChangelists = new HashSet<int>(Changes.Select(x => x.Number));
				PrevPromotedChangelists = new SortedSet<int>(PromotedChangeNumbers);
			}

			// Build a full list of all the paths to sync
			List<string> DepotPaths = new List<string>();
			if(SelectedClientFileName.EndsWith(".uprojectdirs", StringComparison.InvariantCultureIgnoreCase))
			{
				DepotPaths.Add(String.Format("{0}/...", BranchClientPath));
			}
			else
			{
				DepotPaths.Add(String.Format("{0}/*", BranchClientPath));
				DepotPaths.Add(String.Format("{0}/Engine/...", BranchClientPath));
				DepotPaths.Add(String.Format("{0}/...", PerforceUtils.GetClientOrDepotDirectoryName(SelectedClientFileName)));
				if (bIsEnterpriseProject)
				{
					DepotPaths.Add(String.Format("{0}/Enterprise/...", BranchClientPath));
				}
			}

			// Read any new changes
			List<PerforceChangeSummary> NewChanges;
			if(MaxChanges > CurrentMaxChanges)
			{
				if(!Perforce.FindChanges(DepotPaths, MaxChanges, out NewChanges, LogWriter))
				{
					return false;
				}
			}
			else
			{
				if(!Perforce.FindChanges(DepotPaths.Select(DepotPath => String.Format("{0}@>{1}", DepotPath, NewestChangeNumber)), -1, out NewChanges, LogWriter))
				{
					return false;
				}
			}

			// Remove anything we already have
			NewChanges.RemoveAll(x => CurrentChangelists.Contains(x.Number));

			// Update the change ranges
			if(NewChanges.Count > 0)
			{
				OldestChangeNumber = Math.Max(OldestChangeNumber, NewChanges.Last().Number);
				NewestChangeNumber = Math.Min(NewestChangeNumber, NewChanges.First().Number);
			}

			// If we are using zipped binaries, make sure we have every change since the last zip containing them. This is necessary for ensuring that content changes show as
			// syncable in the workspace view if there have been a large number of content changes since the last code change.
			int MinZippedChangeNumber = -1;
			foreach(int ChangeNumber in ChangeNumberToZippedBinaries.Keys)
			{
				if(ChangeNumber > MinZippedChangeNumber && ChangeNumber <= OldestChangeNumber)
				{
					MinZippedChangeNumber = ChangeNumber;
				}
			}
			if(MinZippedChangeNumber != -1 && MinZippedChangeNumber < OldestChangeNumber)
			{
				List<PerforceChangeSummary> ZipChanges;
				if(Perforce.FindChanges(DepotPaths.Select(DepotPath => String.Format("{0}@{1},{2}", DepotPath, MinZippedChangeNumber, OldestChangeNumber - 1)), -1, out ZipChanges, LogWriter))
				{
					NewChanges.AddRange(ZipChanges);
				}
			}

			// Fixup any ROBOMERGE authors
			const string RoboMergePrefix = "#ROBOMERGE-AUTHOR:";
			foreach(PerforceChangeSummary Change in NewChanges)
			{
				if(Change.Description.StartsWith(RoboMergePrefix))
				{
					int StartIdx = RoboMergePrefix.Length;
					while(StartIdx < Change.Description.Length && Change.Description[StartIdx] == ' ')
					{
						StartIdx++;
					}

					int EndIdx = StartIdx;
					while(EndIdx < Change.Description.Length && !Char.IsWhiteSpace(Change.Description[EndIdx]))
					{
						EndIdx++;
					}

					if(EndIdx > StartIdx)
					{
						Change.User = Change.Description.Substring(StartIdx, EndIdx - StartIdx);
						Change.Description = "ROBOMERGE: " + Change.Description.Substring(EndIdx).TrimStart();
					}
				}
			}

			// Process the new changes received
			if(NewChanges.Count > 0 || MaxChanges < CurrentMaxChanges)
			{
				// Insert them into the builds list
				lock(this)
				{
					Changes.UnionWith(NewChanges);
					if(Changes.Count > MaxChanges)
					{
						// Remove changes to shrink it to the max requested size, being careful to avoid removing changes that would affect our ability to correctly
						// show the availability for content changes using zipped binaries.
						SortedSet<PerforceChangeSummary> TrimmedChanges = new SortedSet<PerforceChangeSummary>(new PerforceChangeSorter());
						foreach(PerforceChangeSummary Change in Changes)
						{
							TrimmedChanges.Add(Change);
							if(TrimmedChanges.Count >= MaxChanges && (ChangeNumberToZippedBinaries.Count == 0 || ChangeNumberToZippedBinaries.ContainsKey(Change.Number) || ChangeNumberToZippedBinaries.First().Key > Change.Number))
							{
								break;
							}
						}
						Changes = TrimmedChanges;
					}
					CurrentMaxChanges = MaxChanges;
				}

				// Find the last submitted change by the current user
				int NewLastChangeByCurrentUser = -1;
				foreach(PerforceChangeSummary Change in Changes)
				{
					if(String.Compare(Change.User, Perforce.UserName, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						NewLastChangeByCurrentUser = Math.Max(NewLastChangeByCurrentUser, Change.Number);
					}
				}
				LastChangeByCurrentUser = NewLastChangeByCurrentUser;

				// Notify the main window that we've got more data
				if(OnUpdate != null)
				{
					OnUpdate();
				}
			}
			return true;
		}

		public bool UpdateChangeTypes()
		{
			// Find the changes we need to query
			List<int> QueryChangeNumbers = new List<int>();
			lock(this)
			{
				foreach(PerforceChangeSummary Change in Changes)
				{
					if(!ChangeDetails.ContainsKey(Change.Number))
					{
						QueryChangeNumbers.Add(Change.Number);
					}
				}
			}

			// Update them in batches
			foreach(int QueryChangeNumber in QueryChangeNumbers)
			{
				string[] CodeExtensions = { ".cs", ".h", ".cpp", ".inl", ".usf", ".ush", ".uproject", ".uplugin" };

				// If there's something to check for, find all the content changes after this changelist
				PerforceDescribeRecord DescribeRecord;
				if(Perforce.Describe(QueryChangeNumber, out DescribeRecord, LogWriter))
				{
					// Create the details object
					PerforceChangeDetails Details = new PerforceChangeDetails();
					Details.Description = DescribeRecord.Description;

					// Check whether the files are code or content
					foreach(PerforceDescribeFileRecord File in DescribeRecord.Files)
					{
						if(CodeExtensions.Any(Extension => File.DepotFile.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase)))
						{
							Details.bContainsCode = true;
						}
						else
						{
							Details.bContainsContent = true;
						}
					}

					// Update the type of this change
					lock(this)
					{
						if(!ChangeDetails.ContainsKey(QueryChangeNumber))
						{
							ChangeDetails.Add(QueryChangeNumber, Details);
						}
					}

					// Reload the config file if it changes
					if(DescribeRecord.Files.Any(x => x.DepotFile.EndsWith("/UnrealGameSync.ini", StringComparison.InvariantCultureIgnoreCase)))
					{
						UpdateProjectConfigFile();
					}
				}

				// Find the last submitted code change by the current user
				int NewLastCodeChangeByCurrentUser = -1;
				foreach(PerforceChangeSummary Change in Changes)
				{
					if(String.Compare(Change.User, Perforce.UserName, StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						PerforceChangeDetails Details;
						if(ChangeDetails.TryGetValue(Change.Number, out Details) && Details.bContainsCode)
						{
							NewLastCodeChangeByCurrentUser = Math.Max(NewLastCodeChangeByCurrentUser, Change.Number);
						}
					}
				}
				LastCodeChangeByCurrentUser = NewLastCodeChangeByCurrentUser;

				// Notify the main window that we've got an update
				if(OnUpdateMetadata != null)
				{
					OnUpdateMetadata();
				}
			}

			if(LocalConfigFiles.Any(x => File.GetLastWriteTimeUtc(x.Key) != x.Value))
			{
				UpdateProjectConfigFile();
				if(OnUpdateMetadata != null)
				{
					OnUpdateMetadata();
				}
			}

			return true;
		}

		bool UpdateZippedBinaries()
		{
			string ZippedBinariesPath = null;

			// Find all the zipped binaries under this stream
			ConfigSection ProjectConfigSection = LatestProjectConfigFile.FindSection(SelectedProjectIdentifier);
			if(ProjectConfigSection != null)
			{
				ZippedBinariesPath = ProjectConfigSection.GetValue("ZippedBinariesPath", null);
			}

			// Build a new list of zipped binaries
			SortedList<int, string> NewChangeNumberToZippedBinaries = new SortedList<int,string>();
			if(ZippedBinariesPath != null)
			{
				// Make sure the zipped binaries path exists
				bool bExists;
				if(!Perforce.FileExists(ZippedBinariesPath, out bExists, LogWriter))
				{
					return false;
				}
				if(bExists)
				{
					// Query all the changes to this file
					List<PerforceFileChangeSummary> Changes;
					if(!Perforce.FindFileChanges(ZippedBinariesPath, 100, out Changes, LogWriter))
					{
						return false;
					}

					// Build a new list of zipped binaries
					foreach(PerforceFileChangeSummary Change in Changes)
					{
						if(Change.Action != "purge")
						{
							string[] Tokens = Change.Description.Split(' ');
							if(Tokens[0].StartsWith("[CL") && Tokens[1].EndsWith("]"))
							{
								int OriginalChangeNumber;
								if(int.TryParse(Tokens[1].Substring(0, Tokens[1].Length - 1), out OriginalChangeNumber) && !NewChangeNumberToZippedBinaries.ContainsKey(OriginalChangeNumber))
								{
									NewChangeNumberToZippedBinaries[OriginalChangeNumber] = String.Format("{0}#{1}", ZippedBinariesPath, Change.Revision);
								}
							}
						}
					}
				}
			}

			// Get the new status message
			string NewZippedBinariesStatus;
			if(ZippedBinariesPath == null)
			{
				NewZippedBinariesStatus = String.Format("Precompiled binaries are not available for {0}", SelectedProjectIdentifier);
			}
			else if(NewChangeNumberToZippedBinaries.Count == 0)
			{
				NewZippedBinariesStatus = String.Format("No valid archives found at {0}", ZippedBinariesPath);
			}
			else
			{
				NewZippedBinariesStatus = null;
			}

			// Update the new list of zipped binaries
			if(!ChangeNumberToZippedBinaries.SequenceEqual(NewChangeNumberToZippedBinaries) || ZippedBinariesStatus != NewZippedBinariesStatus)
			{
				ZippedBinariesStatus = NewZippedBinariesStatus;
				ChangeNumberToZippedBinaries = NewChangeNumberToZippedBinaries;
				if(OnUpdateMetadata != null && Changes.Count > 0)
				{
					OnUpdateMetadata();
				}
			}

			return true;
		}

		void UpdateProjectConfigFile()
		{
			LocalConfigFiles.Clear();
			LatestProjectConfigFile = ReadProjectConfigFile(Perforce, BranchClientPath, SelectedClientFileName, LocalConfigFiles, LogWriter);
		}

		public static ConfigFile ReadProjectConfigFile(PerforceConnection Perforce, string BranchClientPath, string SelectedClientFileName, List<KeyValuePair<string, DateTime>> LocalConfigFiles, TextWriter Log)
		{
			List<string> ConfigFilePaths = Utility.GetConfigFileLocations(BranchClientPath, SelectedClientFileName, '/');

			List<PerforceFileRecord> OpenFiles;
			Perforce.GetOpenFiles(String.Format("{0}/....ini", BranchClientPath), out OpenFiles, Log);

			ConfigFile ProjectConfig = new ConfigFile();
			foreach(string ConfigFilePath in ConfigFilePaths)
			{
				List<string> Lines = null;

				// If this file is open for edit, read the local version
				if(OpenFiles != null && OpenFiles.Any(x => x.ClientPath.Equals(ConfigFilePath, StringComparison.InvariantCultureIgnoreCase)))
				{
					try
					{
						string LocalFileName;
						if(Perforce.ConvertToLocalPath(ConfigFilePath, out LocalFileName, Log))
						{
							DateTime LastModifiedTime = File.GetLastWriteTimeUtc(LocalFileName);
							LocalConfigFiles.Add(new KeyValuePair<string, DateTime>(LocalFileName, LastModifiedTime));
							Lines = File.ReadAllLines(LocalFileName).ToList();
						}
					}
					catch(Exception Ex)
					{
						Log.WriteLine("Failed to read local config file for {0}: {1}", ConfigFilePath, Ex.ToString());
					}
				}

				// Otherwise try to get it from perforce
				if(Lines == null)
				{
					Perforce.Print(ConfigFilePath, out Lines, Log);
				}

				// Merge the text with the config file
				if(Lines != null)
				{
					try
					{
						ProjectConfig.Parse(Lines.ToArray());
						Log.WriteLine("Read config file from {0}", ConfigFilePath);
					}
					catch(Exception Ex)
					{
						Log.WriteLine("Failed to read config file from {0}: {1}", ConfigFilePath, Ex.ToString());
					}
				}
			}
			return ProjectConfig;
		}

		public List<PerforceChangeSummary> GetChanges()
		{
			lock(this)
			{
				return new List<PerforceChangeSummary>(Changes);
			}
		}

		public bool TryGetChangeDetails(int Number, out PerforceChangeDetails Details)
		{
			lock(this)
			{
				return ChangeDetails.TryGetValue(Number, out Details);
			}
		}

		public HashSet<int> GetPromotedChangeNumbers()
		{
			lock(this)
			{
				return new HashSet<int>(PromotedChangeNumbers);
			}
		}

		public int LastChangeByCurrentUser
		{
			get;
			private set;
		}

		public int LastCodeChangeByCurrentUser
		{
			get;
			private set;
		}

		public ConfigFile LatestProjectConfigFile
		{
			get;
			private set;
		}

		public string ZippedBinariesStatus
		{
			get;
			private set;
		}

		public bool HasZippedBinaries
		{
			get { return ChangeNumberToZippedBinaries.Count > 0; }
		}

		public bool TryGetArchivePathForChangeNumber(int ChangeNumber, out string ArchivePath)
		{
			return ChangeNumberToZippedBinaries.TryGetValue(ChangeNumber, out ArchivePath);
		}

		public void Refresh()
		{
			RefreshEvent.Set();
		}
	}
}
