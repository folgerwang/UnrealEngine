// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// This executor is similar to LocalExecutor, but uses p/invoke on Windows to ensure that child processes are started at a lower priority and are terminated when the parent process terminates.
	/// </summary>
	class ParallelExecutor : ActionExecutor
	{
		[DebuggerDisplay("{Inner}")]
		class BuildAction
		{
			public int SortIndex;
			public Action Inner;

			public HashSet<BuildAction> Dependencies = new HashSet<BuildAction>();
			public int MissingDependencyCount;

			public HashSet<BuildAction> Dependants = new HashSet<BuildAction>();
			public int TotalDependantCount;

			public List<string> LogLines = new List<string>();
			public int ExitCode = -1;
		}

		/// <summary>
		/// Processor count multiplier for local execution. Can be below 1 to reserve CPU for other tasks.
		/// </summary>
		[XmlConfigFile]
		double ProcessorCountMultiplier = 1.0;

		/// <summary>
		/// Maximum processor count for local execution. 
		/// </summary>
		[XmlConfigFile]
		int MaxProcessorCount = int.MaxValue;

		/// <summary>
		/// When enabled, will stop compiling targets after a compile error occurs.
		/// </summary>
		[XmlConfigFile]
		bool bStopCompilationAfterErrors = false;

		/// <summary>
		/// Constructor
		/// </summary>
		public ParallelExecutor()
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Returns the name of this executor
		/// </summary>
		public override string Name
		{
			get { return "Parallel"; }
		}

		/// <summary>
		/// Checks whether the parallel executor can be used
		/// </summary>
		/// <returns>True if the parallel executor can be used</returns>
		public static bool IsAvailable()
		{
			return Environment.OSVersion.Platform == PlatformID.Win32NT;
		}

		/// <summary>
		/// Executes the specified actions locally.
		/// </summary>
		/// <returns>True if all the tasks successfully executed, or false if any of them failed.</returns>
		public override bool ExecuteActions(List<Action> InputActions, bool bLogDetailedActionStats)
		{
			// Figure out how many processors to use
			int MaxProcesses = Math.Min((int)(Environment.ProcessorCount * ProcessorCountMultiplier), MaxProcessorCount);
			Log.TraceInformation("Building {0} {1} with {2} {3}...", InputActions.Count, (InputActions.Count == 1) ? "action" : "actions", MaxProcesses, (MaxProcesses == 1)? "process" : "processes");

			// Create actions with all our internal metadata
			List<BuildAction> Actions = new List<BuildAction>();
			for(int Idx = 0; Idx < InputActions.Count; Idx++)
			{
				BuildAction Action = new BuildAction();
				Action.SortIndex = Idx;
				Action.Inner = InputActions[Idx];
				Actions.Add(Action);
			}

			// Build a map of items to their producing actions
			Dictionary<FileItem, BuildAction> FileToProducingAction = new Dictionary<FileItem, BuildAction>();
			foreach(BuildAction Action in Actions)
			{
				foreach(FileItem ProducedItem in Action.Inner.ProducedItems)
				{
					FileToProducingAction[ProducedItem] = Action;
				}
			}

			// Update all the actions with all their dependencies
			foreach(BuildAction Action in Actions)
			{
				foreach(FileItem PrerequisiteItem in Action.Inner.PrerequisiteItems)
				{
					BuildAction Dependency;
					if(FileToProducingAction.TryGetValue(PrerequisiteItem, out Dependency))
					{
						Action.Dependencies.Add(Dependency);
						Dependency.Dependants.Add(Action);
					}
				}
			}

			// Figure out the recursive dependency count
			HashSet<BuildAction> VisitedActions = new HashSet<BuildAction>();
			foreach(BuildAction Action in Actions)
			{
				Action.MissingDependencyCount = Action.Dependencies.Count;
				RecursiveIncDependents(Action, VisitedActions);
			}

			// Create the list of things to process
			List<BuildAction> QueuedActions = new List<BuildAction>();
			foreach(BuildAction Action in Actions)
			{
				if(Action.MissingDependencyCount == 0)
				{
					QueuedActions.Add(Action);
				}
			}

			// Execute the actions
			using (LogIndentScope Indent = new LogIndentScope("  "))
			{
				// Create a job object for all the child processes
				bool bResult = true;
				Dictionary<BuildAction, Thread> ExecutingActions = new Dictionary<BuildAction,Thread>();
				List<BuildAction> CompletedActions = new List<BuildAction>();

				using(ManagedProcessGroup ProcessGroup = new ManagedProcessGroup())
				{
					using(AutoResetEvent CompletedEvent = new AutoResetEvent(false))
					{
						int NumCompletedActions = 0;
						using (ProgressWriter ProgressWriter = new ProgressWriter("Compiling C++ source code...", false))
						{
							while(QueuedActions.Count > 0 || ExecutingActions.Count > 0)
							{
								// Sort the actions by the number of things dependent on them
								QueuedActions.Sort((A, B) => (A.TotalDependantCount == B.TotalDependantCount)? (B.SortIndex - A.SortIndex) : (B.TotalDependantCount - A.TotalDependantCount));

								// Create threads up to the maximum number of actions
								while(ExecutingActions.Count < MaxProcesses && QueuedActions.Count > 0)
								{
									BuildAction Action = QueuedActions[QueuedActions.Count - 1];
									QueuedActions.RemoveAt(QueuedActions.Count - 1);

									Thread ExecutingThread = new Thread(() => { ExecuteAction(ProcessGroup, Action, CompletedActions, CompletedEvent); });
									ExecutingThread.Name = String.Format("Build:{0}", Action.Inner.StatusDescription);
									ExecutingThread.Start();

									ExecutingActions.Add(Action, ExecutingThread);
								}

								// Wait for something to finish
								CompletedEvent.WaitOne();

								// Wait for something to finish and flush it to the log
								lock(CompletedActions)
								{
									foreach(BuildAction CompletedAction in CompletedActions)
									{
										// Join the thread
										Thread CompletedThread = ExecutingActions[CompletedAction];
										CompletedThread.Join();
										ExecutingActions.Remove(CompletedAction);

										// Update the progress
										NumCompletedActions++;
										ProgressWriter.Write(NumCompletedActions, InputActions.Count);

										// Write it to the log
										if(CompletedAction.LogLines.Count > 0)
										{
											Log.TraceInformation("[{0}/{1}] {2}", NumCompletedActions, InputActions.Count, CompletedAction.LogLines[0]);
											for(int LineIdx = 1; LineIdx < CompletedAction.LogLines.Count; LineIdx++)
											{
												Log.TraceInformation("{0}", CompletedAction.LogLines[LineIdx]);
											}
										}

										// Check the exit code
										if(CompletedAction.ExitCode == 0)
										{
											// Mark all the dependents as done
											foreach(BuildAction DependantAction in CompletedAction.Dependants)
											{
												if(--DependantAction.MissingDependencyCount == 0)
												{
													QueuedActions.Add(DependantAction);
												}
											}
										}
										else
										{
											// Update the exit code if it's not already set
											if(bResult && CompletedAction.ExitCode != 0)
											{
												bResult = false;
											}
										}
									}
									CompletedActions.Clear();
								}

								// If we've already got a non-zero exit code, clear out the list of queued actions so nothing else will run
								if(!bResult && bStopCompilationAfterErrors)
								{
									QueuedActions.Clear();
								}
							}
						}
					}
				}

				return bResult;
			}
		}

		/// <summary>
		/// Execute an individual action
		/// </summary>
		/// <param name="ProcessGroup">The process group</param>
		/// <param name="Action">The action to execute</param>
		/// <param name="CompletedActions">On completion, the list to add the completed action to</param>
		/// <param name="CompletedEvent">Event to set once an event is complete</param>
		static void ExecuteAction(ManagedProcessGroup ProcessGroup, BuildAction Action, List<BuildAction> CompletedActions, AutoResetEvent CompletedEvent)
		{
			if (Action.Inner.bShouldOutputStatusDescription && !String.IsNullOrEmpty(Action.Inner.StatusDescription))
			{
				Action.LogLines.Add(Action.Inner.StatusDescription);
			}

			using(ManagedProcess Process = new ManagedProcess(ProcessGroup, Action.Inner.CommandPath.FullName, Action.Inner.CommandArguments, Action.Inner.WorkingDirectory.FullName, null, null, ProcessPriorityClass.BelowNormal))
			{
				Action.LogLines.AddRange(Process.ReadAllLines());
				Action.ExitCode = Process.ExitCode;
			}

			lock(CompletedActions)
			{
				CompletedActions.Add(Action);
			}

			CompletedEvent.Set();
		}

		/// <summary>
		/// Increment the number of dependants of an action, recursively
		/// </summary>
		/// <param name="Action">The action to update</param>
		/// <param name="VisitedActions">Set of visited actions</param>
		private static void RecursiveIncDependents(BuildAction Action, HashSet<BuildAction> VisitedActions)
		{
			foreach(BuildAction Dependency in Action.Dependants)
			{
				if(!VisitedActions.Contains(Action))
				{
					VisitedActions.Add(Action);
					Dependency.TotalDependantCount++;
					RecursiveIncDependents(Dependency, VisitedActions);
				}
			}
		}
	}
}
