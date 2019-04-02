// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace UnrealGameSync
{
	class TelemetryTimingData
	{
		public string Action { get; set; }
		public string Result { get; set; }
		public string UserName { get; set; }
		public string Project { get; set; }
		public DateTime Timestamp { get; set; }
		public float Duration { get; set; }
	}

	enum TelemetryErrorType
	{
		Crash,
	}

	class TelemetryErrorData
	{
		public TelemetryErrorType Type { get; set; }
		public string Text { get; set; }
		public string UserName { get; set; }
		public string Project { get; set; }
		public DateTime Timestamp { get; set; }
	}

	class TelemetryStopwatch : IDisposable
	{
		readonly string Action;
		readonly string Project;
		readonly DateTime StartTime;
		string Result;
		DateTime EndTime;

		public TelemetryStopwatch(string InAction, string InProject)
		{
			Action = InAction;
			Project = InProject;
			StartTime = DateTime.UtcNow;
		}

		public TimeSpan Stop(string InResult)
		{
			EndTime = DateTime.UtcNow;
			Result = InResult;
			return Elapsed;
		}

		public void Dispose()
		{
			if(Result == null)
			{
				Stop("Aborted");
			}
			TelemetryWriter.Enqueue(Action, Result, Project, StartTime, (float)Elapsed.TotalSeconds);
		}

		public TimeSpan Elapsed
		{
			get { return ((Result == null)? DateTime.UtcNow : EndTime) - StartTime; }
		}
	}

	class TelemetryWriter : IDisposable
	{
		static TelemetryWriter Instance;

		string ApiUrl;
		Thread WorkerThread;
		BoundedLogWriter LogWriter;
		bool bDisposing;
		ConcurrentQueue<TelemetryTimingData> QueuedTimingData = new ConcurrentQueue<TelemetryTimingData>(); 
		ConcurrentQueue<TelemetryErrorData> QueuedErrorData = new ConcurrentQueue<TelemetryErrorData>(); 
		AutoResetEvent RefreshEvent = new AutoResetEvent(false);

		public TelemetryWriter(string InApiUrl, string InLogFileName)
		{
			Instance = this;

			ApiUrl = InApiUrl;

			LogWriter = new BoundedLogWriter(InLogFileName);
			LogWriter.WriteLine("Using connection string: {0}", ApiUrl);

			WorkerThread = new Thread(() => WorkerThreadCallback());
			WorkerThread.Start();
		}

		public void Dispose()
		{
			bDisposing = true;

			if(WorkerThread != null)
			{
				RefreshEvent.Set();
				if(!WorkerThread.Join(10 * 1000))
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

			Instance = null;
		}

		void WorkerThreadCallback()
		{
			string Version = Assembly.GetExecutingAssembly().GetName().Version.ToString();

			string IpAddress = "Unknown";
			try
			{
				IPHostEntry HostEntry = Dns.GetHostEntry(Dns.GetHostName());
				foreach (IPAddress Address in HostEntry.AddressList)
				{
					if (Address.AddressFamily == AddressFamily.InterNetwork)
					{
						IpAddress = Address.ToString();
						break;
					}
				}
			}
			catch
			{
			}

			while(!bDisposing)
			{
				// Wait for an update
				RefreshEvent.WaitOne();

				// Send all the timing data
				TelemetryTimingData TimingData;
				while(QueuedTimingData.TryDequeue(out TimingData))
				{
					while(!bDisposing && !SendTimingData(TimingData, Version, IpAddress) && ApiUrl != null)
					{
						RefreshEvent.WaitOne(10 * 1000);
					}
				}

				// Send all the error data
				TelemetryErrorData ErrorData;
				while(QueuedErrorData.TryDequeue(out ErrorData))
				{
					while(!SendErrorData(ErrorData, Version, IpAddress) && ApiUrl != null)
					{
						if(bDisposing) break;
						RefreshEvent.WaitOne(10 * 1000);
					}
				}
			}
		}

		bool SendTimingData(TelemetryTimingData Data, string Version, string IpAddress)
		{
			if(!DeploymentSettings.bSendTelemetry)
			{
				return true;
			}

			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting timing data... ({0}, {1}, {2}, {3}, {4}, {5})", Data.Action, Data.Result, Data.UserName, Data.Project, Data.Timestamp, Data.Duration);
				RESTApi.POST(ApiUrl, "telemetry", new JavaScriptSerializer().Serialize(Data), string.Format("Version={0}", Version), string.Format("IpAddress={0}", IpAddress));
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		bool SendErrorData(TelemetryErrorData Data, string Version, string IpAddress)
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting error data... ({0}, {1})", Data.Type, Data.Timestamp);
				RESTApi.POST(ApiUrl, "error", new JavaScriptSerializer().Serialize(Data), string.Format("Version={0}", Version), string.Format("IpAddress={0}", IpAddress));
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		public static void Enqueue(string Action, string Result, string Project, DateTime Timestamp, float Duration)
		{
			TelemetryWriter Writer = Instance;
			if(Writer != null)
			{
				TelemetryTimingData Telemetry = new TelemetryTimingData();
				Telemetry.Action = Action;
				Telemetry.Result = Result;
				Telemetry.UserName = Environment.UserName;
				Telemetry.Project = Project;
				Telemetry.Timestamp = Timestamp;
				Telemetry.Duration = Duration;

				Writer.QueuedTimingData.Enqueue(Telemetry);
				Writer.RefreshEvent.Set();
			}
		}

		public static void Enqueue(TelemetryErrorType Type, string Text, string Project, DateTime Timestamp)
		{
			TelemetryWriter Writer = Instance;
			if(Writer != null)
			{
				TelemetryErrorData Error = new TelemetryErrorData();
				Error.Type = Type;
				Error.Text = Text;
				Error.UserName = Environment.UserName;
				Error.Project = Project;
				Error.Timestamp = Timestamp;

				Writer.QueuedErrorData.Enqueue(Error);
				Writer.RefreshEvent.Set();
			}
		}
	}
}
