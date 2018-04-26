// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace UnrealGameSync
{
	enum EventType
	{
		Syncing,

		// Reviews
		Compiles,
		DoesNotCompile,
		Good,
		Bad,
		Unknown,
		
		// Starred builds
		Starred,
		Unstarred,

		// Investigating events
		Investigating,
		Resolved,
	}

	class EventData
	{
		public long Id { get; set; }
		public int Change { get; set; }
		public string UserName { get; set; }
		public EventType Type { get; set; }
		public string Project { get; set; }
	}

	class CommentData
	{
		public long Id { get; set; }
		public int ChangeNumber { get; set; }
		public string UserName { get; set; }
		public string Text { get; set; }
		public string Project { get; set; }
	}

	enum BuildDataResult
	{
		Starting,
		Failure,
		Warning,
		Success,
		Skipped,
	}

	class BuildData
	{
		public long Id { get; set; }
		public int ChangeNumber { get; set; }
		public string BuildType { get; set; }
		public BuildDataResult Result { get; set; }
		public string Url { get; set; }
		public string Project { get; set; }

		public bool IsSuccess
		{
			get { return Result == BuildDataResult.Success || Result == BuildDataResult.Warning; }
		}

		public bool IsFailure
		{
			get { return Result == BuildDataResult.Failure; }
		}
	}

	enum ReviewVerdict
	{
		Unknown,
		Good,
		Bad,
		Mixed,
	}

	class EventSummary
	{
		public int ChangeNumber;
		public ReviewVerdict Verdict;
		public List<EventData> SyncEvents = new List<EventData>();
		public List<EventData> Reviews = new List<EventData>();
		public List<string> CurrentUsers = new List<string>();
		public EventData LastStarReview;
		public List<BuildData> Builds = new List<BuildData>();
		public List<CommentData> Comments = new List<CommentData>();
	}

	class EventMonitor : IDisposable
	{
		string ApiUrl;
		string Project;
		string CurrentUserName;
		Thread WorkerThread;
		AutoResetEvent RefreshEvent = new AutoResetEvent(false);
		ConcurrentQueue<EventData> OutgoingEvents = new ConcurrentQueue<EventData>();
		ConcurrentQueue<EventData> IncomingEvents = new ConcurrentQueue<EventData>();
		ConcurrentQueue<CommentData> OutgoingComments = new ConcurrentQueue<CommentData>();
		ConcurrentQueue<CommentData> IncomingComments = new ConcurrentQueue<CommentData>();
		ConcurrentQueue<BuildData> IncomingBuilds = new ConcurrentQueue<BuildData>();
		Dictionary<int, EventSummary> ChangeNumberToSummary = new Dictionary<int, EventSummary>();
		Dictionary<string, EventData> UserNameToLastSyncEvent = new Dictionary<string, EventData>(StringComparer.InvariantCultureIgnoreCase);
		BoundedLogWriter LogWriter;
		long LastEventId;
		long LastCommentId;
		long LastBuildId;
		bool bDisposing;
		HashSet<int> FilterChangeNumbers = new HashSet<int>();
		List<EventData> InvestigationEvents = new List<EventData>();
		List<EventData> ActiveInvestigations = new List<EventData>();

		public event Action OnUpdatesReady;

		public EventMonitor(string InApiUrl, string InProject, string InCurrentUserName, string InLogFileName)
		{
			ApiUrl = InApiUrl;
			Project = InProject;
			CurrentUserName = InCurrentUserName;

			LogWriter = new BoundedLogWriter(InLogFileName);
			if(ApiUrl == null)
			{
				LastStatusMessage = "Database functionality disabled due to empty ApiUrl.";
			}
			else
			{
				LogWriter.WriteLine("Using connection string: {0}", ApiUrl);
			}
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
					WorkerThread.Join();
				}
				WorkerThread = null;
			}
			if(LogWriter != null)
			{
				LogWriter.Dispose();
				LogWriter = null;
			}
		}

		public void FilterChanges(IEnumerable<int> ChangeNumbers)
		{
			// Build a lookup for all the change numbers
			FilterChangeNumbers = new HashSet<int>(ChangeNumbers);

			// Clear out the list of active users for each review we have
			UserNameToLastSyncEvent.Clear();
			foreach(EventSummary Summary in ChangeNumberToSummary.Values)
			{
				Summary.CurrentUsers.Clear();
			}

			// Add all the user reviews back in again
			foreach(EventSummary Summary in ChangeNumberToSummary.Values)
			{
				foreach(EventData SyncEvent in Summary.SyncEvents)
				{
					ApplyFilteredUpdate(SyncEvent);
				}
			}

			// Clear the list of active investigations, since this depends on the changes we're showing
			ActiveInvestigations = null;
		}

		protected EventSummary FindOrAddSummary(int ChangeNumber)
		{
			EventSummary Summary;
			if(!ChangeNumberToSummary.TryGetValue(ChangeNumber, out Summary))
			{
				Summary = new EventSummary();
				Summary.ChangeNumber = ChangeNumber;
				ChangeNumberToSummary.Add(ChangeNumber, Summary);
			}
			return Summary;
		}

		public string LastStatusMessage
		{
			get;
			private set;
		}

		public void ApplyUpdates()
		{
			EventData Event;
			while(IncomingEvents.TryDequeue(out Event))
			{
				ApplyEventUpdate(Event);
			}

			BuildData Build;
			while(IncomingBuilds.TryDequeue(out Build))
			{
				ApplyBuildUpdate(Build);
			}

			CommentData Comment;
			while(IncomingComments.TryDequeue(out Comment))
			{
				ApplyCommentUpdate(Comment);
			}
		}

		void ApplyEventUpdate(EventData Event)
		{
			EventSummary Summary = FindOrAddSummary(Event.Change);
			if(Event.Type == EventType.Starred || Event.Type == EventType.Unstarred)
			{
				// If it's a star or un-star review, process that separately
				if(Summary.LastStarReview == null || Event.Id > Summary.LastStarReview.Id)
				{
					Summary.LastStarReview = Event;
				}
			}
			else if(Event.Type == EventType.Investigating || Event.Type == EventType.Resolved)
			{
				// Insert it sorted in the investigation list
				int InsertIdx = 0;
				while(InsertIdx < InvestigationEvents.Count && InvestigationEvents[InsertIdx].Id < Event.Id)
				{
					InsertIdx++;
				}
				if(InsertIdx == InvestigationEvents.Count || InvestigationEvents[InsertIdx].Id != Event.Id)
				{
					InvestigationEvents.Insert(InsertIdx, Event);
				}
				ActiveInvestigations = null;
			}
			else if(Event.Type == EventType.Syncing)
			{
				Summary.SyncEvents.RemoveAll(x => String.Compare(x.UserName, Event.UserName, true) == 0);
				Summary.SyncEvents.Add(Event);
				ApplyFilteredUpdate(Event);
			}
			else if(IsReview(Event.Type))
			{
				// Try to find an existing review by this user. If we already have a newer review, ignore this one. Otherwise remove it.
				EventData ExistingReview = Summary.Reviews.Find(x => String.Compare(x.UserName, Event.UserName, true) == 0);
				if(ExistingReview != null)
				{
					if(ExistingReview.Id <= Event.Id)
					{
						Summary.Reviews.Remove(ExistingReview);
					}
					else
					{
						return;
					}
				}

				// Add the new review, and find the new verdict for this change
				Summary.Reviews.Add(Event);
				Summary.Verdict = GetVerdict(Summary.Reviews, Summary.Builds);
			}
			else
			{
				// Unknown type
			}
		}

		void ApplyBuildUpdate(BuildData Build)
		{
			EventSummary Summary = FindOrAddSummary(Build.ChangeNumber);

			BuildData ExistingBuild = Summary.Builds.Find(x => x.ChangeNumber == Build.ChangeNumber && x.BuildType == Build.BuildType);
			if(ExistingBuild != null)
			{
				if(ExistingBuild.Id <= Build.Id)
				{
					Summary.Builds.Remove(ExistingBuild);
				}
				else
				{
					return;
				}
			}

			Summary.Builds.Add(Build);
			Summary.Verdict = GetVerdict(Summary.Reviews, Summary.Builds);
		}

		void ApplyCommentUpdate(CommentData Comment)
		{
			EventSummary Summary = FindOrAddSummary(Comment.ChangeNumber);
			if(String.Compare(Comment.UserName, CurrentUserName, true) == 0 && Summary.Comments.Count > 0 && Summary.Comments.Last().Id == long.MaxValue)
			{
				// This comment was added by PostComment(), to mask the latency of a round trip to the server. Remove it now we have the sorted comment.
				Summary.Comments.RemoveAt(Summary.Comments.Count - 1);
			}
			AddPerUserItem(Summary.Comments, Comment, x => x.Id, x => x.UserName);
		}

		static bool AddPerUserItem<T>(List<T> Items, T NewItem, Func<T, long> IdSelector, Func<T, string> UserSelector)
		{
			int InsertIdx = Items.Count;

			for(; InsertIdx > 0 && IdSelector(Items[InsertIdx - 1]) >= IdSelector(NewItem); InsertIdx--)
			{
				if(String.Compare(UserSelector(Items[InsertIdx - 1]), UserSelector(NewItem), true) == 0)
				{
					return false;
				}
			}

			Items.Insert(InsertIdx, NewItem);

			for(; InsertIdx > 0; InsertIdx--)
			{
				if(String.Compare(UserSelector(Items[InsertIdx - 1]), UserSelector(NewItem), true) == 0)
				{
					Items.RemoveAt(InsertIdx - 1);
				}
			}

			return true;
		}

		static ReviewVerdict GetVerdict(IEnumerable<EventData> Events, IEnumerable<BuildData> Builds)
		{
			int NumPositiveReviews = Events.Count(x => x.Type == EventType.Good);
			int NumNegativeReviews = Events.Count(x => x.Type == EventType.Bad);
			if(NumPositiveReviews > 0 || NumNegativeReviews > 0)
			{
				return GetVerdict(NumPositiveReviews, NumNegativeReviews);
			}

			int NumCompiles = Events.Count(x => x.Type == EventType.Compiles);
			int NumFailedCompiles = Events.Count(x => x.Type == EventType.DoesNotCompile);
			if(NumCompiles > 0 || NumFailedCompiles > 0)
			{
				return GetVerdict(NumCompiles, NumFailedCompiles);
			}

			int NumBuilds = Builds.Count(x => x.BuildType == "Editor" && x.IsSuccess);
			int NumFailedBuilds = Builds.Count(x => x.BuildType == "Editor" && x.IsFailure);
			if(NumBuilds > 0 || NumFailedBuilds > 0)
			{
				return GetVerdict(NumBuilds, NumFailedBuilds);
			}

			return ReviewVerdict.Unknown;
		}

		static ReviewVerdict GetVerdict(int NumPositive, int NumNegative)
		{
			if(NumPositive > (int)(NumNegative * 1.5))
			{
				return ReviewVerdict.Good;
			}
			else if(NumPositive >= NumNegative)
			{
				return ReviewVerdict.Mixed;
			}
			else
			{
				return ReviewVerdict.Bad;
			}
		}

		void ApplyFilteredUpdate(EventData Event)
		{
			if(Event.Type == EventType.Syncing && FilterChangeNumbers.Contains(Event.Change))
			{
				// Update the active users list for this change
				EventData LastSync;
				if(UserNameToLastSyncEvent.TryGetValue(Event.UserName, out LastSync))
				{
					if(Event.Id > LastSync.Id)
					{
						ChangeNumberToSummary[LastSync.Change].CurrentUsers.RemoveAll(x => String.Compare(x, Event.UserName, true) == 0);
						FindOrAddSummary(Event.Change).CurrentUsers.Add(Event.UserName);
						UserNameToLastSyncEvent[Event.UserName] = Event;
					}
				}
				else
				{
					FindOrAddSummary(Event.Change).CurrentUsers.Add(Event.UserName);
					UserNameToLastSyncEvent[Event.UserName] = Event;
				}
			}
		}

		void PollForUpdates()
		{
			EventData Event = null;
			CommentData Comment = null;
			while(!bDisposing)
			{
				// If there's no connection string, just empty out the queue
				if(ApiUrl != null)
				{
					// Post all the reviews to the database. We don't send them out of order, so keep the review outside the queue until the next update if it fails
					while(Event != null || OutgoingEvents.TryDequeue(out Event))
					{
						SendEventToBackend(Event);
						Event = null;
					}

					// Post all the comments to the database.
					while(Comment != null || OutgoingComments.TryDequeue(out Comment))
					{
						SendCommentToBackend(Comment);
						Comment = null;
					}

					// Read all the new reviews
					ReadEventsFromBackend();

					// Send a notification that we're ready to update
					if((IncomingEvents.Count > 0 || IncomingBuilds.Count > 0 || IncomingComments.Count > 0) && OnUpdatesReady != null)
					{
						OnUpdatesReady();
					}
				}

				// Wait for something else to do
				RefreshEvent.WaitOne(30 * 1000);
			}
		}

		bool SendEventToBackend(EventData Event)
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting event... ({0}, {1}, {2})", Event.Change, Event.UserName, Event.Type);
				RESTApi.POST(ApiUrl, "event", new JavaScriptSerializer().Serialize(Event));
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		bool SendCommentToBackend(CommentData Comment)
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting comment... ({0}, {1}, {2}, {3})", Comment.ChangeNumber, Comment.UserName, Comment.Text, Comment.Project);
				RESTApi.POST(ApiUrl, "comment", new JavaScriptSerializer().Serialize(Comment));
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		bool ReadEventsFromBackend()
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine();
				LogWriter.WriteLine("Polling for reviews at {0}...", DateTime.Now.ToString());
				//////////////
				/// LastIds 
				//////////////
				long[] LastIds= RESTApi.GET<long[]>(ApiUrl, "cis", "GET");
				LastEventId = LastIds[0];
				LastCommentId = LastIds[1];
				LastBuildId = LastIds[2];

				//////////////
				/// Reviews 
				//////////////
				List<EventData> Events = RESTApi.GET<List<EventData>>(ApiUrl, "event", string.Format("project={0}", Project), string.Format("lasteventid={0}", LastEventId));
				foreach(EventData Review in Events)
				{
					IncomingEvents.Enqueue(Review);
					LastEventId = Math.Max(LastEventId, Review.Id);
				}

				//////////////
				/// Comments 
				//////////////
				List<CommentData> Comments = RESTApi.GET<List<CommentData>>(ApiUrl, "comment", string.Format("project={0}", Project), string.Format("lastcommentid={0}", LastCommentId));
				foreach (CommentData Comment in Comments)
				{
					IncomingComments.Enqueue(Comment);
					LastCommentId = Math.Max(LastCommentId, Comment.Id);
				}

				//////////////
				/// CIS 
				//////////////
				List<BuildData> Builds = RESTApi.GET<List<BuildData>>(ApiUrl, "cis", string.Format("project={0}", Project), string.Format("lastbuildid={0}", LastBuildId));
				foreach (BuildData Build in Builds)
				{
					IncomingBuilds.Enqueue(Build);
					LastBuildId = Math.Max(LastBuildId, Build.Id);
				}

				LastStatusMessage = String.Format("Last update took {0}ms", Timer.ElapsedMilliseconds);
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				LastStatusMessage = String.Format("Last update failed: ({0})", Ex.ToString());
				return false;
			}
		}

		static bool MatchesWildcard(string Wildcard, string Project)
		{
			return Wildcard.EndsWith("...") && Project.StartsWith(Wildcard.Substring(0, Wildcard.Length - 3), StringComparison.InvariantCultureIgnoreCase);
		}

		public void PostEvent(int ChangeNumber, EventType Type)
		{
			if(ApiUrl != null)
			{
				EventData Event = new EventData();
				Event.Change = ChangeNumber;
				Event.UserName = CurrentUserName;
				Event.Type = Type;
				Event.Project = Project;
				OutgoingEvents.Enqueue(Event);

				ApplyEventUpdate(Event);

				RefreshEvent.Set();
			}
		}

		public void PostComment(int ChangeNumber, string Text)
		{
			if(ApiUrl != null)
			{
				CommentData Comment = new CommentData();
				Comment.Id = long.MaxValue;
				Comment.ChangeNumber = ChangeNumber;
				Comment.UserName = CurrentUserName;
				Comment.Text = Text;
				Comment.Project = Project;
				OutgoingComments.Enqueue(Comment);

				ApplyCommentUpdate(Comment);

				RefreshEvent.Set();
			}
		}

		public bool GetCommentByCurrentUser(int ChangeNumber, out string CommentText)
		{
			EventSummary Summary = GetSummaryForChange(ChangeNumber);
			if(Summary == null)
			{
				CommentText = null;
				return false;
			}

			CommentData Comment = Summary.Comments.Find(x => String.Compare(x.UserName, CurrentUserName, true) == 0);
			if(Comment == null || String.IsNullOrWhiteSpace(Comment.Text))
			{
				CommentText = null;
				return false;
			}

			CommentText = Comment.Text;
			return true;
		}

		public EventData GetReviewByCurrentUser(int ChangeNumber)
		{
			EventSummary Summary = GetSummaryForChange(ChangeNumber);
			if(Summary == null)
			{
				return null;
			}

			EventData Event = Summary.Reviews.FirstOrDefault(x => String.Compare(x.UserName, CurrentUserName, true) == 0);
			if(Event == null || Event.Type == EventType.Unknown)
			{
				return null;
			}

			return Event;
		}

		public EventSummary GetSummaryForChange(int ChangeNumber)
		{
			EventSummary Summary;
			ChangeNumberToSummary.TryGetValue(ChangeNumber, out Summary);
			return Summary;
		}

		public static bool IsReview(EventType Type)
		{
			return IsPositiveReview(Type) || IsNegativeReview(Type) || Type == EventType.Unknown;
		}

		public static bool IsPositiveReview(EventType Type)
		{
			return Type == EventType.Good || Type == EventType.Compiles;
		}

		public static bool IsNegativeReview(EventType Type)
		{
			return Type == EventType.DoesNotCompile || Type == EventType.Bad;
		}

		public bool WasSyncedByCurrentUser(int ChangeNumber)
		{
			EventSummary Summary = GetSummaryForChange(ChangeNumber);
			return (Summary != null && Summary.SyncEvents.Any(x => x.Type == EventType.Syncing && String.Compare(x.UserName, CurrentUserName, true) == 0));
		}

		public void StartInvestigating(int ChangeNumber)
		{
			PostEvent(ChangeNumber, EventType.Investigating);
		}

		public void FinishInvestigating(int ChangeNumber)
		{
			PostEvent(ChangeNumber, EventType.Resolved);
		}

		protected void UpdateActiveInvestigations()
		{
			if(ActiveInvestigations == null)
			{
				// Insert investigation events into the active list, sorted by change number. Remove 
				ActiveInvestigations = new List<EventData>();
				foreach(EventData InvestigationEvent in InvestigationEvents)
				{
					if(FilterChangeNumbers.Contains(InvestigationEvent.Change))
					{
						if(InvestigationEvent.Type == EventType.Investigating)
						{
							int InsertIdx = 0;
							while(InsertIdx < ActiveInvestigations.Count && ActiveInvestigations[InsertIdx].Change > InvestigationEvent.Change)
							{
								InsertIdx++;
							}
							ActiveInvestigations.Insert(InsertIdx, InvestigationEvent);
						}
						else
						{
							ActiveInvestigations.RemoveAll(x => String.Compare(x.UserName, InvestigationEvent.UserName, true) == 0 && x.Change <= InvestigationEvent.Change);
						}
					}
				}

				// Remove any duplicate users
				for(int Idx = 0; Idx < ActiveInvestigations.Count; Idx++)
				{
					for(int OtherIdx = 0; OtherIdx < Idx; OtherIdx++)
					{
						if(String.Compare(ActiveInvestigations[Idx].UserName, ActiveInvestigations[OtherIdx].UserName, true) == 0)
						{
							ActiveInvestigations.RemoveAt(Idx--);
							break;
						}
					}
				}
			}
		}

		public bool IsUnderInvestigation(int ChangeNumber)
		{
			UpdateActiveInvestigations();
			return ActiveInvestigations.Any(x => x.Change <= ChangeNumber);
		}

		public bool IsUnderInvestigationByCurrentUser(int ChangeNumber)
		{
			UpdateActiveInvestigations();
			return ActiveInvestigations.Any(x => x.Change <= ChangeNumber && String.Compare(x.UserName, CurrentUserName, true) == 0);
		}

		public IEnumerable<string> GetInvestigatingUsers(int ChangeNumber)
		{
			UpdateActiveInvestigations();
			return ActiveInvestigations.Where(x => ChangeNumber >= x.Change).Select(x => x.UserName);
		}

		public int GetInvestigationStartChangeNumber(int LastChangeNumber)
		{
			UpdateActiveInvestigations();

			int StartChangeNumber = -1;
			foreach(EventData ActiveInvestigation in ActiveInvestigations)
			{
				if(String.Compare(ActiveInvestigation.UserName, CurrentUserName, true) == 0)
				{
					if(ActiveInvestigation.Change <= LastChangeNumber && (StartChangeNumber == -1 || ActiveInvestigation.Change < StartChangeNumber))
					{
						StartChangeNumber = ActiveInvestigation.Change;
					}
				}
			}
			return StartChangeNumber;
		}
	}
}
