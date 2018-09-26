// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Drawing;
using System.Linq;
using System.Text;

namespace Gauntlet
{
	public abstract class UnrealTestNode<TConfigClass> : BaseTest, IDisposable
		where TConfigClass : UnrealTestConfiguration, new()
	{
		/// <summary>
		/// Returns an identifier for this test
		/// </summary>
		public override string Name { get { return this.GetType().FullName; } }

		/// <summary>
		/// How long this test should run for, set during LaunchTest based on results of GetConfiguration
		/// </summary>
		public override float MaxDuration { get; protected set; }

		/// <summary>
		/// Priority of this test
		/// </summary>
		public override TestPriority Priority { get { return GetPriority(); } }

		/// <summary>
		/// Returns true if the test has warnings. At this time we only consider Ensures() to be warnings by default
		/// </summary>
		public override bool HasWarnings { get {
				if (SessionArtifacts == null)
				{
					return false;
				}

				bool HaveEnsures = SessionArtifacts.Any(A => A.LogSummary.Ensures.Count() > 0);
				return HaveEnsures;
			}
		}

		// Begin UnrealTestNode properties and members

		/// <summary>
		/// Our context that holds environment wide info about the required conditions for the test
		/// </summary>
		public UnrealTestContext Context { get; private set; }

		/// <summary>
		/// When the test is running holds all running Unreal processes (clients, servers etc).
		/// </summary>
		public UnrealSessionInstance TestInstance { get; private set; }

		/// <summary>
		/// After the test completes holds artifacts for each process (clients, servers etc).
		/// </summary>
		public IEnumerable<UnrealRoleArtifacts> SessionArtifacts { get; private set; }

		/// <summary>
		/// Helper class that turns our wishes into reallity
		/// </summary>
		protected UnrealSession UnrealApp;

		/// <summary>
		/// Used to track how much of our log has been written out
		/// </summary>
		private int LastLogCount ;

		private int CurrentPass;

		private int NumPasses;

		static private DateTime SessionStartTime = DateTime.MinValue;

		/// <summary>
		/// Our test result. May be set directly, or by overriding GetUnrealTestResult()
		/// </summary>
		private TestResult UnrealTestResult;

		private TConfigClass CachedConfig = null;

		private string CachedArtifactPath = null;

		/// <summary>
		/// If our test should exit suddenly, this is the process that caused it
		/// </summary>
		protected List<IAppInstance> MissingProcesses;

		protected DateTime TimeOfFirstMissingProcess;

		protected int TimeToWaitForProcesses { get; set; }

		// End  UnrealTestNode properties and members 

		// UnrealTestNode member functions
		public UnrealTestNode(UnrealTestContext InContext)
		{
			Context = InContext;

			UnrealTestResult = TestResult.Invalid;
			MissingProcesses = new List<IAppInstance>();
			TimeToWaitForProcesses = 5;
			LastLogCount = 0;
			CurrentPass = 0;
			NumPasses = 0;
		}

		 ~UnrealTestNode()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				CleanupTest();

				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		public override String ToString()
		{
			if (Context == null)
			{
				return Name;
			}

			return string.Format("{0} ({1})", Name, Context);
		}

		/// <summary>
		/// Sets the context that tests run under. Called once during creation
		/// </summary>
		/// <param name="InContext"></param>
		/// <returns></returns>
		public override void SetContext(ITestContext InContext)
		{
			Context = InContext as UnrealTestContext;
		}


		/// <summary>
		/// Returns information about how to configure our Unreal processes. For the most part the majority
		/// of Unreal tests should only need to override this function
		/// </summary>
		/// <returns></returns>
		public virtual TConfigClass GetConfiguration()
		{
			if (CachedConfig == null)
			{
				CachedConfig = new TConfigClass();
				AutoParam.ApplyParamsAndDefaults(CachedConfig, this.Context.TestParams.AllArguments);
			}
			return CachedConfig;
		}

		/// <summary>
		/// Returns a priority value for this test
		/// </summary>
		/// <returns></returns>
		protected TestPriority GetPriority()
		{
			IEnumerable<UnrealTargetPlatform> DesktopPlatforms = UnrealBuildTool.Utils.GetPlatformsInClass(UnrealPlatformClass.Desktop);

			UnrealTestRoleContext ClientContext = Context.GetRoleContext(UnrealTargetRole.Client);

			// because these need deployed we want them in flight asap
			if (ClientContext.Platform == UnrealTargetPlatform.PS4 || ClientContext.Platform == UnrealTargetPlatform.XboxOne)
			{
				return TestPriority.High;
			}

			return TestPriority.Normal;
		}

		protected virtual IEnumerable<UnrealSessionInstance.RoleInstance> FindAnyMissingRoles()
		{
			return TestInstance.RunningRoles.Where(R => R.AppInstance.HasExited);
		}

		/// <summary>
		/// Checks whether the test is still running. The default implementation checks whether all of our processes
		/// are still alive.
		/// </summary>
		/// <returns></returns>
		public virtual bool IsTestRunning()
		{
			var MissingRoles = FindAnyMissingRoles().ToList();

			if (MissingRoles.Count == 0)
			{
				// nothing missing, keep going.
				return true;
			}
			
			// if all roles are gone, we're done
			if (MissingRoles.Count == TestInstance.RunningRoles.Count())
			{
				return false;
			}

			// This test only ends when all roles are gone
			if (GetConfiguration().AllRolesExit)
			{
				return true;
			}

			if (TimeOfFirstMissingProcess == DateTime.MinValue)
			{
				TimeOfFirstMissingProcess = DateTime.Now;
				Log.Verbose("Role {0} exited. Waiting {1} seconds for others to exit", MissingRoles.First().ToString(), TimeToWaitForProcesses);
			}

			if ((DateTime.Now - TimeOfFirstMissingProcess).TotalSeconds < TimeToWaitForProcesses)
			{
				// give other processes time to exit normally
				return true;
			}

			Log.Info("Ending {0} due to exit of Role {1}. {2} processes still running", Name, MissingRoles.First().ToString(), TestInstance.RunningRoles.Count());

			// Done!
			return false;
		}

		protected bool PrepareUnrealApp()
		{
			// Get our configuration
			TConfigClass Config = GetConfiguration();

			if (Config == null)
			{
				throw new AutomationException("Test {0} returned null config!", this);
			}

			if (UnrealApp != null)
			{
				throw new AutomationException("Node already has an UnrealApp, was PrepareUnrealSession called twice?");
			}

			// pass through any arguments such as -TestNameArg or -TestNameArg=Value
			var TestName = this.GetType().Name;
			var ShortName = TestName.Replace("Test", "");

			var PassThroughArgs = Context.TestParams.AllArguments
				.Where(A => A.StartsWith(TestName, System.StringComparison.OrdinalIgnoreCase) || A.StartsWith(ShortName, System.StringComparison.OrdinalIgnoreCase))
				.Select(A =>
				{
					A = "-" + A;

					var EqIndex = A.IndexOf("=");

					// no =? Just a -switch then
					if (EqIndex == -1)
					{
						return A;
					}

					var Cmd = A.Substring(0, EqIndex + 1);
					var Args = A.Substring(EqIndex + 1);

					// if no space in the args, just leave it
					if (Args.IndexOf(" ") == -1)
					{
						return A;
					}

					return string.Format("{0}\"{1}\"", Cmd, Args);
				});

			List<UnrealSessionRole> SessionRoles = new List<UnrealSessionRole>();

			// Go through each type of role that was required and create a session role
			foreach (var TypesToRoles in Config.RequiredRoles)
			{
				// get the actual context of what this role means.
				UnrealTestRoleContext RoleContext = Context.GetRoleContext(TypesToRoles.Key);

				foreach (UnrealTestRole TestRole in TypesToRoles.Value)
				{
					// important, use the type from the ContextRolke because Server may have been mapped to EditorServer etc
					UnrealTargetPlatform SessionPlatform = TestRole.PlatformOverride != UnrealTargetPlatform.Unknown ? TestRole.PlatformOverride : RoleContext.Platform;

					UnrealSessionRole SessionRole = new UnrealSessionRole(RoleContext.Type, SessionPlatform, RoleContext.Configuration, TestRole.CommandLine);

					SessionRole.RoleModifier = TestRole.RoleType;
					SessionRole.Constraint = TestRole.Type == UnrealTargetRole.Client ? Context.Constraint : new UnrealTargetConstraint(SessionPlatform);
					
					Log.Verbose("Created SessionRole {0} from RoleContext {1} (RoleType={2})", SessionRole, RoleContext, TypesToRoles.Key);

					// TODO - this can all / mostly go into UnrealTestConfiguration.ApplyToConfig

					// Deal with command lines
					if (string.IsNullOrEmpty(TestRole.ExplicitClientCommandLine) == false)
					{
						SessionRole.CommandLine = TestRole.ExplicitClientCommandLine;
					}
					else
					{
						// start with anything from our context
						SessionRole.CommandLine = RoleContext.ExtraArgs;

						// did the test ask for anything?
						if (string.IsNullOrEmpty(TestRole.CommandLine) == false)
						{
							SessionRole.CommandLine += " " + TestRole.CommandLine;
						}

						// add controllers
						if (TestRole.Controllers.Count > 0)
						{
							SessionRole.CommandLine += string.Format(" -gauntlet=\"{0}\"", string.Join(",", TestRole.Controllers));
						}

						if (PassThroughArgs.Count() > 0)
						{
							SessionRole.CommandLine += " " + string.Join(" ", PassThroughArgs);
						}

						// add options
						SessionRole.Options = Config;
					}

					if (RoleContext.Skip)
					{
						SessionRole.RoleModifier = ERoleModifier.Null;
					}

                    SessionRole.FilesToCopy = TestRole.FilesToCopy;

					SessionRoles.Add(SessionRole);
				}
			}

			UnrealApp = new UnrealSession(Context.BuildInfo, SessionRoles) { Sandbox = Context.Options.Sandbox };

			return true;
		}

		public override bool IsReadyToStart()
		{
			if (UnrealApp == null)
			{
				PrepareUnrealApp();
			}

			return UnrealApp.TryReserveDevices();
		}

		/// <summary>
		/// Called by the test executor to start our test running. After this
		/// Test.Status should return InProgress or greater
		/// </summary>
		/// <returns></returns>
		public override bool StartTest(int Pass, int InNumPasses)
		{
			if (UnrealApp == null)
			{
				throw new AutomationException("Node already has a null UnrealApp, was PrepareUnrealSession or IsReadyToStart called?");
			}

			TConfigClass Config = GetConfiguration();

			CurrentPass = Pass;
			NumPasses = InNumPasses;			

			// Launch the test
			TestInstance = UnrealApp.LaunchSession();

			// track the overall session time
			if (SessionStartTime == DateTime.MinValue)
			{
				SessionStartTime = DateTime.Now;
			}

			if (TestInstance != null)
			{
				// Update these for the executor
				MaxDuration = Config.MaxDuration;
				UnrealTestResult = TestResult.Invalid;
				MarkTestStarted();
			}
			
			return TestInstance != null;
		}

		/// <summary>
		/// Cleanup all resources
		/// </summary>
		/// <param name="Node"></param>
		/// <returns></returns>
		public override void CleanupTest()
		{
			if (TestInstance != null)
			{
				TestInstance.Dispose();
				TestInstance = null;
			}

			if (UnrealApp != null)
			{
				UnrealApp.Dispose();
				UnrealApp = null;
			}			
		}

		/// <summary>
		/// Restarts the provided test. Only called if one of our derived
		/// classes requests it via the Status result
		/// </summary>
		/// <param name="Node"></param>
		/// <returns></returns>
		public override bool RestartTest()
		{
			TestInstance = UnrealApp.RestartSession();

			return TestInstance != null;
		}

		/// <summary>
		/// Periodically called while the test is running. A chance for tests to examine their
		/// health, log updates etc. Base classes must call this or take all responsibility for
		/// setting Status as necessary
		/// </summary>
		/// <returns></returns>
		public override void TickTest()
		{
			IAppInstance App = null;

			if (TestInstance.ClientApps == null)
			{
				App = TestInstance.ServerApp;
			}
			else
			{
				if (TestInstance.ClientApps.Length > 0)
				{
					App = TestInstance.ClientApps.First();
				}
			}

			if (App != null)
			{
				UnrealLogParser Parser = new UnrealLogParser(App.StdOut);

				// TODO - hardcoded for Orion
				List<string> TestLines = Parser.GetLogChannel("Gauntlet").ToList();

				TestLines.AddRange(Parser.GetLogChannel("OrionTest"));

				for (int i = LastLogCount; i < TestLines.Count; i++)
				{
					Log.Info(TestLines[i]);
				}

				LastLogCount = TestLines.Count;
			}

			
			// Check status and health after updating logs
			if (GetTestStatus() == TestStatus.InProgress && IsTestRunning() == false)
			{
				MarkTestComplete();
			}
		}

		/// <summary>
		/// Called when a test has completed. By default saves artifacts and calles CreateReport
		/// </summary>
		/// <param name="Result"></param>
		/// <returns></returns>
		public override void StopTest(bool WasCancelled)
		{
			// Shutdown the instance so we can access all files, but do not null it or shutdown the UnrealApp because we still need
			// access to these objects and their resources! Final cleanup is done in CleanupTest()
			TestInstance.Shutdown();

			//string TestFolder = string.Format("{0}-{1:yyyy.MM.dd-HH.mm}", Name, SessionStartTime);
			string TestFolder = ToString();
			TestFolder = TestFolder.Replace(" ", "_");
			TestFolder = TestFolder.Replace(",", "");

			string OutputPath = Path.Combine(Context.Options.LogDir, TestFolder);

			// if doing multiple passes, put each in a subdir
			if (NumPasses > 1)
			{
				OutputPath = Path.Combine(OutputPath, string.Format("Pass_{0}_of_{1}", CurrentPass, NumPasses));
			}

			try
			{
                // Basic pre-existing directory check.
                if (!Globals.Params.ParseParam("dev") && Directory.Exists(OutputPath))
                {
                    string NewOutputPath = OutputPath;
                    int i = 0;
                    while (Directory.Exists(NewOutputPath))
                    {
                        i++;
                        NewOutputPath = string.Format("{0}_{1}", OutputPath, i);
                    }
                    Log.Info("Directory already exists at {0}", OutputPath);
                    OutputPath = NewOutputPath;
                }
				Log.Info("Saving artifacts to {0}", OutputPath);
				Directory.CreateDirectory(OutputPath);
				SessionArtifacts = SaveRoleArtifacts(OutputPath);

				// call legacy version
				SaveArtifacts_DEPRECATED(OutputPath);
			}
			catch (Exception Ex)
			{
				Log.Warning("Failed to save artifacts. {0}", Ex);
			}

			try
			{
				CreateReport(GetTestResult(), Context, Context.BuildInfo, SessionArtifacts, OutputPath);
			}
			catch (Exception Ex)
			{
				Log.Warning("Failed to save completion report. {0}", Ex);
			}
		}

		/// <summary>
		/// Optional function that is called on test completion and gives an opportunity to create a report
		/// </summary>
		/// <param name="Result"></param>
		/// <param name="Contex"></param>
		/// <param name="Build"></param>
		public virtual void CreateReport(TestResult Result, UnrealTestContext Contex, UnrealBuildSource Build, IEnumerable<UnrealRoleArtifacts> Artifacts, string ArtifactPath)
		{
		}

		/// <summary>
		/// Called to request that the test save all artifacts from the completed test to the specified 
		/// output path. By default the app will save all logs and crash dumps
		/// </summary>
		/// <param name="Completed"></param>
		/// <param name="Node"></param>
		/// <param name="OutputPath"></param>
		/// <returns></returns>
		public virtual void SaveArtifacts_DEPRECATED(string OutputPath)
		{
			// called for legacy reasons
		}

		/// <summary>
		/// Called to request that the test save all artifacts from the completed test to the specified 
		/// output path. By default the app will save all logs and crash dumps
		/// </summary>
		/// <param name="Completed"></param>
		/// <param name="Node"></param>
		/// <param name="OutputPath"></param>
		/// <returns></returns>
		public IEnumerable<UnrealRoleArtifacts> SaveRoleArtifacts(string OutputPath)
		{
			CachedArtifactPath = OutputPath;
			return UnrealApp.SaveRoleArtifacts(Context, TestInstance, CachedArtifactPath);
		}

		/// <summary>
		/// Parses the provided artifacts to determine the cause of an exit and whether it was abnormal
		/// </summary>
		/// <param name="InArtifacts"></param>
		/// <param name="Reason"></param>
		/// <param name="WasAbnormal"></param>
		/// <returns></returns>
		protected virtual int GetExitCodeAndReason(UnrealRoleArtifacts InArtifacts, out string ExitReason)
		{
			UnrealLogParser.LogSummary LogSummary = InArtifacts.LogSummary;

			// Assume failure!
			int ExitCode = -1;
			ExitReason = "Unknown";

			if (LogSummary.FatalError != null)
			{
				ExitReason = "Process encountered fatal error";
			}
			else if (LogSummary.Ensures.Count() > 0 && CachedConfig.FailOnEnsures)
			{
				ExitReason = string.Format("Process encountered {0} Ensures", LogSummary.Ensures.Count());
			}
			else if (InArtifacts.AppInstance.WasKilled)
			{
				ExitReason = "Process was killed";
			}
			else if (LogSummary.HasTestExitCode)
			{
				if (LogSummary.TestExitCode == 0)
				{
					ExitReason = "Process exited with code 0";
				}
				else
				{
					ExitReason = string.Format("Process exited with error code {0}", LogSummary.TestExitCode);
				}

				ExitCode = LogSummary.TestExitCode;
			}
			else if (LogSummary.RequestedExit)
			{
				ExitReason = string.Format("Process requested exit with no fatal errors");
				ExitCode = 0;
			}
			else
			{
				// ok, process appears to have exited for no good reason so try to divine a result...
				if (LogSummary.HasTestExitCode == false
					&& InArtifacts.SessionRole.CommandLine.ToLower().Contains("-gauntlet"))
				{
					Log.Verbose("Role {0} had 0 exit code but used Gauntlet and no TestExitCode was found. Assuming failure", InArtifacts.SessionRole.RoleType);
					ExitCode = -1;
					ExitReason = "No test result from Gauntlet controller";
				}
			}

			// Normal exits from server are not ok if we had clients running!
			if (ExitCode == 0 && InArtifacts.SessionRole.RoleType.IsServer())
			{
				bool ClientsKilled = SessionArtifacts.Any(A => A.AppInstance.WasKilled && A.SessionRole.RoleType.IsClient());

				if (ClientsKilled)
				{
					ExitCode = -1;
					ExitReason = "Server exited while clients were running";
				}
			}

			if (ExitCode == -1 && string.IsNullOrEmpty(ExitReason))
			{
				ExitReason = "Process exited with no indication of success";
			}

			return ExitCode;
		}

		/// <summary>
		/// Parses the output of an application to try and determine a failure cause (if one exists). Returns
		/// 0 for graceful shutdown
		/// </summary>
		/// <param name="Prefix"></param>
		/// <param name="App"></param>
		/// <returns></returns>
		protected virtual int GetRoleSummary(UnrealRoleArtifacts InArtifacts, out string Summary)
		{

			const int MaxLogLines = 10;
			const int MaxCallstackLines = 20;

			UnrealLogParser.LogSummary LogSummary = InArtifacts.LogSummary;
						
			string ExitReason = "Unknown";
			int ExitCode = GetExitCodeAndReason(InArtifacts, out ExitReason);

			MarkdownBuilder MB = new MarkdownBuilder();

			MB.H3(string.Format("Role: {0} ({1} {2})", InArtifacts.SessionRole.RoleType, InArtifacts.SessionRole.Platform, InArtifacts.SessionRole.Configuration));

			if (ExitCode != 0)
			{
				MB.H4(string.Format("Result: Abnormal Exit: {0}", ExitReason));
			}
			else
			{
				MB.H4(string.Format("Result: {0}", ExitReason));
			}

			int FatalErrors = LogSummary.FatalError != null ? 1 : 0;

			if (LogSummary.FatalError != null)
			{
				MB.H4(string.Format("Fatal Error: {0}", LogSummary.FatalError.Message));
				MB.UnorderedList(InArtifacts.LogSummary.FatalError.Callstack.Take(MaxCallstackLines));

				if (InArtifacts.LogSummary.FatalError.Callstack.Count() > MaxCallstackLines)
				{
					MB.Paragraph("See log for full callstack");
				}
			}

			if (LogSummary.Ensures.Count() > 0)
			{
				foreach (var Ensure in LogSummary.Ensures)
				{
					MB.H4(string.Format("Ensure: {0}", Ensure.Message));
					MB.UnorderedList(Ensure.Callstack.Take(MaxCallstackLines));

					if (Ensure.Callstack.Count() > MaxCallstackLines)
					{
						MB.Paragraph("See log for full callstack");
					}
				}
			}

			MB.Paragraph(string.Format("FatalErrors: {0}, Ensures: {1}, Errors: {2}, Warnings: {3}",
				FatalErrors, LogSummary.Ensures.Count(), LogSummary.Errors.Count(), LogSummary.Warnings.Count()));

			if (GetConfiguration().ShowErrorsInSummary && InArtifacts.LogSummary.Errors.Count() > 0)
			{
				MB.H4("Errors");
				MB.UnorderedList(LogSummary.Errors.Take(MaxLogLines));

				if (LogSummary.Errors.Count() > MaxLogLines)
				{
					MB.Paragraph(string.Format("(First {0} of {1} errors)", MaxLogLines, LogSummary.Errors.Count()));
				}
			}

			if (GetConfiguration().ShowWarningsInSummary && InArtifacts.LogSummary.Warnings.Count() > 0)
			{
				MB.H4("Warnings");
				MB.UnorderedList(LogSummary.Warnings.Take(MaxLogLines));

				if (LogSummary.Warnings.Count() > MaxLogLines)
				{
					MB.Paragraph(string.Format("(First {0} of {1} warnings)", MaxLogLines, LogSummary.Warnings.Count()));
				}
			}

			MB.H4("Artifacts");
			MB.Paragraph(string.Format("Log: {0}", InArtifacts.LogPath));
			MB.Paragraph(string.Format("Commandline: {0}", InArtifacts.AppInstance.CommandLine));
			MB.Paragraph(InArtifacts.ArtifactPath);

			Summary = MB.ToString();
			return ExitCode;
		}

		/// <summary>
		/// Result of the test once completed. Nodes inheriting from us should override
		/// this if custom results are necessary
		/// </summary>
		public sealed override TestResult GetTestResult()
		{
			if (UnrealTestResult == TestResult.Invalid)
			{
				UnrealTestResult = GetUnrealTestResult();
			}

			return UnrealTestResult;
		}

		/// <summary>
		/// Allows tests to set this at anytime. If not called then GetUnrealTestResult() will be called when
		/// the framework first calls GetTestResult()
		/// </summary>
		/// <param name="Result"></param>
		/// <returns></returns>
		protected void SetUnrealTestResult(TestResult Result)
		{
			if (GetTestStatus() != TestStatus.Complete)
			{
				throw new Exception("SetUnrealTestResult() called while test is incomplete!");
			}

			UnrealTestResult = Result;
		}

		/// <summary>
		/// Return all artifacts that are considered to have caused the test to fail
		/// </summary>
		/// <returns></returns>
		protected virtual IEnumerable<UnrealRoleArtifacts> GetArtifactsWithFailures()
		{
			if (SessionArtifacts == null)
			{
				Log.Warning("SessionArtifacts was null, unable to check for failures");
				return new UnrealRoleArtifacts[0] { };
			}

			bool DidKillClients = SessionArtifacts.Any(A => A.SessionRole.RoleType.IsClient() && A.AppInstance.WasKilled);

			Dictionary<UnrealRoleArtifacts, int> ErrorCodes = new Dictionary<UnrealRoleArtifacts, int>();

			var FailureList = SessionArtifacts.Where(A =>
			{
				// ignore anything we killed
				if (A.AppInstance.WasKilled)
				{
					return false;
				}

				string ExitReason;
				int ExitCode = GetExitCodeAndReason(A, out ExitReason);

				ErrorCodes.Add(A, ExitCode);

				return ExitCode != 0;
			});

			return FailureList.OrderByDescending(A =>
			{
				int Score = 0;

				if (A.LogSummary.FatalError != null || (ErrorCodes[A] != 0 && A.AppInstance.WasKilled == false))
				{
					Score += 100000;
				}

				Score += A.LogSummary.Ensures.Count();
				return Score;
			}).ToList();
		}

		/// <summary>
		/// THe base implementation considers  considers Classes can override this to implement more custom detection of success/failure than our
		/// log parsing
		/// </summary>
		/// <returns></returns>in
		protected virtual TestResult GetUnrealTestResult()
		{
			int ExitCode = 0;

			var ProblemArtifact = GetArtifactsWithFailures().FirstOrDefault();

			if (ProblemArtifact != null)
			{
				string ExitReason;

				ExitCode = GetExitCodeAndReason(ProblemArtifact, out ExitReason);
				Log.Info("{0} exited with {1}. ({2})", ProblemArtifact.SessionRole, ExitCode, ExitReason);
			}				

			return ExitCode == 0 ? TestResult.Passed : TestResult.Failed;
		}

		/// <summary>
		/// Returns a summary of this test
		/// </summary>
		/// <returns></returns>
		public override string GetTestSummary()
		{
			
			int AbnormalExits = 0;
			int FatalErrors = 0;
			int Ensures = 0;
			int Errors = 0;
			int Warnings = 0;

			StringBuilder SB = new StringBuilder();
			
			// Sort our artifacts so any missing processes are first
			var ProblemArtifacts = GetArtifactsWithFailures();

			var AllArtifacts = ProblemArtifacts.Union(SessionArtifacts);

			// create a quicck summary of total failures, ensures, errors, etc
			foreach( var Artifact in AllArtifacts)
			{
				string Summary = "NoSummary";
				int ExitCode = GetRoleSummary(Artifact, out Summary);

				if (ExitCode != 0 && Artifact.AppInstance.WasKilled == false)
				{
					AbnormalExits++;
				}

				if (SB.Length > 0)
				{
					SB.AppendLine();
				}
				SB.Append(Summary);

				FatalErrors += Artifact.LogSummary.FatalError != null ? 1 : 0;
				Ensures += Artifact.LogSummary.Ensures.Count();
				Errors += Artifact.LogSummary.Errors.Count();
				Warnings += Artifact.LogSummary.Warnings.Count();
			}

			MarkdownBuilder MB = new MarkdownBuilder();

			// Create a summary
			MB.H2(string.Format("{0} {1}", Name, GetTestResult()));

			if (GetTestResult() != TestResult.Passed)
			{
				if (ProblemArtifacts.Count() > 0)
				{
					foreach (var FailedArtifact in ProblemArtifacts)
					{
						string FirstProcessCause = "";
						int FirstExitCode = GetExitCodeAndReason(FailedArtifact, out FirstProcessCause);
						MB.H3(string.Format("{0}: {1}", FailedArtifact.SessionRole.RoleType, FirstProcessCause));

						if (FailedArtifact.LogSummary.FatalError != null)
						{
							MB.H4(FailedArtifact.LogSummary.FatalError.Message);
						}
					}

					MB.Paragraph("See below for callstack and logs");
				}
			}
			MB.Paragraph(string.Format("Context: {0}", Context.ToString()));
			MB.Paragraph(string.Format("FatalErrors: {0}, Ensures: {1}, Errors: {2}, Warnings: {3}", FatalErrors, Ensures, Errors, Warnings));
			//MB.Paragraph(string.Format("Artifacts: {0}", CachedArtifactPath));
			MB.Append("--------");
			MB.Append(SB.ToString());

			//SB.Clear();
			//SB.AppendLine("begin: stack for UAT");
			//SB.Append(MB.ToString());
			//SB.Append("end: stack for UAT");
			return MB.ToString();
		}
	}
}