// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Builds a target
	/// </summary>
	[ToolMode("Build", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance)]
	class BuildMode : ToolMode
	{
		/// <summary>
		/// Specifies the file to use for logging
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string BaseLogFileName = "../Programs/UnrealBuildTool/Log.txt";

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>One of the values of ECompilationResult</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);

			// Initialize the log system, buffering the output until we can create the log file
			StartupTraceListener StartupListener = new StartupTraceListener();
			Trace.Listeners.Add(StartupListener);

			// Write the command line
			Log.TraceLog("Command line: {0}", Environment.CommandLine);

			// Grab the environment.
			UnrealBuildTool.InitialEnvironment = Environment.GetEnvironmentVariables();
			if (UnrealBuildTool.InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// Read the XML configuration files
			XmlConfig.ApplyTo(this);

			// Create the log file, and flush the startup listener to it
			FileReference LogFile = null;
			if(!Arguments.HasOption("-NoLog") && !Log.HasFileWriter())
			{
				LogFile = new FileReference(BaseLogFileName);
				foreach(string LogSuffix in Arguments.GetValues("-LogSuffix="))
				{
					LogFile = LogFile.ChangeExtension(null) + "_" + LogSuffix + LogFile.GetExtension();
				}

				TextWriterTraceListener LogTraceListener = Log.AddFileWriter("DefaultLogTraceListener", LogFile);
				StartupListener.CopyTo(LogTraceListener);
			}
			Trace.Listeners.Remove(StartupListener);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Then let the command lines override any configs necessary.
			if(BuildConfiguration.bXGEExport)
			{
				BuildConfiguration.bAllowXGE = true;
			}
			if(BuildConfiguration.SingleFileToCompile != null)
			{
				BuildConfiguration.bUseUBTMakefiles = false;
			}

			// Parse the remote INI setting
			string RemoteIniPath;
			Arguments.TryGetValue("-RemoteIni=", out RemoteIniPath);
			UnrealBuildTool.SetRemoteIniPath(RemoteIniPath);

			Timeline.AddEvent("Basic UBT initialization");

			// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
			if (!BuildConfiguration.bIgnoreJunk)
			{
				JunkDeleter.DeleteJunk();
			}

			Stopwatch BuildTimer = Stopwatch.StartNew();

			// Reset global configurations
			string ExecutorName = "Unknown";

			List<CPPHeaders> HeadersList = new List<CPPHeaders>();

			Stopwatch ExecutorTimer = new Stopwatch();

			try
			{
				// action graph implies using the dependency resolve cache
				bool GeneratingActionGraph = Arguments.HasOption("-Graph");

				Timeline.AddEvent("RunUBT() initialization complete");

				bool bSkipRulesCompile = Arguments.Any(x => x.Equals("-skiprulescompile", StringComparison.InvariantCultureIgnoreCase));

				List<TargetDescriptor> TargetDescriptors = new List<TargetDescriptor>();
				using(Timeline.ScopeEvent("TargetDescriptor.ParseCommandLine()"))
				{
					TargetDescriptors.AddRange(TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration.bUsePrecompiled, bSkipRulesCompile));
				}

				// Handle remote builds
				for(int Idx = 0; Idx < TargetDescriptors.Count; Idx++)
				{
					TargetDescriptor TargetDesc = TargetDescriptors[Idx];
					if(RemoteMac.HandlesTargetPlatform(TargetDesc.Platform))
					{
						FileReference RemoteLogFile = null;
						if(LogFile != null)
						{
							RemoteLogFile = FileReference.Combine(LogFile.Directory, LogFile.GetFileNameWithoutExtension() + "_Remote.txt");
						}

						RemoteMac RemoteMac = new RemoteMac(TargetDesc.ProjectFile);
						if(!RemoteMac.Build(TargetDesc, LogFile))
						{
							return (int)ECompilationResult.Unknown;
						}

						TargetDescriptors.RemoveAt(Idx--);
						if(TargetDescriptors.Count == 0)
						{
							return (int)ECompilationResult.Succeeded;
						}
					}
				}

				if (Arguments.Any(x => x.Equals("-InvalidateMakefilesOnly", StringComparison.InvariantCultureIgnoreCase)))
				{
					Log.TraceInformation("Invalidating makefiles only in this run.");
					foreach(TargetDescriptor TargetDesc in TargetDescriptors)
					{
						FileReference MakefileLocation = TargetMakefile.GetLocation(TargetDesc.ProjectFile, TargetDesc.Name, TargetDesc.Platform, TargetDesc.Configuration);
						if(FileReference.Exists(MakefileLocation))
						{ 
							FileReference.Delete(MakefileLocation);
						}
					}
					return (int)ECompilationResult.Succeeded;
				}

				// Get a set of all the project directories.
				HashSet<DirectoryReference> ProjectDirs = new HashSet<DirectoryReference>();
				foreach(TargetDescriptor TargetDesc in TargetDescriptors)
				{
					if(TargetDesc.ProjectFile != null)
					{
						ProjectDirs.Add(TargetDesc.ProjectFile.Directory);
					}
				}

				// Create the working set provider
				using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(UnrealBuildTool.RootDirectory, ProjectDirs))
				{
					// Create a makefile for each target
					TargetMakefile[] Makefiles = new TargetMakefile[TargetDescriptors.Count];
					for(int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
					{
						TargetDescriptor TargetDesc = TargetDescriptors[TargetIdx];

						// Get the path to the makefile for this target
						FileReference MakefileLocation = TargetMakefile.GetLocation(TargetDesc.ProjectFile, TargetDesc.Name, TargetDesc.Platform, TargetDesc.Configuration);

						// Try to load an existing makefile
						TargetMakefile Makefile = null;
						if(BuildConfiguration.bUseUBTMakefiles)
						{
							using(Timeline.ScopeEvent("UBTMakefile.LoadUBTMakefile"))
							{
								string ReasonNotLoaded;
								Makefile = TargetMakefile.Load(MakefileLocation, TargetDesc.ProjectFile, TargetDesc.Platform, WorkingSet, out ReasonNotLoaded);
								if (Makefile == null)
								{
									Log.TraceInformation("Creating makefile for {0} ({1})", TargetDesc.Name, ReasonNotLoaded);
								}
							}
						}

						// If we couldn't load a makefile, create a new one
						if(Makefile == null)
						{
							// Create the target
							UEBuildTarget Target = UEBuildTarget.CreateTarget(TargetDesc, bSkipRulesCompile, BuildConfiguration.SingleFileToCompile != null, BuildConfiguration.bUsePrecompiled);

							// Create the header cache for this target
							FileReference DependencyCacheFile = DependencyCache.GetDependencyCachePathForTarget(Target.ProjectFile, Target.Platform, Target.TargetName);
							CPPHeaders Headers = new CPPHeaders(Target.ProjectFile, DependencyCacheFile);
							HeadersList.Add(Headers);

							// Read the dependency cache
							using(Timeline.ScopeEvent("DependencyCache.Create()"))
							{
								Headers.IncludeDependencyCache = DependencyCache.Create(DependencyCache.GetDependencyCachePathForTarget(Target.ProjectFile, Target.Platform, Target.TargetName));
							}

							// Build the target
							const bool bIsAssemblingBuild = true;

							List<FileItem> OutputItems = new List<FileItem>();
							Dictionary<string, FileItem[]> ModuleNameToOutputItems = new Dictionary<string, FileItem[]>();
							List<UHTModuleInfo> UObjectModules = new List<UHTModuleInfo>();
							ActionGraph ActionGraph = new ActionGraph();
							BuildPrerequisites Prerequisites = new BuildPrerequisites();

							ECompilationResult BuildResult = Target.Build(BuildConfiguration, Headers, OutputItems, ModuleNameToOutputItems, UObjectModules, WorkingSet, ActionGraph, Prerequisites, bIsAssemblingBuild);
							if (BuildResult != ECompilationResult.Succeeded)
							{
								return (int)BuildResult;
							}

							// Create the makefile
							Makefile = new TargetMakefile(Target.TargetType);
							Makefile.ReceiptFile = Target.ReceiptFileName;
							Makefile.Actions = ActionGraph.Actions;
							Makefile.OutputItems = OutputItems;
							Makefile.ModuleNameToOutputItems = ModuleNameToOutputItems;
							Makefile.UObjectModules = UObjectModules;
							Makefile.Prerequisites = Prerequisites;
							Makefile.HotReloadModuleNames = Target.GetHotReloadModuleNames();
							Makefile.Prerequisites = Prerequisites;
							Makefile.ProjectIntermediateDirectory = Target.ProjectIntermediateDirectory;
							Makefile.bDeployAfterCompile = Target.bDeployAfterCompile;
							Makefile.bHasProjectScriptPlugin = Target.bHasProjectScriptPlugin;
							Makefile.PreBuildScripts = Target.PreBuildStepScripts;

							// Save the environment variables
							foreach (System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables())
							{
								Makefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value));
							}

							// Save the makefile for next time
							using(Timeline.ScopeEvent("UBTMakefile.SaveUBTMakefile()"))
							{
								TargetMakefile.SaveUBTMakefile(TargetDesc, Makefile);
							}
						}
						else
						{
							// Restore the environment variables
							foreach (Tuple<string, string> EnvironmentVariable in Makefile.EnvironmentVariables)
							{
								Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
							}

							// Execute all the pre-build steps
							if (!BuildConfiguration.bXGEExport)
							{
								if (!Utils.ExecuteCustomBuildSteps(Makefile.PreBuildScripts))
								{
									return (int)ECompilationResult.OtherCompilationError;
								}
							}

							// If the target needs UHT to be run, we'll go ahead and do that now
							if (Makefile.UObjectModules.Count > 0)
							{
								const bool bIsGatheringBuild = false;
								const bool bIsAssemblingBuild = true;

								FileReference ModuleInfoFileName = FileReference.Combine(Makefile.ProjectIntermediateDirectory, TargetDesc.Name + ".uhtmanifest");
								ECompilationResult UHTResult = ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, TargetDesc.ProjectFile, TargetDesc.Name, Makefile.TargetType, Makefile.bHasProjectScriptPlugin, UObjectModules: Makefile.UObjectModules, ModuleInfoFileName: ModuleInfoFileName, bIsGatheringBuild: bIsGatheringBuild, bIsAssemblingBuild: bIsAssemblingBuild);
								if(!UHTResult.Succeeded())
								{
									Log.TraceInformation("UnrealHeaderTool failed for target '" + TargetDesc.Name + "' (platform: " + TargetDesc.Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
									return (int)UHTResult;
								}
							}
						}

						// Add the makefile to the list to build
						Makefiles[TargetIdx] = Makefile;
					}

					// Execute the build
					if(!BuildConfiguration.bSkipBuild)
					{
						// Make sure that none of the actions conflict with any other (producing output files differently, etc...)
						if(!ActionGraph.CheckForConflicts(Makefiles.SelectMany(x => x.Actions)))
						{
							Log.TraceInformation("Check log for additional details.");
							return (int)ECompilationResult.OtherCompilationError;
						}

						// Find all the actions to be executed
						HashSet<Action>[] ActionsToExecute = new HashSet<Action>[TargetDescriptors.Count];
						for(int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
						{
							ActionsToExecute[TargetIdx] = GetActionsForTarget(BuildConfiguration, TargetDescriptors[TargetIdx], Makefiles[TargetIdx]);
						}

						// If there are multiple targets being built, make sure the actions for each one 
						List<Action> MergedActionsToExecute = new List<Action>();
						if(TargetDescriptors.Count == 1)
						{
							// Just take the actions from the first target
							MergedActionsToExecute.AddRange(ActionsToExecute[0]);
						}
						else
						{
							// Set of all output items. Knowing that there are no conflicts in produced items, we use this to eliminate duplicate actions.
							Dictionary<FileItem, Action> OutputItemToProducingAction = new Dictionary<FileItem, Action>();
							for(int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
							{
								string GroupPrefix = String.Format("{0}-{1}-{2}", TargetDescriptors[TargetIdx].Name, TargetDescriptors[TargetIdx].Platform, TargetDescriptors[TargetIdx].Configuration);
								foreach(Action Action in ActionsToExecute[TargetIdx])
								{
									Action ExistingAction;
									if(!OutputItemToProducingAction.TryGetValue(Action.ProducedItems[0], out ExistingAction))
									{
										OutputItemToProducingAction[Action.ProducedItems[0]] = Action;
										ExistingAction = Action;
									}
									ExistingAction.GroupNames.Add(GroupPrefix);
								}
							}
							MergedActionsToExecute.AddRange(OutputItemToProducingAction.Values);
						}

						// Link all the actions together
						ActionGraph.Link(MergedActionsToExecute);

						// Make sure the appropriate executor is selected
						foreach(TargetDescriptor TargetDescriptor in TargetDescriptors)
						{
							UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(TargetDescriptor.Platform);
							BuildConfiguration.bAllowXGE &= BuildPlatform.CanUseXGE();
							BuildConfiguration.bAllowDistcc &= BuildPlatform.CanUseDistcc();
							BuildConfiguration.bAllowSNDBS &= BuildPlatform.CanUseSNDBS();
						}

						// Delete produced items that are outdated.
						ActionGraph.DeleteOutdatedProducedItems(MergedActionsToExecute);

						// Save all the action histories now that files have been removed. We have to do this after deleting produced items to ensure that any
						// items created during the build don't have the wrong command line.
						ActionHistory.SaveAll();

						// Create directories for the outdated produced items.
						ActionGraph.CreateDirectoriesForProducedItems(MergedActionsToExecute);

						// Execute the actions.
						ExecutorTimer.Start();
						if (BuildConfiguration.bXGEExport)
						{
							using(Timeline.ScopeEvent("XGE.ExportActions()"))
							{
								XGE.ExportActions(MergedActionsToExecute);
							}
						}
						else
						{
							using(Timeline.ScopeEvent("ActionGraph.ExecuteActions()"))
							{
								if(!ActionGraph.ExecuteActions(BuildConfiguration, MergedActionsToExecute, out ExecutorName))
								{
									return (int)ECompilationResult.OtherCompilationError;
								}
							}
						}
						ExecutorTimer.Stop();

						// Run the deployment steps
						if (BuildConfiguration.SingleFileToCompile == null
							&& !BuildConfiguration.bGenerateManifest
							&& !BuildConfiguration.bXGEExport)
						{
							foreach(TargetMakefile Makefile in Makefiles)
							{
								if (Makefile.bDeployAfterCompile)
								{
									TargetReceipt Receipt = TargetReceipt.Read(Makefile.ReceiptFile);
									Log.TraceInformation("Deploying {0} {1} {2}...", Receipt.TargetName, Receipt.Platform, Receipt.Configuration);
									UEBuildPlatform.GetBuildPlatform(Receipt.Platform).Deploy(Receipt);
								}
							}
						}
					}
				}
				return (int)ECompilationResult.Succeeded;
			}
			catch (Exception Ex)
			{
				Log.WriteException(Ex, (LogFile == null) ? null : LogFile.FullName);
				return (int)ECompilationResult.OtherCompilationError;
			}
			finally
			{
				// Save the dependency caches
				CppDependencyCache.SaveAll();

				// Save the include dependency cache.
				foreach (CPPHeaders Headers in HeadersList)
				{
					// NOTE: It's very important that we save the include cache, even if a build exception was thrown (compile error, etc), because we need to make sure that
					//    any C++ include dependencies that we computed for out of date source files are saved.  Remember, the build may fail *after* some build products
					//    are successfully built.  If we didn't save our dependency cache after build failures, source files for those build products that were successfully
					//    built before the failure would not be considered out of date on the next run, so this is our only chance to cache C++ includes for those files!

					if (Headers.IncludeDependencyCache != null)
					{
						using(Timeline.ScopeEvent("DependencyCache.Save()"))
						{
							Headers.IncludeDependencyCache.Save();
						}
					}
				}

				// Figure out how long we took to execute.
				if (ExecutorName != "Unknown")
				{
					Log.TraceInformation("Total build time: {0:0.00} seconds ({1} executor: {2:0.00} seconds)", BuildTimer.Elapsed.TotalSeconds, ExecutorName, ExecutorTimer.Elapsed.TotalSeconds);
				}

				// Print some performance info
				Log.TraceLog("DirectIncludes cache miss time: {0}s ({1} misses)", CPPHeaders.DirectIncludeCacheMissesTotalTime, CPPHeaders.TotalDirectIncludeCacheMisses);
			}
		}

		/// <summary>
		/// Determine what needs to be built for a target
		/// </summary>
		/// <param name="BuildConfiguration">The build configuration</param>
		/// <param name="TargetDescriptor">Target being build</param>
		/// <param name="Makefile">Makefile generated for this target</param>
		/// <returns>Set of actions to execute</returns>
		static HashSet<Action> GetActionsForTarget(BuildConfiguration BuildConfiguration, TargetDescriptor TargetDescriptor, TargetMakefile Makefile)
		{
			// Create the action graph
			ActionGraph.Link(Makefile.Actions);

			// Parse the list of hot-reload module names
			Dictionary<string, int> HotReloadModuleNameToSuffix = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
			foreach(string ModuleWithSuffix in TargetDescriptor.AdditionalArguments.GetValues("-ModuleWithSuffix="))
			{
				int SuffixIdx = ModuleWithSuffix.LastIndexOf(',');
				if(SuffixIdx == -1)
				{
					throw new BuildException("Missing suffix argument from -ModuleWithSuffix=Name,Suffix");
				}

				string ModuleName = ModuleWithSuffix.Substring(0, SuffixIdx);

				int Suffix;
				if(!Int32.TryParse(ModuleWithSuffix.Substring(SuffixIdx + 1), out Suffix))
				{
					throw new BuildException("Suffix for modules must be an integer");
				}

				HotReloadModuleNameToSuffix[ModuleName] = Suffix;
			}

			// Get the hot-reload mode
			HotReloadMode HotReloadMode = HotReloadMode.Disabled;
			if(TargetDescriptor.AdditionalArguments.Any(x => x.Equals("-ForceHotReload", StringComparison.InvariantCultureIgnoreCase)))
			{
				HotReloadMode = HotReloadMode.FromIDE;
			}
			else if (!TargetDescriptor.AdditionalArguments.Any(x => x.Equals("-NoHotReload", StringComparison.InvariantCultureIgnoreCase)))
			{
				if (BuildConfiguration.bAllowHotReloadFromIDE && HotReload.ShouldDoHotReloadFromIDE(BuildConfiguration, TargetDescriptor))
				{
					HotReloadMode = HotReloadMode.FromIDE;
				}
				else if (HotReloadModuleNameToSuffix.Count > 0 && TargetDescriptor.ForeignPlugin == null)
				{
					HotReloadMode = HotReloadMode.FromEditor;
				}
			}

			// Get the output items we're trying to build
			List<FileItem> TargetOutputItems = Makefile.OutputItems;

			// Parse the list of modules to build
			HashSet<string> OnlyModuleNames = new HashSet<string>(TargetDescriptor.AdditionalArguments.GetValues("-Module="));
			if (OnlyModuleNames.Count > 0)
			{
				TargetOutputItems = new List<FileItem>();
				foreach(string OnlyModuleName in OnlyModuleNames)
				{
					FileItem[] OutputItemsForModule;
					if(!Makefile.ModuleNameToOutputItems.TryGetValue(OnlyModuleName, out OutputItemsForModule))
					{
						throw new BuildException("Unable to find output items for module '{0}'", OnlyModuleName);
					}
					TargetOutputItems.AddRange(OutputItemsForModule);
				}
			}

			// Get the root prerequisite actions
			List<Action> PrerequisiteActions = ActionGraph.GatherPrerequisiteActions(Makefile.Actions, new HashSet<FileItem>(TargetOutputItems));

			// Get the path to the hot reload state file for this target
			FileReference HotReloadStateFile = global::UnrealBuildTool.HotReloadState.GetLocation(TargetDescriptor.ProjectFile, TargetDescriptor.Name, TargetDescriptor.Platform, TargetDescriptor.Configuration, TargetDescriptor.Architecture);

			// Apply the previous hot reload state
			HotReloadState HotReloadState = null;
			if(HotReloadMode == HotReloadMode.Disabled)
			{
				// Delete the previous state file
				HotReload.DeleteTemporaryFiles(HotReloadStateFile);
			}
			else
			{
				// Read the previous state file and apply it to the action graph
				if(FileReference.Exists(HotReloadStateFile))
				{
					HotReloadState = HotReloadState.Load(HotReloadStateFile);
				}
				else
				{
					HotReloadState = new HotReloadState();
				}

				// Update the action graph to produce these new files
				HotReload.PatchActionGraph(PrerequisiteActions, HotReloadState.OriginalFileToHotReloadFile);

				// Update the module to output file mapping
				foreach(string HotReloadModuleName in Makefile.HotReloadModuleNames)
				{
					FileItem[] ModuleOutputItems = Makefile.ModuleNameToOutputItems[HotReloadModuleName];
					for(int Idx = 0; Idx < ModuleOutputItems.Length; Idx++)
					{
						FileReference NewLocation;
						if(HotReloadState.OriginalFileToHotReloadFile.TryGetValue(ModuleOutputItems[Idx].Location, out NewLocation))
						{
							ModuleOutputItems[Idx] = FileItem.GetItemByFileReference(NewLocation);
						}
					}
				}

				// If we want a specific suffix on any modules, apply that now. We'll track the outputs later, but the suffix has to be forced  (and is always out of date if it doesn't exist).
				if(HotReloadModuleNameToSuffix.Count > 0)
				{
					Dictionary<FileReference, FileReference> OldLocationToNewLocation = new Dictionary<FileReference, FileReference>();
					foreach(string HotReloadModuleName in Makefile.HotReloadModuleNames)
					{
						int ModuleSuffix;
						if(HotReloadModuleNameToSuffix.TryGetValue(HotReloadModuleName, out ModuleSuffix))
						{
							FileItem[] ModuleOutputItems = Makefile.ModuleNameToOutputItems[HotReloadModuleName];
							foreach(FileItem ModuleOutputItem in ModuleOutputItems)
							{
								FileReference OldLocation = ModuleOutputItem.Location;
								FileReference NewLocation = HotReload.ReplaceSuffix(OldLocation, ModuleSuffix);
								OldLocationToNewLocation[OldLocation] = NewLocation;
							}
						}
					}
					HotReload.PatchActionGraph(PrerequisiteActions, OldLocationToNewLocation);
				}

				// Re-link the action graph
				ActionGraph.Link(PrerequisiteActions);
			}

			// Create the dependencies cache
			CppDependencyCache CppDependencies;
			using(Timeline.ScopeEvent("Reading dependency cache"))
			{
				CppDependencies = CppDependencyCache.CreateHierarchy(TargetDescriptor.ProjectFile, TargetDescriptor.Name, TargetDescriptor.Platform, TargetDescriptor.Configuration, Makefile.TargetType);
			}

			// Create the action history
			ActionHistory History;
			using(Timeline.ScopeEvent("Reading action history"))
			{
				History = ActionHistory.CreateHierarchy(TargetDescriptor.ProjectFile, TargetDescriptor.Name, TargetDescriptor.Platform, Makefile.TargetType);
			}

			// Plan the actions to execute for the build.
			HashSet<Action> TargetActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, Makefile.Actions, PrerequisiteActions, CppDependencies, History);

			// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
			// the same for every invocation on that makefile, but we need a new suffix each time.
			if (HotReloadMode != HotReloadMode.Disabled)
			{
				// For all the hot-reloadable modules that may need a unique suffix appended, build a mapping from output item to all the output items in that module. We can't 
				// apply a suffix to one without applying a suffix to all of them.
				Dictionary<FileItem, FileItem[]> HotReloadItemToDependentItems = new Dictionary<FileItem, FileItem[]>();
				foreach(string HotReloadModuleName in Makefile.HotReloadModuleNames)
				{
					int ModuleSuffix;
					if(!HotReloadModuleNameToSuffix.TryGetValue(HotReloadModuleName, out ModuleSuffix) || ModuleSuffix == -1)
					{
						FileItem[] ModuleOutputItems;
						if(Makefile.ModuleNameToOutputItems.TryGetValue(HotReloadModuleName, out ModuleOutputItems))
						{
							foreach(FileItem ModuleOutputItem in ModuleOutputItems)
							{
								HotReloadItemToDependentItems[ModuleOutputItem] = ModuleOutputItems;
							}
						}
					} 
				}

				// Expand the list of actions to execute to include everything that references any files with a new suffix. Unlike a regular build, we can't ignore
				// dependencies on import libraries under the assumption that a header would change if the API changes, because the dependency will be on a different DLL.
				HashSet<FileItem> FilesRequiringSuffix = new HashSet<FileItem>(TargetActionsToExecute.SelectMany(x => x.ProducedItems).Where(x => HotReloadItemToDependentItems.ContainsKey(x)));
				for(int LastNumFilesWithNewSuffix = 0; FilesRequiringSuffix.Count > LastNumFilesWithNewSuffix;)
				{
					LastNumFilesWithNewSuffix = FilesRequiringSuffix.Count;
					foreach(Action PrerequisiteAction in PrerequisiteActions)
					{
						if(!TargetActionsToExecute.Contains(PrerequisiteAction))
						{
							foreach(FileItem ProducedItem in PrerequisiteAction.ProducedItems)
							{
								FileItem[] DependentItems;
								if(HotReloadItemToDependentItems.TryGetValue(ProducedItem, out DependentItems))
								{
									TargetActionsToExecute.Add(PrerequisiteAction);
									FilesRequiringSuffix.UnionWith(DependentItems);
								}
							}
						}
					}
				}

				// Build a list of file mappings
				Dictionary<FileReference, FileReference> OldLocationToNewLocation = new Dictionary<FileReference, FileReference>();
				foreach(FileItem FileRequiringSuffix in FilesRequiringSuffix)
				{
					FileReference OldLocation = FileRequiringSuffix.Location;
					FileReference NewLocation = HotReload.ReplaceSuffix(OldLocation, HotReloadState.NextSuffix);
					OldLocationToNewLocation[OldLocation] = NewLocation;
				}

				// Update the action graph with these new paths
				HotReload.PatchActionGraph(PrerequisiteActions, OldLocationToNewLocation);

				// Get a new list of actions to execute now that the graph has been modified
				TargetActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, Makefile.Actions, PrerequisiteActions, CppDependencies, History);

				// Build a mapping of all file items to their original
				Dictionary<FileReference, FileReference> HotReloadFileToOriginalFile = new Dictionary<FileReference, FileReference>();
				foreach(KeyValuePair<FileReference, FileReference> Pair in HotReloadState.OriginalFileToHotReloadFile)
				{
					HotReloadFileToOriginalFile[Pair.Value] = Pair.Key;
				}
				foreach(KeyValuePair<FileReference, FileReference> Pair in OldLocationToNewLocation)
				{
					FileReference OriginalLocation;
					if(!HotReloadFileToOriginalFile.TryGetValue(Pair.Key, out OriginalLocation))
					{
						OriginalLocation = Pair.Key;
					}
					HotReloadFileToOriginalFile[Pair.Value] = OriginalLocation;
				}

				// Now filter out all the hot reload files and update the state
				foreach(Action Action in TargetActionsToExecute)
				{
					foreach(FileItem ProducedItem in Action.ProducedItems)
					{
						FileReference OriginalLocation;
						if(HotReloadFileToOriginalFile.TryGetValue(ProducedItem.Location, out OriginalLocation))
						{
							HotReloadState.OriginalFileToHotReloadFile[OriginalLocation] = ProducedItem.Location;
							HotReloadState.TemporaryFiles.Add(ProducedItem.Location);
						}
					}
				}

				// Increment the suffix for the next iteration
				if(TargetActionsToExecute.Count > 0)
				{
					HotReloadState.NextSuffix++;
				}

				// Save the new state
				HotReloadState.Save(HotReloadStateFile);

				// Prevent this target from deploying
				Makefile.bDeployAfterCompile = false;
			}

			return TargetActionsToExecute;
		}
	}
}

