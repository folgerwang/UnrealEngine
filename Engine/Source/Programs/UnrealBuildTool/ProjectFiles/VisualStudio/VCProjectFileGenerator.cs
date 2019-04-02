// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a folder within the master project (e.g. Visual Studio solution)
	/// </summary>
	class VisualStudioSolutionFolder : MasterProjectFolder
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public VisualStudioSolutionFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
			// Generate a unique GUID for this folder
			// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
			//       matches identically with the pre-existing file
			FolderGUID = Guid.NewGuid();
		}


		/// GUID for this folder
		public Guid FolderGUID
		{
			get;
			private set;
		}
	}

	enum VCProjectFileFormat
	{
		Default,          // Default to the best installed version, but allow SDKs to override
		VisualStudio2012, // Unsupported
		VisualStudio2013, // Unsupported
		VisualStudio2015,
		VisualStudio2017,
		VisualStudio2019,
	}

	/// <summary>
	/// Visual C++ project file generator implementation
	/// </summary>
	class VCProjectFileGenerator : ProjectFileGenerator
	{
		/// <summary>
		/// The version of Visual Studio to generate project files for.
		/// </summary>
		[XmlConfigFile(Name = "Version")]
		protected VCProjectFileFormat ProjectFileFormat = VCProjectFileFormat.Default;

		/// <summary>
		/// Whether to write a solution option (suo) file for the sln
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		protected bool bWriteSolutionOptionFile = true;

		/// <summary>
		/// Whether to add the -FastPDB option to build command lines by default
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bAddFastPDBToProjects = false;

		/// <summary>
		/// Whether to generate per-file intellisense data
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bUsePerFileIntellisense = false;

		/// <summary>
		/// Whether to include a dependency on ShaderCompileWorker when generating project files for the editor.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		bool bEditorDependsOnShaderCompileWorker = true;

		/// <summary>
		/// Override for the build tool to use in generated projects. If the compiler version is specified on the command line, we use the same argument on the 
		/// command line for generated projects.
		/// </summary>
		string BuildToolOverride;

		/// <summary>
		/// Default constructor
		/// </summary>
		/// <param name="InOnlyGameProject">The single project to generate project files for, or null</param>
		/// <param name="InProjectFileFormat">Override the project file format to use</param>
		/// <param name="InArguments">Additional command line arguments</param>
		public VCProjectFileGenerator(FileReference InOnlyGameProject, VCProjectFileFormat InProjectFileFormat, CommandLineArguments InArguments)
			: base(InOnlyGameProject)
		{
			XmlConfig.ApplyTo(this);

			if(InProjectFileFormat != VCProjectFileFormat.Default)
			{
				ProjectFileFormat = InProjectFileFormat;
			}

			if(InArguments.HasOption("-2015"))
			{
				BuildToolOverride = "-2015";
			}
			else if(InArguments.HasOption("-2017"))
			{
				BuildToolOverride = "-2017";
			}
			else if(InArguments.HasOption("-2019"))
			{
				BuildToolOverride = "-2019";
			}
		}

		/// File extension for project files we'll be generating (e.g. ".vcxproj")
		override public string ProjectFileExtension
		{
			get
			{
				return ".vcxproj";
			}
		}

		/// <summary>
		/// </summary>
		public override void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesDirectory)
		{
			FileReference MasterProjectFile = FileReference.Combine(InMasterProjectDirectory, InMasterProjectName);
			FileReference MasterProjDeleteFilename = MasterProjectFile + ".sln";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".sdf";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".v11.suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}
			MasterProjDeleteFilename = MasterProjectFile + ".v12.suo";
			if (FileReference.Exists(MasterProjDeleteFilename))
			{
				FileReference.Delete(MasterProjDeleteFilename);
			}

			// Delete the project files folder
			if (DirectoryReference.Exists(InIntermediateProjectFilesDirectory))
			{
				try
				{
					DirectoryReference.Delete(InIntermediateProjectFilesDirectory, true);
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Error while trying to clean project files path {0}. Ignored.", InIntermediateProjectFilesDirectory);
					Log.TraceInformation("\t" + Ex.Message);
				}
			}
		}

		/// <summary>
		/// Allocates a generator-specific project file object
		/// </summary>
		/// <param name="InitFilePath">Path to the project file</param>
		/// <returns>The newly allocated project file object</returns>
		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath)
		{
			return new VCProjectFile(InitFilePath, OnlyGameProject, ProjectFileFormat, bAddFastPDBToProjects, bUsePerFileIntellisense, bUsePrecompiled, bEditorDependsOnShaderCompileWorker, BuildToolOverride);
		}


		/// ProjectFileGenerator interface
		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new VisualStudioSolutionFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}

		/// "4.0", "12.0", or "14.0", etc...
		static public string GetProjectFileToolVersionString(VCProjectFileFormat ProjectFileFormat)
		{
			switch (ProjectFileFormat)
            {
                case VCProjectFileFormat.VisualStudio2012:
                    return "4.0";
				case VCProjectFileFormat.VisualStudio2013:
					return "12.0";
				case VCProjectFileFormat.VisualStudio2015:
					return "14.0";
				case VCProjectFileFormat.VisualStudio2017:
					return "15.0";
				case VCProjectFileFormat.VisualStudio2019:
					return "15.0"; // Correct as of VS2019 Preview 1
			}
			return string.Empty;
		}

		/// for instance: <PlatformToolset>v110</PlatformToolset>
		static public string GetProjectFilePlatformToolsetVersionString(VCProjectFileFormat ProjectFileFormat)
		{
            switch (ProjectFileFormat)
            {
                case VCProjectFileFormat.VisualStudio2012:
                    return "v110";
                case VCProjectFileFormat.VisualStudio2013:
                    return "v120";
                case VCProjectFileFormat.VisualStudio2015:
					return "v140";
				case VCProjectFileFormat.VisualStudio2017:
                    return "v141";
				case VCProjectFileFormat.VisualStudio2019:
					return "v142"; // Correct as of VS2019 Preview 2

            }
			return string.Empty;
		}

		/// <summary>
		/// Configures project generator based on command-line options
		/// </summary>
		/// <param name="Arguments">Arguments passed into the program</param>
		/// <param name="IncludeAllPlatforms">True if all platforms should be included</param>
		protected override void ConfigureProjectFileGeneration(String[] Arguments, ref bool IncludeAllPlatforms)
		{
			// Call parent implementation first
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms);
		}

		/// <summary>
		/// Selects which platforms and build configurations we want in the project file
		/// </summary>
		/// <param name="IncludeAllPlatforms">True if we should include ALL platforms that are supported on this machine.  Otherwise, only desktop platforms will be included.</param>
		/// <param name="SupportedPlatformNames">Output string for supported platforms, returned as comma-separated values.</param>
		protected override void SetupSupportedPlatformsAndConfigurations(bool IncludeAllPlatforms, out string SupportedPlatformNames)
		{
			// Call parent implementation to figure out the actual platforms
			base.SetupSupportedPlatformsAndConfigurations(IncludeAllPlatforms, out SupportedPlatformNames);

			// If we have a non-default setting for visual studio, check the compiler exists. If not, revert to the default.
			if(ProjectFileFormat == VCProjectFileFormat.VisualStudio2015)
			{
				if (!WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2015_DEPRECATED))
				{
					Log.TraceWarning("Visual Studio C++ 2015 installation not found - ignoring preferred project file format.");
					ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}
			else if(ProjectFileFormat == VCProjectFileFormat.VisualStudio2017)
			{
				if (!WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2017))
				{
					Log.TraceWarning("Visual Studio C++ 2017 installation not found - ignoring preferred project file format.");
					ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}
			else if(ProjectFileFormat == VCProjectFileFormat.VisualStudio2019)
			{
				if (!WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2019))
				{
					Log.TraceWarning("Visual Studio C++ 2019 installation not found - ignoring preferred project file format.");
					ProjectFileFormat = VCProjectFileFormat.Default;
				}
			}

			// Certain platforms override the project file format because their debugger add-ins may not yet support the latest
			// version of Visual Studio.  This is their chance to override that.
			// ...but only if the user didn't override this via the command-line.
			if (ProjectFileFormat == VCProjectFileFormat.Default)
			{
				// Pick the best platform installed by default
				if (WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2017) && WindowsPlatform.HasIDE(WindowsCompiler.VisualStudio2017))
				{
					ProjectFileFormat = VCProjectFileFormat.VisualStudio2017;
				}
				else if (WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2015_DEPRECATED) && WindowsPlatform.HasIDE(WindowsCompiler.VisualStudio2015_DEPRECATED))
				{
					ProjectFileFormat = VCProjectFileFormat.VisualStudio2015;
				}
				else if (WindowsPlatform.HasCompiler(WindowsCompiler.VisualStudio2019) && WindowsPlatform.HasIDE(WindowsCompiler.VisualStudio2019))
				{
					ProjectFileFormat = VCProjectFileFormat.VisualStudio2019;
				}

				// Allow the SDKs to override
				foreach (UnrealTargetPlatform SupportedPlatform in SupportedPlatforms)
				{
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(SupportedPlatform, true);
					if (BuildPlatform != null)
					{
						// Don't worry about platforms that we're missing SDKs for
						if (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
						{
							VCProjectFileFormat ProposedFormat = BuildPlatform.GetRequiredVisualStudioVersion();

							if (ProposedFormat != VCProjectFileFormat.Default)
							{
								// Reduce the Visual Studio version to the max supported by each platform we plan to include.
								if (ProjectFileFormat == VCProjectFileFormat.Default || ProposedFormat < ProjectFileFormat)
								{
									ProjectFileFormat = ProposedFormat;
								}
							}
						}
					}
				}
			}
		}


		/// <summary>
		/// Used to sort VC solution config names along with the config and platform values
		/// </summary>
		class VCSolutionConfigCombination
		{
			/// <summary>
			/// Visual Studio solution configuration name for this config+platform
			/// </summary>
			public string VCSolutionConfigAndPlatformName;

			/// <summary>
			/// Configuration name
			/// </summary>
			public UnrealTargetConfiguration Configuration;

			/// <summary>
			/// Platform name
			/// </summary>
			public UnrealTargetPlatform Platform;

			/// <summary>
			/// The target type
			/// </summary>
			public TargetType TargetConfigurationName;

			public override string ToString()
			{
				return String.Format("{0}={1} {2} {3}", VCSolutionConfigAndPlatformName, Configuration, Platform, TargetConfigurationName);
			}
		}


		/// <summary>
		/// Composes a string to use for the Visual Studio solution configuration, given a build configuration and target rules configuration name
		/// </summary>
		/// <param name="Configuration">The build configuration</param>
		/// <param name="TargetType">The type of target being built</param>
		/// <returns>The generated solution configuration name</returns>
		string MakeSolutionConfigurationName(UnrealTargetConfiguration Configuration, TargetType TargetType)
		{
			string SolutionConfigName = Configuration.ToString();

			// Don't bother postfixing "Game" or "Program" -- that will be the default when using "Debug", "Development", etc.
			// Also don't postfix "RocketGame" when we're building Rocket game projects.  That's the only type of game there is in that case!
			if (TargetType != TargetType.Game && TargetType != TargetType.Program)
			{
				SolutionConfigName += " " + TargetType.ToString();
			}

			return SolutionConfigName;
		}

		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected override bool WriteProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			if(!base.WriteProjectFiles(PlatformProjectGenerators))
			{
				return false;
			}

			// Write AutomationReferences file
			if (AutomationProjectFiles.Any())
			{
				XNamespace NS = XNamespace.Get("http://schemas.microsoft.com/developer/msbuild/2003");

				DirectoryReference AutomationToolDir = DirectoryReference.Combine(UnrealBuildTool.EngineSourceDirectory, "Programs", "AutomationTool");
				new XDocument(
					new XElement(NS + "Project",
						new XAttribute("ToolsVersion", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat)),
						new XAttribute("DefaultTargets", "Build"),
						new XElement(NS + "ItemGroup",
							from AutomationProject in AutomationProjectFiles
							select new XElement(NS + "ProjectReference",
								new XAttribute("Include", AutomationProject.ProjectFilePath.MakeRelativeTo(AutomationToolDir)),
								new XElement(NS + "Project", (AutomationProject as VCSharpProjectFile).ProjectGUID.ToString("B")),
								new XElement(NS + "Name", AutomationProject.ProjectFilePath.GetFileNameWithoutExtension()),
								new XElement(NS + "Private", "false")
							)
						)
					)
				).Save(FileReference.Combine(AutomationToolDir, "AutomationTool.csproj.References").FullName);
			}

			return true;
		}


		protected override bool WriteMasterProjectFile(ProjectFile UBTProject, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			bool bSuccess = true;

			string SolutionFileName = MasterProjectName + ".sln";

			// Setup solution file content
			StringBuilder VCSolutionFileContent = new StringBuilder();

			// Solution file header. Note that a leading newline is required for file type detection to work correclty in the shell.
			if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2019)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio Version 16");
				VCSolutionFileContent.AppendLine("VisualStudioVersion = 16.0.28315.86");
				VCSolutionFileContent.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");
			}
			else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2017)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio 15");
				VCSolutionFileContent.AppendLine("VisualStudioVersion = 15.0.25807.0");
				VCSolutionFileContent.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");
			}
			else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2015)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio 14");
				VCSolutionFileContent.AppendLine("VisualStudioVersion = 14.0.22310.1");
				VCSolutionFileContent.AppendLine("MinimumVisualStudioVersion = 10.0.40219.1");
			}
			else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2013)
			{
				VCSolutionFileContent.AppendLine();
				VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio 2013");
            }
            else if (ProjectFileFormat == VCProjectFileFormat.VisualStudio2012)
            {
				VCSolutionFileContent.AppendLine();
                VCSolutionFileContent.AppendLine("Microsoft Visual Studio Solution File, Format Version 12.00");
				VCSolutionFileContent.AppendLine("# Visual Studio 2012");
            }
			else
			{
				throw new BuildException("Unexpected ProjectFileFormat");
			}

			// Solution folders, files and project entries
			{
				// This the GUID that Visual Studio uses to identify a solution folder
				string SolutionFolderEntryGUID = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}";

				// Solution folders
				{
					List<MasterProjectFolder> AllSolutionFolders = new List<MasterProjectFolder>();
					System.Action<List<MasterProjectFolder> /* Folders */ > GatherFoldersFunction = null;
					GatherFoldersFunction = FolderList =>
						{
							AllSolutionFolders.AddRange(FolderList);
							foreach (MasterProjectFolder CurSubFolder in FolderList)
							{
								GatherFoldersFunction(CurSubFolder.SubFolders);
							}
						};
					GatherFoldersFunction(RootFolder.SubFolders);

					foreach (VisualStudioSolutionFolder CurFolder in AllSolutionFolders)
					{
						string FolderGUIDString = CurFolder.FolderGUID.ToString("B").ToUpperInvariant();
						VCSolutionFileContent.AppendLine("Project(\"" + SolutionFolderEntryGUID + "\") = \"" + CurFolder.FolderName + "\", \"" + CurFolder.FolderName + "\", \"" + FolderGUIDString + "\"");

						// Add any files that are inlined right inside the solution folder
						if (CurFolder.Files.Count > 0)
						{
							VCSolutionFileContent.AppendLine("	ProjectSection(SolutionItems) = preProject");
							foreach (string CurFile in CurFolder.Files)
							{
								// Syntax is:  <relative file path> = <relative file path>
								VCSolutionFileContent.AppendLine("		" + CurFile + " = " + CurFile);
							}
							VCSolutionFileContent.AppendLine("	EndProjectSection");
						}

						VCSolutionFileContent.AppendLine("EndProject");
					}
				}


				// Project files
				foreach (MSBuildProjectFile CurProject in AllProjectFiles)
				{
					// Visual Studio uses different GUID types depending on the project type
					string ProjectTypeGUID = CurProject.ProjectTypeGUID;

					// NOTE: The project name in the solution doesn't actually *have* to match the project file name on disk.  However,
					//       we prefer it when it does match so we use the actual file name here.
					string ProjectNameInSolution = CurProject.ProjectFilePath.GetFileNameWithoutExtension();

					// Use the existing project's GUID that's already known to us
					string ProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();

					VCSolutionFileContent.AppendLine("Project(\"" + ProjectTypeGUID + "\") = \"" + ProjectNameInSolution + "\", \"" + CurProject.ProjectFilePath.MakeRelativeTo(ProjectFileGenerator.MasterProjectPath) + "\", \"" + ProjectGUID + "\"");

					// Setup dependency on UnrealBuildTool, if we need that.  This makes sure that UnrealBuildTool is
					// freshly compiled before kicking off any build operations on this target project
					if (!CurProject.IsStubProject)
					{
						List<ProjectFile> Dependencies = new List<ProjectFile>();
						if (CurProject.IsGeneratedProject && UBTProject != null && CurProject != UBTProject)
						{
							Dependencies.Add(UBTProject);
							Dependencies.AddRange(UBTProject.DependsOnProjects);
						}
						Dependencies.AddRange(CurProject.DependsOnProjects);

						if (Dependencies.Count > 0)
						{
							VCSolutionFileContent.AppendLine("\tProjectSection(ProjectDependencies) = postProject");

							// Setup any addition dependencies this project has...
							foreach (ProjectFile DependsOnProject in Dependencies)
							{
								string DependsOnProjectGUID = ((MSBuildProjectFile)DependsOnProject).ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.AppendLine("\t\t" + DependsOnProjectGUID + " = " + DependsOnProjectGUID);
							}

							VCSolutionFileContent.AppendLine("\tEndProjectSection");
						}
					}

					VCSolutionFileContent.AppendLine("EndProject");
				}

				// Get the path to the visualizers file. Try to make it relative to the solution directory, but fall back to a full path if it's a foreign project.
				FileReference VisualizersFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Extras", "VisualStudioDebugging", "UE4.natvis");

				// Add the visualizers at the solution level. Doesn't seem to be picked up from a makefile project in VS2017 15.8.5.
				VCSolutionFileContent.AppendLine(String.Format("Project(\"{{2150E333-8FDC-42A3-9474-1A3956D46DE8}}\") = \"Visualizers\", \"Visualizers\", \"{0}\"", Guid.NewGuid().ToString("B").ToUpperInvariant()));
				VCSolutionFileContent.AppendLine("\tProjectSection(SolutionItems) = preProject");
				VCSolutionFileContent.AppendLine("\t\t{0} = {0}", VisualizersFile.MakeRelativeTo(MasterProjectPath));
				VCSolutionFileContent.AppendLine("\tEndProjectSection");
				VCSolutionFileContent.AppendLine("EndProject");
			}

			// Solution configuration platforms.  This is just a list of all of the platforms and configurations that
			// appear in Visual Studio's build configuration selector.
			List<VCSolutionConfigCombination> SolutionConfigCombinations = new List<VCSolutionConfigCombination>();

			// The "Global" section has source control, solution configurations, project configurations,
			// preferences, and project hierarchy data
			{
				VCSolutionFileContent.AppendLine("Global");
				{
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(SolutionConfigurationPlatforms) = preSolution");

						Dictionary<string, Tuple<UnrealTargetConfiguration, TargetType>> SolutionConfigurationsValidForProjects = new Dictionary<string, Tuple<UnrealTargetConfiguration, TargetType>>();
						HashSet<UnrealTargetPlatform> PlatformsValidForProjects = new HashSet<UnrealTargetPlatform>();

						foreach (UnrealTargetConfiguration CurConfiguration in SupportedConfigurations)
						{
							if (InstalledPlatformInfo.IsValidConfiguration(CurConfiguration, EProjectType.Code))
							{
								foreach (UnrealTargetPlatform CurPlatform in SupportedPlatforms)
								{
									if (InstalledPlatformInfo.IsValidPlatform(CurPlatform, EProjectType.Code))
									{
										foreach (ProjectFile CurProject in AllProjectFiles)
										{
											if (!CurProject.IsStubProject)
											{
												if (CurProject.ProjectTargets.Count == 0)
												{
													throw new BuildException("Expecting project '" + CurProject.ProjectFilePath + "' to have at least one ProjectTarget associated with it!");
												}

												// Figure out the set of valid target configuration names
												foreach (ProjectTarget ProjectTarget in CurProject.ProjectTargets)
												{
													if (VCProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, CurPlatform, CurConfiguration, PlatformProjectGenerators))
													{
														PlatformsValidForProjects.Add(CurPlatform);

														// Default to a target configuration name of "Game", since that will collapse down to an empty string
														TargetType TargetType = TargetType.Game;
														if (ProjectTarget.TargetRules != null)
														{
															TargetType = ProjectTarget.TargetRules.Type;
														}

														string SolutionConfigName = MakeSolutionConfigurationName(CurConfiguration, TargetType);
														SolutionConfigurationsValidForProjects[SolutionConfigName] = new Tuple<UnrealTargetConfiguration, TargetType>(CurConfiguration, TargetType);
													}
												}
											}
										}
									}
								}
							}
						}

						foreach (UnrealTargetPlatform CurPlatform in PlatformsValidForProjects)
						{
							foreach (KeyValuePair<string, Tuple<UnrealTargetConfiguration, TargetType>> SolutionConfigKeyValue in SolutionConfigurationsValidForProjects)
							{
								// e.g.  "Development|Win64 = Development|Win64"
								string SolutionConfigName = SolutionConfigKeyValue.Key;
								UnrealTargetConfiguration Configuration = SolutionConfigKeyValue.Value.Item1;
								TargetType TargetType = SolutionConfigKeyValue.Value.Item2;

								string SolutionPlatformName = CurPlatform.ToString();

								string SolutionConfigAndPlatformPair = SolutionConfigName + "|" + SolutionPlatformName;
								SolutionConfigCombinations.Add(
										new VCSolutionConfigCombination
										{
											VCSolutionConfigAndPlatformName = SolutionConfigAndPlatformPair,
											Configuration = Configuration,
											Platform = CurPlatform,
											TargetConfigurationName = TargetType
										}
									);
							}
						}

						// Sort the list of solution platform strings alphabetically (Visual Studio prefers it)
						SolutionConfigCombinations.Sort(
								new Comparison<VCSolutionConfigCombination>(
									(x, y) => { return String.Compare(x.VCSolutionConfigAndPlatformName, y.VCSolutionConfigAndPlatformName, StringComparison.InvariantCultureIgnoreCase); }
								)
							);

						HashSet<string> AppendedSolutionConfigAndPlatformNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
						foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
						{
							// We alias "Game" and "Program" to both have the same solution configuration, so we're careful not to add the same combination twice.
							if (!AppendedSolutionConfigAndPlatformNames.Contains(SolutionConfigCombination.VCSolutionConfigAndPlatformName))
							{
								VCSolutionFileContent.AppendLine("		" + SolutionConfigCombination.VCSolutionConfigAndPlatformName + " = " + SolutionConfigCombination.VCSolutionConfigAndPlatformName);
								AppendedSolutionConfigAndPlatformNames.Add(SolutionConfigCombination.VCSolutionConfigAndPlatformName);
							}
						}

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}


					// Assign each project's "project configuration" to our "solution platform + configuration" pairs.  This
					// also sets up which projects are actually built when building the solution.
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(ProjectConfigurationPlatforms) = postSolution");

						foreach (MSBuildProjectFile CurProject in AllProjectFiles)
						{
							foreach (VCSolutionConfigCombination SolutionConfigCombination in SolutionConfigCombinations)
							{
								// Get the context for the current solution context
								MSBuildProjectContext ProjectContext = CurProject.GetMatchingProjectContext(SolutionConfigCombination.TargetConfigurationName, SolutionConfigCombination.Configuration, SolutionConfigCombination.Platform, PlatformProjectGenerators);

								// Write the solution mapping (e.g.  "{4232C52C-680F-4850-8855-DC39419B5E9B}.Debug|iOS.ActiveCfg = iOS_Debug|Win32")
								string CurProjectGUID = CurProject.ProjectGUID.ToString("B").ToUpperInvariant();
								VCSolutionFileContent.AppendLine("		{0}.{1}.ActiveCfg = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
								if (ProjectContext.bBuildByDefault)
								{
									VCSolutionFileContent.AppendLine("		{0}.{1}.Build.0 = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
									if(ProjectContext.bDeployByDefault)
									{
										VCSolutionFileContent.AppendLine("		{0}.{1}.Deploy.0 = {2}", CurProjectGUID, SolutionConfigCombination.VCSolutionConfigAndPlatformName, ProjectContext.Name);
									}
								}
							}
						}

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}


					// Setup other solution properties
					{
						// HideSolutionNode sets whether or not the top-level solution entry is completely hidden in the UI.
						// We don't want that, as we need users to be able to right click on the solution tree item.
						VCSolutionFileContent.AppendLine("	GlobalSection(SolutionProperties) = preSolution");
						VCSolutionFileContent.AppendLine("		HideSolutionNode = FALSE");
						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}



					// Solution directory hierarchy
					{
						VCSolutionFileContent.AppendLine("	GlobalSection(NestedProjects) = preSolution");

						// Every entry in this section is in the format "Guid1 = Guid2".  Guid1 is the child project (or solution
						// filter)'s GUID, and Guid2 is the solution filter directory to parent the child project (or solution
						// filter) to.  This sets up the hierarchical solution explorer tree for all solution folders and projects.

						System.Action<StringBuilder /* VCSolutionFileContent */, List<MasterProjectFolder> /* Folders */ > FolderProcessorFunction = null;
						FolderProcessorFunction = (LocalVCSolutionFileContent, LocalMasterProjectFolders) =>
							{
								foreach (VisualStudioSolutionFolder CurFolder in LocalMasterProjectFolders)
								{
									string CurFolderGUIDString = CurFolder.FolderGUID.ToString("B").ToUpperInvariant();

									foreach (MSBuildProjectFile ChildProject in CurFolder.ChildProjects)
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.AppendLine("		" + ChildProject.ProjectGUID.ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString);
									}

									foreach (VisualStudioSolutionFolder SubFolder in CurFolder.SubFolders)
									{
										//	e.g. "{BF6FB09F-A2A6-468F-BE6F-DEBE07EAD3EA} = {C43B6BB5-3EF0-4784-B896-4099753BCDA9}"
										LocalVCSolutionFileContent.AppendLine("		" + SubFolder.FolderGUID.ToString("B").ToUpperInvariant() + " = " + CurFolderGUIDString);
									}

									// Recurse into subfolders
									FolderProcessorFunction(LocalVCSolutionFileContent, CurFolder.SubFolders);
								}
							};
						FolderProcessorFunction(VCSolutionFileContent, RootFolder.SubFolders);

						VCSolutionFileContent.AppendLine("	EndGlobalSection");
					}
				}

				VCSolutionFileContent.AppendLine("EndGlobal");
			}


			// Save the solution file
			if (bSuccess)
			{
				string SolutionFilePath = FileReference.Combine(MasterProjectPath, SolutionFileName).FullName;
				bSuccess = WriteFileIfChanged(SolutionFilePath, VCSolutionFileContent.ToString());
			}


			// Save a solution config file which selects the development editor configuration by default.
			if (bSuccess && bWriteSolutionOptionFile)
			{
				// Figure out the filename for the SUO file. VS will automatically import the options from earlier versions if necessary.
				FileReference SolutionOptionsFileName;
				switch (ProjectFileFormat)
                {
                    case VCProjectFileFormat.VisualStudio2012:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, Path.ChangeExtension(SolutionFileName, "v11.suo"));
                        break;
					case VCProjectFileFormat.VisualStudio2013:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, Path.ChangeExtension(SolutionFileName, "v12.suo"));
						break;
					case VCProjectFileFormat.VisualStudio2015:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v14", ".suo");
						break;
					case VCProjectFileFormat.VisualStudio2017:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v15", ".suo");
						break;
					case VCProjectFileFormat.VisualStudio2019:
						SolutionOptionsFileName = FileReference.Combine(MasterProjectPath, ".vs", Path.GetFileNameWithoutExtension(SolutionFileName), "v16", ".suo");
						break;
					default:
						throw new BuildException("Unsupported Visual Studio version");
				}

				// Check it doesn't exist before overwriting it. Since these files store the user's preferences, it'd be bad form to overwrite them.
				if (!FileReference.Exists(SolutionOptionsFileName))
				{
					DirectoryReference.CreateDirectory(SolutionOptionsFileName.Directory);

					VCSolutionOptions Options = new VCSolutionOptions(ProjectFileFormat);

					// Set the default configuration and startup project
					VCSolutionConfigCombination DefaultConfig = SolutionConfigCombinations.Find(x => x.Configuration == UnrealTargetConfiguration.Development && x.Platform == UnrealTargetPlatform.Win64 && x.TargetConfigurationName == TargetType.Editor);
					if (DefaultConfig != null)
					{
						List<VCBinarySetting> Settings = new List<VCBinarySetting>();
						Settings.Add(new VCBinarySetting("ActiveCfg", DefaultConfig.VCSolutionConfigAndPlatformName));
						if (DefaultProject != null)
						{
							Settings.Add(new VCBinarySetting("StartupProject", ((MSBuildProjectFile)DefaultProject).ProjectGUID.ToString("B")));
						}
						Options.SetConfiguration(Settings);
					}

					// Mark all the projects as closed by default, apart from the startup project
					VCSolutionExplorerState ExplorerState = new VCSolutionExplorerState();
					if(ProjectFileFormat >= VCProjectFileFormat.VisualStudio2017)
					{
						BuildSolutionExplorerState_VS2017(RootFolder, "", ExplorerState, DefaultProject);
					}
					else
					{
						BuildSolutionExplorerState_VS2015(AllProjectFiles, ExplorerState, DefaultProject, IncludeEnginePrograms);
					}
					Options.SetExplorerState(ExplorerState);

					// Write the file
					if (Options.Sections.Count > 0)
					{
						Options.Write(SolutionOptionsFileName.FullName);
					}
				}
			}

			return bSuccess;
		}

		protected override void WriteDebugSolutionFiles( PlatformProjectGeneratorCollection PlatformProjectGenerators, DirectoryReference IntermediateProjectFilesPath )
		{
			//build and collect UnrealVS configuration
			StringBuilder UnrealVSContent = new StringBuilder();
			foreach (UnrealTargetPlatform SupportedPlatform in SupportedPlatforms)
			{
				PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(SupportedPlatform, true);
				if (ProjGenerator != null)
				{
					ProjGenerator.GetUnrealVSConfigurationEntries(UnrealVSContent);
				}
			}
			if (UnrealVSContent.Length > 0 )
			{
				UnrealVSContent.Insert(0, "<UnrealVS>" + ProjectFileGenerator.NewLine);
				UnrealVSContent.Append("</UnrealVS>" + ProjectFileGenerator.NewLine );

				string ConfigFilePath = FileReference.Combine(IntermediateProjectFilesPath, "UnrealVS.xml").FullName;
				bool bSuccess = ProjectFileGenerator.WriteFileIfChanged(ConfigFilePath, UnrealVSContent.ToString());
			}
		}

		static void BuildSolutionExplorerState_VS2017(MasterProjectFolder Folder, string Suffix, VCSolutionExplorerState ExplorerState, ProjectFile DefaultProject)
		{
			foreach(ProjectFile Project in Folder.ChildProjects)
			{
				string ProjectIdentifier = String.Format("{0}{1}", Project.ProjectFilePath.GetFileNameWithoutExtension(), Suffix);
				if (Project == DefaultProject)
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectIdentifier, new string[] { ProjectIdentifier }));
				}
				else
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectIdentifier, new string[] { }));
				}
			}

			foreach(MasterProjectFolder SubFolder in Folder.SubFolders)
			{
				string SubFolderName = SubFolder.FolderName + Suffix;
				if(SubFolderName == "Automation;Programs")
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(SubFolderName, new string[] { }));
				}
				else
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(SubFolderName, new string[] { SubFolderName }));
				}
				BuildSolutionExplorerState_VS2017(SubFolder, ";" + SubFolderName, ExplorerState, DefaultProject);
			}
		}

		static void BuildSolutionExplorerState_VS2015(List<ProjectFile> AllProjectFiles, VCSolutionExplorerState ExplorerState, ProjectFile DefaultProject, bool IncludeEnginePrograms)
		{
			foreach (ProjectFile ProjectFile in AllProjectFiles)
			{
				string ProjectName = ProjectFile.ProjectFilePath.GetFileNameWithoutExtension();
				if (ProjectFile == DefaultProject)
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[] { ProjectName }));
				}
				else
				{
					ExplorerState.OpenProjects.Add(new Tuple<string, string[]>(ProjectName, new string[] { }));
				}
			}
			if (IncludeEnginePrograms)
			{
				ExplorerState.OpenProjects.Add(new Tuple<string, string[]>("Automation", new string[0]));
			}
		}

		/// <summary>
		/// Takes a string and "cleans it up" to make it parsable by the Visual Studio source control provider's file format
		/// </summary>
		/// <param name="Str">String to clean up</param>
		/// <returns>The cleaned up string</returns>
		public string CleanupStringForSCC(string Str)
		{
			string Cleaned = Str;

			// SCC is expecting paths to contain only double-backslashes for path separators.  It's a bit weird but we need to do it.
			Cleaned = Cleaned.Replace(Path.DirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());
			Cleaned = Cleaned.Replace(Path.AltDirectorySeparatorChar.ToString(), Path.DirectorySeparatorChar.ToString() + Path.DirectorySeparatorChar.ToString());

			// SCC is expecting not to see spaces in these strings, so we'll replace spaces with "\u0020"
			Cleaned = Cleaned.Replace(" ", "\\u0020");

			return Cleaned;
		}

	}

}
