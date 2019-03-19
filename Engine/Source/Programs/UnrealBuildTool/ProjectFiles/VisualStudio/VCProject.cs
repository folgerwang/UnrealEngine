// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Linq;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.Security;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Specifies the context for building an MSBuild project
	/// </summary>
	class MSBuildProjectContext
	{
		/// <summary>
		/// Name of the configuration
		/// </summary>
		public string ConfigurationName;

		/// <summary>
		/// Name of the platform
		/// </summary>
		public string PlatformName;

		/// <summary>
		/// Whether this context should be built by default
		/// </summary>
		public bool bBuildByDefault;

		/// <summary>
		/// Whether this context should be deployed by default
		/// </summary>
		public bool bDeployByDefault;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ConfigurationName">Name of this configuration</param>
		/// <param name="PlatformName">Name of this platform</param>
		public MSBuildProjectContext(string ConfigurationName, string PlatformName)
		{
			this.ConfigurationName = ConfigurationName;
			this.PlatformName = PlatformName;
		}

		/// <summary>
		/// The name of this context
		/// </summary>
		public string Name
		{
			get { return String.Format("{0}|{1}", ConfigurationName, PlatformName); }
		}

		/// <summary>
		/// Serializes this context to a string for debugging
		/// </summary>
		/// <returns>Name of this context</returns>
		public override string ToString()
		{
			return Name;
		}
	}

	/// <summary>
	/// Represents an arbitrary MSBuild project
	/// </summary>
	abstract class MSBuildProjectFile : ProjectFile
	{
		/// The project file version string
		static public readonly string VCProjectFileVersionString = "10.0.30319.1";

		/// The build configuration name to use for stub project configurations.  These are projects whose purpose
		/// is to make it easier for developers to find source files and to provide IntelliSense data for the module
		/// to Visual Studio
		static public readonly string StubProjectConfigurationName = "BuiltWithUnrealBuildTool";

		/// The name of the Visual C++ platform to use for stub project configurations
		/// NOTE: We always use Win32 for the stub project's platform, since that is guaranteed to be supported by Visual Studio
		static public readonly string StubProjectPlatformName = "Win32";

		/// <summary>
		/// The Guid representing the project type e.g. C# or C++
		/// </summary>
		public virtual string ProjectTypeGUID
		{
			get { throw new BuildException("Unrecognized type of project file for Visual Studio solution"); }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public MSBuildProjectFile(FileReference InitFilePath)
			: base(InitFilePath)
		{
			// Each project gets its own GUID.  This is stored in the project file and referenced in the solution file.

			// First, check to see if we have an existing file on disk.  If we do, then we'll try to preserve the
			// GUID by loading it from the existing file.
			if (FileReference.Exists(ProjectFilePath))
			{
				try
				{
					LoadGUIDFromExistingProject();
				}
				catch (Exception)
				{
					// Failed to find GUID, so just create a new one
					ProjectGUID = Guid.NewGuid();
				}
			}

			if (ProjectGUID == Guid.Empty)
			{
				// Generate a brand new GUID
				ProjectGUID = Guid.NewGuid();
			}
		}


		/// <summary>
		/// Attempts to load the project's GUID from an existing project file on disk
		/// </summary>
		public override void LoadGUIDFromExistingProject()
		{
			// Only load GUIDs if we're in project generation mode.  Regular builds don't need GUIDs for anything.
			if (ProjectFileGenerator.bGenerateProjectFiles)
			{
				XmlDocument Doc = new XmlDocument();
				Doc.Load(ProjectFilePath.FullName);

				// @todo projectfiles: Ideally we could do a better job about preserving GUIDs when only minor changes are made
				// to the project (such as adding a single new file.) It would make diffing changes much easier!

				// @todo projectfiles: Can we "seed" a GUID based off the project path and generate consistent GUIDs each time?

				XmlNodeList Elements = Doc.GetElementsByTagName("ProjectGuid");
				foreach (XmlElement Element in Elements)
				{
					ProjectGUID = Guid.ParseExact(Element.InnerText.Trim("{}".ToCharArray()), "D");
				}
			}
		}

		/// <summary>
		/// Get the project context for the given solution context
		/// </summary>
		/// <param name="SolutionTarget">The solution target type</param>
		/// <param name="SolutionConfiguration">The solution configuration</param>
		/// <param name="SolutionPlatform">The solution platform</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generators</param>
		/// <returns>Project context matching the given solution context</returns>
		public abstract MSBuildProjectContext GetMatchingProjectContext(TargetType SolutionTarget, UnrealTargetConfiguration SolutionConfiguration, UnrealTargetPlatform SolutionPlatform, PlatformProjectGeneratorCollection PlatformProjectGenerators);

		static UnrealTargetConfiguration[] GetSupportedConfigurations(TargetRules Rules)
		{
			// Otherwise take the SupportedConfigurationsAttribute from the first type in the inheritance chain that supports it
			for (Type CurrentType = Rules.GetType(); CurrentType != null; CurrentType = CurrentType.BaseType)
			{
				object[] Attributes = Rules.GetType().GetCustomAttributes(typeof(SupportedConfigurationsAttribute), false);
				if (Attributes.Length > 0)
				{
					return Attributes.OfType<SupportedConfigurationsAttribute>().SelectMany(x => x.Configurations).Distinct().ToArray();
				}
			}

			// Otherwise, get the default for the target type
			if(Rules.Type == TargetType.Editor)
			{
				return new[] { UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.DebugGame, UnrealTargetConfiguration.Development };
			}
			else
			{
				return ((UnrealTargetConfiguration[])Enum.GetValues(typeof(UnrealTargetConfiguration))).Where(x => x != UnrealTargetConfiguration.Unknown).ToArray();
			}
		}

		/// <summary>
		/// Checks to see if the specified solution platform and configuration is able to map to this project
		/// </summary>
		/// <param name="ProjectTarget">The target that we're checking for a valid platform/config combination</param>
		/// <param name="Platform">Platform</param>
		/// <param name="Configuration">Configuration</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generators</param>
		/// <returns>True if this is a valid combination for this project, otherwise false</returns>
		public static bool IsValidProjectPlatformAndConfiguration(ProjectTarget ProjectTarget, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			if (!ProjectFileGenerator.bIncludeTestAndShippingConfigs)
			{
				if(Configuration == UnrealTargetConfiguration.Test || Configuration == UnrealTargetConfiguration.Shipping)
				{
					return false;
				}
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
			if (BuildPlatform == null)
			{
				return false;
			}

			if (BuildPlatform.HasRequiredSDKsInstalled() != SDKStatus.Valid)
			{
				return false;
			}


			List<UnrealTargetConfiguration> SupportedConfigurations = new List<UnrealTargetConfiguration>();
			List<UnrealTargetPlatform> SupportedPlatforms = new List<UnrealTargetPlatform>();
			if (ProjectTarget.TargetRules != null)
			{
				SupportedPlatforms.AddRange(ProjectTarget.SupportedPlatforms);
				SupportedConfigurations.AddRange(GetSupportedConfigurations(ProjectTarget.TargetRules));
			}

			// Add all of the extra platforms/configurations for this target
			{
				foreach (UnrealTargetPlatform ExtraPlatform in ProjectTarget.ExtraSupportedPlatforms)
				{
					if (!SupportedPlatforms.Contains(ExtraPlatform))
					{
						SupportedPlatforms.Add(ExtraPlatform);
					}
				}
				foreach (UnrealTargetConfiguration ExtraConfig in ProjectTarget.ExtraSupportedConfigurations)
				{
					if (!SupportedConfigurations.Contains(ExtraConfig))
					{
						SupportedConfigurations.Add(ExtraConfig);
					}
				}
			}

			// Only build for supported platforms
			if (SupportedPlatforms.Contains(Platform) == false)
			{
				return false;
			}

			// Only build for supported configurations
			if (SupportedConfigurations.Contains(Configuration) == false)
			{
				return false;
			}

			return true;
		}

		/// <summary>
		/// Escapes characters in a filename so they can be stored in an XML attribute
		/// </summary>
		/// <param name="FileName">The filename to escape</param>
		/// <returns>The escaped filename</returns>
		public static string EscapeFileName(string FileName)
		{
			return SecurityElement.Escape(FileName);
		}

		/// <summary>
		/// GUID for this Visual C++ project file
		/// </summary>
		public Guid ProjectGUID
		{
			get;
			protected set;
		}
	}

	class VCProjectFile : MSBuildProjectFile
	{
		FileReference OnlyGameProject;
		VCProjectFileFormat ProjectFileFormat;
		bool bUseFastPDB;
		bool bUsePerFileIntellisense;
		bool bUsePrecompiled;
		bool bEditorDependsOnShaderCompileWorker;
		string BuildToolOverride;

		/// This is the platform name that Visual Studio is always guaranteed to support.  We'll use this as
		/// a platform for any project configurations where our actual platform is not supported by the
		/// installed version of Visual Studio (e.g, "iOS")
		public const string DefaultPlatformName = "Win32";

		// This is the GUID that Visual Studio uses to identify a C++ project file in the solution
		public override string ProjectTypeGUID
		{
			get { return "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"; }
		}

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InFilePath">The path to the project file on disk</param>
		/// <param name="InOnlyGameProject"></param>
		/// <param name="InProjectFileFormat">Visual C++ project file version</param>
		/// <param name="bUseFastPDB">If true, adds the -FastPDB argument to build command lines</param>
		/// <param name="bUsePerFileIntellisense">If true, generates per-file intellisense data</param>
		/// <param name="bUsePrecompiled">Whether to add the -UsePrecompiled argumemnt when building targets</param>
		/// <param name="bEditorDependsOnShaderCompileWorker">Whether editor targets should also build ShaderCompileWorker</param>
		/// <param name="BuildToolOverride">Optional arguments to pass to UBT when building</param>
		public VCProjectFile(FileReference InFilePath, FileReference InOnlyGameProject, VCProjectFileFormat InProjectFileFormat, bool bUseFastPDB, bool bUsePerFileIntellisense, bool bUsePrecompiled, bool bEditorDependsOnShaderCompileWorker, string BuildToolOverride)
			: base(InFilePath)
		{
			OnlyGameProject = InOnlyGameProject;
			ProjectFileFormat = InProjectFileFormat;
			this.bUseFastPDB = bUseFastPDB;
			this.bUsePerFileIntellisense = bUsePerFileIntellisense;
			this.bUsePrecompiled = bUsePrecompiled;
			this.bEditorDependsOnShaderCompileWorker = bEditorDependsOnShaderCompileWorker;
			this.BuildToolOverride = BuildToolOverride;
		}

		/// <summary>
		/// Given a target platform and configuration, generates a platform and configuration name string to use in Visual Studio projects.
		/// Unlike with solution configurations, Visual Studio project configurations only support certain types of platforms, so we'll
		/// generate a configuration name that has the platform "built in", and use a default platform type
		/// </summary>
		/// <param name="Platform">Actual platform</param>
		/// <param name="Configuration">Actual configuration</param>
		/// <param name="TargetConfigurationName">The configuration name from the target rules, or null if we don't have one</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generators</param>
		/// <param name="ProjectPlatformName">Name of platform string to use for Visual Studio project</param>
		/// <param name="ProjectConfigurationName">Name of configuration string to use for Visual Studio project</param>
		private void MakeProjectPlatformAndConfigurationNames(UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetType TargetConfigurationName, PlatformProjectGeneratorCollection PlatformProjectGenerators, out string ProjectPlatformName, out string ProjectConfigurationName)
		{
			PlatformProjectGenerator PlatformProjectGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, bInAllowFailure: true);

			// Check to see if this platform is supported directly by Visual Studio projects.
			bool HasActualVSPlatform = (PlatformProjectGenerator != null) ? PlatformProjectGenerator.HasVisualStudioSupport(Platform, Configuration, ProjectFileFormat) : false;

			if (HasActualVSPlatform)
			{
				// Great!  Visual Studio supports this platform natively, so we don't need to make up
				// a fake project configuration name.

				// Allow the platform to specify the name used in VisualStudio.
				// Note that the actual name of the platform on the Visual Studio side may be different than what
				// UnrealBuildTool calls it (e.g. "Win64" -> "x64".) GetVisualStudioPlatformName() will figure this out.
				ProjectConfigurationName = Configuration.ToString();
				ProjectPlatformName = PlatformProjectGenerator.GetVisualStudioPlatformName(Platform, Configuration);
			}
			else
			{
				// Visual Studio doesn't natively support this platform, so we fake it by mapping it to
				// a project configuration that has the platform name in that configuration as a suffix,
				// and then using "Win32" as the actual VS platform name
				ProjectConfigurationName = Platform.ToString() + "_" + Configuration.ToString();
				ProjectPlatformName = DefaultPlatformName;
			}

			if(TargetConfigurationName != TargetType.Game)
			{
				ProjectConfigurationName += "_" + TargetConfigurationName.ToString();
			}
		}

		/// <summary>
		/// Get the project context for the given solution context
		/// </summary>
		/// <param name="SolutionTarget">The solution target type</param>
		/// <param name="SolutionConfiguration">The solution configuration</param>
		/// <param name="SolutionPlatform">The solution platform</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generations</param>
		/// <returns>Project context matching the given solution context</returns>
		public override MSBuildProjectContext GetMatchingProjectContext(TargetType SolutionTarget, UnrealTargetConfiguration SolutionConfiguration, UnrealTargetPlatform SolutionPlatform, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			// Stub projects always build in the same configuration
			if(IsStubProject)
			{
				return new MSBuildProjectContext(StubProjectConfigurationName, StubProjectPlatformName);
			}

			// Have to match every solution configuration combination to a project configuration (or use the invalid one)
			string ProjectConfigurationName = "Invalid";

			// Get the default platform. If there were not valid platforms for this project, just use one that will always be available in VS.
			string ProjectPlatformName = InvalidConfigPlatformNames.First();

			// Whether the configuration should be built automatically as part of the solution
			bool bBuildByDefault = false;

			// Whether this configuration should deploy by default (requires bBuildByDefault)
			bool bDeployByDefault = false;

			// Programs are built in editor configurations (since the editor is like a desktop program too) and game configurations (since we omit the "game" qualification in the configuration name).
			bool IsProgramProject = ProjectTargets[0].TargetRules != null && ProjectTargets[0].TargetRules.Type == TargetType.Program;
			if(!IsProgramProject || SolutionTarget == TargetType.Game || SolutionTarget == TargetType.Editor)
			{
				// Get the target type we expect to find for this project
				TargetType TargetConfigurationName = SolutionTarget;
				if (IsProgramProject)
				{
					TargetConfigurationName = TargetType.Program;
				}

				// Now, we want to find a target in this project that maps to the current solution config combination.  Only up to one target should
				// and every solution config combination should map to at least one target in one project (otherwise we shouldn't have added it!).
				List<ProjectTarget> MatchingProjectTargets = new List<ProjectTarget>();
				foreach (ProjectTarget ProjectTarget in ProjectTargets)
				{
					if(VCProjectFile.IsValidProjectPlatformAndConfiguration(ProjectTarget, SolutionPlatform, SolutionConfiguration, PlatformProjectGenerators))
					{
						if (ProjectTarget.TargetRules != null)
						{
							if (TargetConfigurationName == ProjectTarget.TargetRules.Type)
							{
								MatchingProjectTargets.Add(ProjectTarget);
							}
						}
						else
						{
							if (ShouldBuildForAllSolutionTargets || TargetConfigurationName == TargetType.Game)
							{
								MatchingProjectTargets.Add(ProjectTarget);
							}
						}
					}
				}

				// Always allow SCW and UnrealLighmass to build in editor configurations
				if (MatchingProjectTargets.Count == 0 && SolutionTarget == TargetType.Editor && SolutionPlatform == UnrealTargetPlatform.Win64)
				{
					foreach(ProjectTarget ProjectTarget in ProjectTargets)
					{
						string TargetName = ProjectTargets[0].TargetRules.Name;
						if(TargetName == "ShaderCompileWorker" || TargetName == "UnrealLightmass")
						{
							MatchingProjectTargets.Add(ProjectTarget);
							break;
						}
					}
				}

				// Make sure there's only one matching project target
				if(MatchingProjectTargets.Count > 1)
				{
					throw new BuildException("Not expecting more than one target for project {0} to match solution configuration {1} {2} {3}", ProjectFilePath, SolutionTarget, SolutionConfiguration, SolutionPlatform);
				}

				// If we found a matching project, get matching configuration
				if(MatchingProjectTargets.Count == 1)
				{
					// Get the matching target
					ProjectTarget MatchingProjectTarget = MatchingProjectTargets[0];

					// If the project wants to always build in "Development", regardless of what the solution configuration is set to, then we'll do that here.
					UnrealTargetConfiguration ProjectConfiguration = SolutionConfiguration;
					if (MatchingProjectTarget.ForceDevelopmentConfiguration && SolutionTarget != TargetType.Game)
					{
						ProjectConfiguration = UnrealTargetConfiguration.Development;
					}

					// Get the matching project configuration
					UnrealTargetPlatform ProjectPlatform = SolutionPlatform;
					if (IsStubProject)
					{
						if (ProjectPlatform != UnrealTargetPlatform.Unknown || ProjectConfiguration != UnrealTargetConfiguration.Unknown)
						{
							throw new BuildException("Stub project was expecting platform and configuration type to be set to Unknown");
						}
						ProjectConfigurationName = StubProjectConfigurationName;
						ProjectPlatformName = StubProjectPlatformName;
					}
					else
					{
						MakeProjectPlatformAndConfigurationNames(ProjectPlatform, ProjectConfiguration, TargetConfigurationName, PlatformProjectGenerators, out ProjectPlatformName, out ProjectConfigurationName);
					}

					// Set whether this project configuration should be built when the user initiates "build solution"
					if (ShouldBuildByDefaultForSolutionTargets)
					{
						// Some targets are "dummy targets"; they only exist to show user friendly errors in VS. Weed them out here, and don't set them to build by default.
						List<UnrealTargetPlatform> SupportedPlatforms = null;
						if (MatchingProjectTarget.TargetRules != null)
						{
							SupportedPlatforms = new List<UnrealTargetPlatform>();
							SupportedPlatforms.AddRange(MatchingProjectTarget.SupportedPlatforms);
						}
						if (SupportedPlatforms == null || SupportedPlatforms.Contains(SolutionPlatform))
						{
							bBuildByDefault = true;

							PlatformProjectGenerator ProjGen = PlatformProjectGenerators.GetPlatformProjectGenerator(SolutionPlatform, true);
							if (MatchingProjectTarget.ProjectDeploys ||
								((ProjGen != null) && (ProjGen.GetVisualStudioDeploymentEnabled(ProjectPlatform, ProjectConfiguration) == true)))
							{
								bDeployByDefault = true;
							}
						}
					}
				}
			}

			return new MSBuildProjectContext(ProjectConfigurationName, ProjectPlatformName){ bBuildByDefault = bBuildByDefault, bDeployByDefault = bDeployByDefault };
		}

		class ProjectConfigAndTargetCombination
		{
			public UnrealTargetPlatform Platform;
			public UnrealTargetConfiguration Configuration;
			public string ProjectPlatformName;
			public string ProjectConfigurationName;
			public ProjectTarget ProjectTarget;

			public ProjectConfigAndTargetCombination(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, string InProjectPlatformName, string InProjectConfigurationName, ProjectTarget InProjectTarget)
			{
				Platform = InPlatform;
				Configuration = InConfiguration;
				ProjectPlatformName = InProjectPlatformName;
				ProjectConfigurationName = InProjectConfigurationName;
				ProjectTarget = InProjectTarget;
			}

			public string ProjectConfigurationAndPlatformName
			{
				get { return (ProjectPlatformName == null) ? null : (ProjectConfigurationName + "|" + ProjectPlatformName); }
			}

			public override string ToString()
			{
				return String.Format("{0} {1} {2}", ProjectTarget, Platform, Configuration);
			}
		}

		WindowsCompiler GetCompilerForIntellisense()
		{
			switch(ProjectFileFormat)
			{
				case VCProjectFileFormat.VisualStudio2019:
					return WindowsCompiler.VisualStudio2019;
				case VCProjectFileFormat.VisualStudio2017:
					return WindowsCompiler.VisualStudio2017;
				default:
					return WindowsCompiler.VisualStudio2015_DEPRECATED;
			}
		}

		HashSet<string> InvalidConfigPlatformNames;
		List<ProjectConfigAndTargetCombination> ProjectConfigAndTargetCombinations;

		private void BuildProjectConfigAndTargetCombinations(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			//no need to do this more than once
			if(ProjectConfigAndTargetCombinations == null)
			{
				// Build up a list of platforms and configurations this project will support.  In this list, Unknown simply
				// means that we should use the default "stub" project platform and configuration name.

				// If this is a "stub" project, then only add a single configuration to the project
				ProjectConfigAndTargetCombinations = new List<ProjectConfigAndTargetCombination>();
				if (IsStubProject)
				{
					ProjectConfigAndTargetCombination StubCombination = new ProjectConfigAndTargetCombination(UnrealTargetPlatform.Unknown, UnrealTargetConfiguration.Unknown, StubProjectPlatformName, StubProjectConfigurationName, null);
					ProjectConfigAndTargetCombinations.Add(StubCombination);
				}
				else
				{
					// Figure out all the desired configurations
					foreach (UnrealTargetConfiguration Configuration in InConfigurations)
					{
						//@todo.Rocket: Put this in a commonly accessible place?
						if (InstalledPlatformInfo.IsValidConfiguration(Configuration, EProjectType.Code) == false)
						{
							continue;
						}
						foreach (UnrealTargetPlatform Platform in InPlatforms)
						{
							if (InstalledPlatformInfo.IsValidPlatform(Platform, EProjectType.Code) == false)
							{
								continue;
							}
							UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform, true);
							if ((BuildPlatform != null) && (BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid))
							{
								// Now go through all of the target types for this project
								if (ProjectTargets.Count == 0)
								{
									throw new BuildException("Expecting at least one ProjectTarget to be associated with project '{0}' in the TargetProjects list ", ProjectFilePath);
								}

								foreach (ProjectTarget ProjectTarget in ProjectTargets)
								{
									if (IsValidProjectPlatformAndConfiguration(ProjectTarget, Platform, Configuration, PlatformProjectGenerators))
									{
										string ProjectPlatformName, ProjectConfigurationName;
										MakeProjectPlatformAndConfigurationNames(Platform, Configuration, ProjectTarget.TargetRules.Type, PlatformProjectGenerators, out ProjectPlatformName, out ProjectConfigurationName);

										ProjectConfigAndTargetCombination Combination = new ProjectConfigAndTargetCombination(Platform, Configuration, ProjectPlatformName, ProjectConfigurationName, ProjectTarget);
										ProjectConfigAndTargetCombinations.Add(Combination);
									}
								}
							}
						}
					}
				}

				// Create a list of platforms for the "invalid" configuration. We always require at least one of these.
				if(ProjectConfigAndTargetCombinations.Count == 0)
				{
					InvalidConfigPlatformNames = new HashSet<string>{ DefaultPlatformName };
				}
				else
				{
					InvalidConfigPlatformNames = new HashSet<string>(ProjectConfigAndTargetCombinations.Select(x => x.ProjectPlatformName));
				}
			}
		}


		/// <summary>
		/// If found writes a debug project file to disk
		/// </summary>
		/// <returns>True on success</returns>
		public override List<Tuple<ProjectFile, string>> WriteDebugProjectFiles(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			List<UnrealTargetPlatform> ProjectPlatforms = new List<UnrealTargetPlatform>();
			List<Tuple<ProjectFile, string>> ProjectFiles = new List<Tuple<ProjectFile, string>>();

			BuildProjectConfigAndTargetCombinations(InPlatforms, InConfigurations, PlatformProjectGenerators);


			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				if (!ProjectPlatforms.Contains(Combination.Platform))
				{
					ProjectPlatforms.Add(Combination.Platform);
				}
			}

			//write out any additional project files
			if (!IsStubProject && ProjectTargets.Any(x => x.TargetRules != null && x.TargetRules.Type != TargetType.Program))
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null)
					{
						//write out additional prop file
						ProjGenerator.WriteAdditionalPropFile();

						//write out additional project user files
						ProjGenerator.WriteAdditionalProjUserFile(this);

						//write out additional project files
						Tuple<ProjectFile, string> DebugProjectInfo = ProjGenerator.WriteAdditionalProjFile(this);
						if(DebugProjectInfo != null)
						{
							ProjectFiles.Add(DebugProjectInfo);
						}
					}
				}
			}

			return ProjectFiles;
		}

		/// Implements Project interface
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			string ProjectName = ProjectFilePath.GetFileNameWithoutExtension();

			bool bSuccess = true;

			// Build up the new include search path string
			StringBuilder VCIncludeSearchPaths = new StringBuilder();
			{
				foreach (string CurPath in IntelliSenseIncludeSearchPaths)
				{
					VCIncludeSearchPaths.Append(CurPath + ";");
				}
				foreach (string CurPath in IntelliSenseSystemIncludeSearchPaths)
				{
					VCIncludeSearchPaths.Append(CurPath + ";");
				}
				if (InPlatforms.Contains(UnrealTargetPlatform.Win64))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CppPlatform.Win64, GetCompilerForIntellisense(), null) + ";");
				}
				else if (InPlatforms.Contains(UnrealTargetPlatform.Win32))
				{
					VCIncludeSearchPaths.Append(VCToolChain.GetVCIncludePaths(CppPlatform.Win32, GetCompilerForIntellisense(), null) + ";");
				}
			}

			StringBuilder VCPreprocessorDefinitions = new StringBuilder();
			foreach (string CurDef in IntelliSensePreprocessorDefinitions)
			{
				if (VCPreprocessorDefinitions.Length > 0)
				{
					VCPreprocessorDefinitions.Append(';');
				}
				VCPreprocessorDefinitions.Append(CurDef);
			}

			// Setup VC project file content
			StringBuilder VCProjectFileContent = new StringBuilder();
			StringBuilder VCFiltersFileContent = new StringBuilder();
			StringBuilder VCUserFileContent = new StringBuilder();

			// Visual Studio doesn't require a *.vcxproj.filters file to even exist alongside the project unless
			// it actually has something of substance in it.  We'll avoid saving it out unless we need to.
			bool FiltersFileIsNeeded = false;

			// Project file header
			VCProjectFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			VCProjectFileContent.AppendLine("<Project DefaultTargets=\"Build\" ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat));

			bool bGenerateUserFileContent = PlatformProjectGenerators.PlatformRequiresVSUserFileGeneration(InPlatforms, InConfigurations);
			if (bGenerateUserFileContent)
			{
				VCUserFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
				VCUserFileContent.AppendLine("<Project ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat));
			}

			BuildProjectConfigAndTargetCombinations(InPlatforms, InConfigurations, PlatformProjectGenerators);

			VCProjectFileContent.AppendLine("  <ItemGroup Label=\"ProjectConfigurations\">");

			// Make a list of the platforms and configs as project-format names
			List<UnrealTargetPlatform> ProjectPlatforms = new List<UnrealTargetPlatform>();
			List<Tuple<string, UnrealTargetPlatform>> ProjectPlatformNameAndPlatforms = new List<Tuple<string, UnrealTargetPlatform>>();	// ProjectPlatformName, Platform
			List<Tuple<string, UnrealTargetConfiguration>> ProjectConfigurationNameAndConfigurations = new List<Tuple<string, UnrealTargetConfiguration>>();	// ProjectConfigurationName, Configuration
			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				if (!ProjectPlatforms.Contains(Combination.Platform))
				{
					ProjectPlatforms.Add(Combination.Platform);
				}
				if (!ProjectPlatformNameAndPlatforms.Any(ProjectPlatformNameAndPlatformTuple => ProjectPlatformNameAndPlatformTuple.Item1 == Combination.ProjectPlatformName))
				{
					ProjectPlatformNameAndPlatforms.Add(Tuple.Create(Combination.ProjectPlatformName, Combination.Platform));
				}
				if (!ProjectConfigurationNameAndConfigurations.Any(ProjectConfigurationNameAndConfigurationTuple => ProjectConfigurationNameAndConfigurationTuple.Item1 == Combination.ProjectConfigurationName))
				{
					ProjectConfigurationNameAndConfigurations.Add(Tuple.Create(Combination.ProjectConfigurationName, Combination.Configuration));
				}
			}

			// Add the "invalid" configuration for each platform. We use this when the solution configuration does not match any project configuration.
			foreach(string InvalidConfigPlatformName in InvalidConfigPlatformNames)
			{
				VCProjectFileContent.AppendLine("    <ProjectConfiguration Include=\"Invalid|{0}\">", InvalidConfigPlatformName);
				VCProjectFileContent.AppendLine("      <Configuration>Invalid</Configuration>");
				VCProjectFileContent.AppendLine("      <Platform>{0}</Platform>", InvalidConfigPlatformName);
				VCProjectFileContent.AppendLine("    </ProjectConfiguration>");
			}

			// Output ALL the project's config-platform permutations (project files MUST do this)
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					VCProjectFileContent.AppendLine("    <ProjectConfiguration Include=\"{0}|{1}\">", ProjectConfigurationName, ProjectPlatformName);
					VCProjectFileContent.AppendLine("      <Configuration>{0}</Configuration>", ProjectConfigurationName);
					VCProjectFileContent.AppendLine("      <Platform>{0}</Platform>", ProjectPlatformName);
					VCProjectFileContent.AppendLine("    </ProjectConfiguration>");
				}
			}

			VCProjectFileContent.AppendLine("  </ItemGroup>");

			VCFiltersFileContent.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			VCFiltersFileContent.AppendLine("<Project ToolsVersion=\"{0}\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat));

			// Platform specific PropertyGroups, etc.
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						ProjGenerator.GetAdditionalVisualStudioPropertyGroups(Platform, ProjectFileFormat, VCProjectFileContent);
					}
				}
			}

			// Project globals (project GUID, project type, SCC bindings, etc)
			{
				VCProjectFileContent.AppendLine("  <PropertyGroup Label=\"Globals\">");
				VCProjectFileContent.AppendLine("    <ProjectGuid>{0}</ProjectGuid>", ProjectGUID.ToString("B").ToUpperInvariant());
				VCProjectFileContent.AppendLine("    <Keyword>MakeFileProj</Keyword>");
				VCProjectFileContent.AppendLine("    <RootNamespace>{0}</RootNamespace>", ProjectName);
				VCProjectFileContent.AppendLine("    <PlatformToolset>{0}</PlatformToolset>", VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(ProjectFileFormat));
				VCProjectFileContent.AppendLine("    <MinimumVisualStudioVersion>{0}</MinimumVisualStudioVersion>", VCProjectFileGenerator.GetProjectFileToolVersionString(ProjectFileFormat));
				VCProjectFileContent.AppendLine("    <TargetRuntime>Native</TargetRuntime>");
				VCProjectFileContent.AppendLine("  </PropertyGroup>");
			}

			// look for additional import lines for all platforms for non stub projects
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						ProjGenerator.GetVisualStudioGlobalProperties(Platform, VCProjectFileContent);
					}
				}
			}

			if (!IsStubProject)
			{
				// TODO: Restrict this to only the Lumin platform targets, routing via GetVisualStudioGlobalProperties().
				// Currently hacking here because returning true from HasVisualStudioSupport() for lumin causes bunch of faiures in VS.
				VCProjectFileContent.AppendLine("  <ItemGroup>");
				VCProjectFileContent.AppendLine("    <ProjectCapability Include=\"MLProject\" />");
				VCProjectFileContent.AppendLine("    <PropertyPageSchema Include=\"$(LOCALAPPDATA)\\Microsoft\\VisualStudio\\MagicLeap\\debugger.xaml\" />");
				VCProjectFileContent.AppendLine("  </ItemGroup>");
			}

			// Write each project configuration PreDefaultProps section
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				UnrealTargetConfiguration TargetConfiguration = ConfigurationTuple.Item2;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					UnrealTargetPlatform TargetPlatform = PlatformTuple.Item2;
					WritePreDefaultPropsConfiguration(TargetPlatform, TargetConfiguration, ProjectPlatformName, ProjectConfigurationName, PlatformProjectGenerators, VCProjectFileContent);
				}
			}

			VCProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />");

			// Write the invalid configuration data
			foreach(string InvalidConfigPlatformName in InvalidConfigPlatformNames)
			{
				VCProjectFileContent.AppendLine("  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Invalid|{0}'\" Label=\"Configuration\">", InvalidConfigPlatformName);
				VCProjectFileContent.AppendLine("    <ConfigurationType>Makefile</ConfigurationType>");
				VCProjectFileContent.AppendLine("  </PropertyGroup>");
			}

			// Write each project configuration PreDefaultProps section
			foreach (Tuple<string, UnrealTargetConfiguration> ConfigurationTuple in ProjectConfigurationNameAndConfigurations)
			{
				string ProjectConfigurationName = ConfigurationTuple.Item1;
				UnrealTargetConfiguration TargetConfiguration = ConfigurationTuple.Item2;
				foreach (Tuple<string, UnrealTargetPlatform> PlatformTuple in ProjectPlatformNameAndPlatforms)
				{
					string ProjectPlatformName = PlatformTuple.Item1;
					UnrealTargetPlatform TargetPlatform = PlatformTuple.Item2;
					WritePostDefaultPropsConfiguration(TargetPlatform, TargetConfiguration, ProjectPlatformName, ProjectConfigurationName, PlatformProjectGenerators, VCProjectFileContent);
				}
			}

			VCProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />");
			VCProjectFileContent.AppendLine("  <ImportGroup Label=\"ExtensionSettings\" />");
			VCProjectFileContent.AppendLine("  <PropertyGroup Label=\"UserMacros\" />");

			// Write the invalid configuration
			foreach(string InvalidConfigPlatformName in InvalidConfigPlatformNames)
			{
				const string InvalidMessage = "echo The selected platform/configuration is not valid for this target.";

				string ProjectRelativeUnusedDirectory = NormalizeProjectPath(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Unused"));

				VCProjectFileContent.AppendLine("  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Invalid|{0}'\">", InvalidConfigPlatformName);
				VCProjectFileContent.AppendLine("    <NMakeBuildCommandLine>{0}</NMakeBuildCommandLine>", InvalidMessage);
				VCProjectFileContent.AppendLine("    <NMakeReBuildCommandLine>{0}</NMakeReBuildCommandLine>", InvalidMessage);
				VCProjectFileContent.AppendLine("    <NMakeCleanCommandLine>{0}</NMakeCleanCommandLine>", InvalidMessage);
				VCProjectFileContent.AppendLine("    <NMakeOutput>Invalid Output</NMakeOutput>", InvalidMessage);
				VCProjectFileContent.AppendLine("    <OutDir>{0}{1}</OutDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
				VCProjectFileContent.AppendLine("    <IntDir>{0}{1}</IntDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
				VCProjectFileContent.AppendLine("  </PropertyGroup>");
			}

			// Write each project configuration
			foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
			{
				WriteConfiguration(ProjectName, Combination, VCProjectFileContent, PlatformProjectGenerators, bGenerateUserFileContent ? VCUserFileContent : null);
			}

			// Source folders and files
			{
				List<AliasedFile> LocalAliasedFiles = new List<AliasedFile>(AliasedFiles);

				foreach (SourceFile CurFile in SourceFiles)
				{
					// We want all source file and directory paths in the project files to be relative to the project file's
					// location on the disk.  Convert the path to be relative to the project file directory
					string ProjectRelativeSourceFile = CurFile.Reference.MakeRelativeTo(ProjectFilePath.Directory);

					// By default, files will appear relative to the project file in the solution.  This is kind of the normal Visual
					// Studio way to do things, but because our generated project files are emitted to intermediate folders, if we always
					// did this it would yield really ugly paths int he solution explorer
					string FilterRelativeSourceDirectory;
					if (CurFile.BaseFolder == null)
					{
						FilterRelativeSourceDirectory = ProjectRelativeSourceFile;
					}
					else
					{
						FilterRelativeSourceDirectory = CurFile.Reference.MakeRelativeTo(CurFile.BaseFolder);
					}

					// Manually remove the filename for the filter. We run through this code path a lot, so just do it manually.
					int LastSeparatorIdx = FilterRelativeSourceDirectory.LastIndexOf(Path.DirectorySeparatorChar);
					if (LastSeparatorIdx == -1)
					{
						FilterRelativeSourceDirectory = "";
					}
					else
					{
						FilterRelativeSourceDirectory = FilterRelativeSourceDirectory.Substring(0, LastSeparatorIdx);
					}

					LocalAliasedFiles.Add(new AliasedFile(ProjectRelativeSourceFile, FilterRelativeSourceDirectory));
				}

				VCFiltersFileContent.AppendLine("  <ItemGroup>");

				VCProjectFileContent.AppendLine("  <ItemGroup>");

				// Add all file directories to the filters file as solution filters
				HashSet<string> FilterDirectories = new HashSet<string>();
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(BuildHostPlatform.Current.Platform);

				foreach (AliasedFile AliasedFile in LocalAliasedFiles)
				{
					// No need to add the root directory relative to the project (it would just be an empty string!)
					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						FiltersFileIsNeeded = EnsureFilterPathExists(AliasedFile.ProjectPath, VCFiltersFileContent, FilterDirectories);
					}

					string VCFileType = GetVCFileType(AliasedFile.FileSystemPath);
					VCProjectFileContent.AppendLine("    <{0} Include=\"{1}\"/>", VCFileType, EscapeFileName(AliasedFile.FileSystemPath));

					if (!String.IsNullOrWhiteSpace(AliasedFile.ProjectPath))
					{
						VCFiltersFileContent.AppendLine("    <{0} Include=\"{1}\">", VCFileType, EscapeFileName(AliasedFile.FileSystemPath));
						VCFiltersFileContent.AppendLine("      <Filter>{0}</Filter>", Utils.CleanDirectorySeparators(EscapeFileName(AliasedFile.ProjectPath)));
						VCFiltersFileContent.AppendLine("    </{0}>", VCFileType);

						FiltersFileIsNeeded = true;
					}
					else
					{
						// No need to specify the root directory relative to the project (it would just be an empty string!)
						VCFiltersFileContent.AppendLine("    <{0} Include=\"{1}\" />", VCFileType, EscapeFileName(AliasedFile.FileSystemPath));
					}
				}

				VCProjectFileContent.AppendLine("  </ItemGroup>");

				VCFiltersFileContent.AppendLine("  </ItemGroup>");
			}

			// For Installed engine builds, include engine source in the source search paths if it exists. We never build it locally, so the debugger can't find it.
			if (UnrealBuildTool.IsEngineInstalled() && !IsStubProject)
			{
				VCProjectFileContent.AppendLine("  <PropertyGroup>");
				VCProjectFileContent.Append("    <SourcePath>");
				foreach (string DirectoryName in Directory.EnumerateDirectories(UnrealBuildTool.EngineSourceDirectory.FullName, "*", SearchOption.AllDirectories))
				{
					if (Directory.EnumerateFiles(DirectoryName, "*.cpp").Any())
					{
						VCProjectFileContent.Append(DirectoryName);
						VCProjectFileContent.Append(";");
					}
				}
				VCProjectFileContent.AppendLine("</SourcePath>");
				VCProjectFileContent.AppendLine("  </PropertyGroup>");
			}

			// Write IntelliSense info
			{
				// @todo projectfiles: Currently we are storing defines/include paths for ALL configurations rather than using ConditionString and storing
				//      this data uniquely for each target configuration.  IntelliSense may behave better if we did that, but it will result in a LOT more
				//      data being stored into the project file, and might make the IDE perform worse when switching configurations!
				VCProjectFileContent.AppendLine("  <PropertyGroup>");
				VCProjectFileContent.AppendLine("    <NMakePreprocessorDefinitions>$(NMakePreprocessorDefinitions){0}</NMakePreprocessorDefinitions>", (VCPreprocessorDefinitions.Length > 0 ? (";" + VCPreprocessorDefinitions) : ""));
				VCProjectFileContent.AppendLine("    <NMakeIncludeSearchPath>$(NMakeIncludeSearchPath){0}</NMakeIncludeSearchPath>", (VCIncludeSearchPaths.Length > 0 ? (";" + VCIncludeSearchPaths) : ""));
				VCProjectFileContent.AppendLine("    <NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>");
				VCProjectFileContent.AppendLine("    <NMakeAssemblySearchPath>$(NMakeAssemblySearchPath)</NMakeAssemblySearchPath>");
				VCProjectFileContent.AppendLine("    <NMakeForcedUsingAssemblies>$(NMakeForcedUsingAssemblies)</NMakeForcedUsingAssemblies>");
				VCProjectFileContent.AppendLine("  </PropertyGroup>");
			}

			string OutputManifestString = "";
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						// @todo projectfiles: Serious hacks here because we are trying to emit one-time platform-specific sections that need information
						//    about a target type, but the project file may contain many types of targets!  Some of this logic will need to move into
						//    the per-target configuration writing code.
						TargetType HackTargetType = TargetType.Game;
						FileReference HackTargetFilePath = null;
						foreach (ProjectConfigAndTargetCombination Combination in ProjectConfigAndTargetCombinations)
						{
							if (Combination.Platform == Platform &&
								Combination.ProjectTarget.TargetRules != null &&
								Combination.ProjectTarget.TargetRules.Type == HackTargetType)
							{
								HackTargetFilePath = Combination.ProjectTarget.TargetFilePath;// ProjectConfigAndTargetCombinations[0].ProjectTarget.TargetFilePath;
								break;
							}
						}

						if (HackTargetFilePath != null)
						{
							OutputManifestString += ProjGenerator.GetVisualStudioOutputManifestSection(Platform, HackTargetType, HackTargetFilePath, ProjectFilePath, ProjectFileFormat);
						}
					}
				}
			}

			VCProjectFileContent.Append(OutputManifestString); // output manifest must come before the Cpp.targets file.
			VCProjectFileContent.AppendLine("  <ItemDefinitionGroup>");
			VCProjectFileContent.AppendLine("  </ItemDefinitionGroup>");
			VCProjectFileContent.AppendLine("  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />");
			if (!IsStubProject)
			{
				foreach (UnrealTargetPlatform Platform in ProjectPlatforms)
				{
					PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);
					if (ProjGenerator != null && ProjGenerator.HasVisualStudioSupport(Platform, UnrealTargetConfiguration.Development, ProjectFileFormat))
					{
						ProjGenerator.GetVisualStudioTargetOverrides(Platform, ProjectFileFormat, VCProjectFileContent);
					}
				}
			}
			VCProjectFileContent.AppendLine("  <ImportGroup Label=\"ExtensionTargets\">");
			VCProjectFileContent.AppendLine("  </ImportGroup>");
			VCProjectFileContent.AppendLine("</Project>");

			VCFiltersFileContent.AppendLine("</Project>");

			if (bGenerateUserFileContent)
			{
				VCUserFileContent.AppendLine("</Project>");
			}

			// Save the project file
			if (bSuccess)
			{
				bSuccess = ProjectFileGenerator.WriteFileIfChanged(ProjectFilePath.FullName, VCProjectFileContent.ToString());
			}


			// Save the filters file
			if (bSuccess)
			{
				// Create a path to the project file's filters file
				string VCFiltersFilePath = ProjectFilePath.FullName + ".filters";
				if (FiltersFileIsNeeded)
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(VCFiltersFilePath, VCFiltersFileContent.ToString());
				}
				else
				{
					Log.TraceVerbose("Deleting Visual C++ filters file which is no longer needed: " + VCFiltersFilePath);

					// Delete the filters file, if one exists.  We no longer need it
					try
					{
						File.Delete(VCFiltersFilePath);
					}
					catch (Exception)
					{
						Log.TraceInformation("Error deleting filters file (file may not be writable): " + VCFiltersFilePath);
					}
				}
			}

			// Save the user file, if required
			if (VCUserFileContent.Length > 0)
			{
				// Create a path to the project file's user file
				string VCUserFilePath = ProjectFilePath.FullName + ".user";
				// Never overwrite the existing user path as it will cause them to lose their settings
				if (File.Exists(VCUserFilePath) == false)
				{
					bSuccess = ProjectFileGenerator.WriteFileIfChanged(VCUserFilePath, VCUserFileContent.ToString());
				}
			}

			return bSuccess;
		}

		private static bool EnsureFilterPathExists(string FilterRelativeSourceDirectory, StringBuilder VCFiltersFileContent, HashSet<string> FilterDirectories)
		{
			// We only want each directory to appear once in the filters file
			string PathRemaining = Utils.CleanDirectorySeparators(FilterRelativeSourceDirectory);
			bool FiltersFileIsNeeded = false;
			if (!FilterDirectories.Contains(PathRemaining))
			{
				// Make sure all subdirectories leading up to this directory each have their own filter, too!
				List<string> AllDirectoriesInPath = new List<string>();
				string PathSoFar = "";
				for (; ; )
				{
					if (PathRemaining.Length > 0)
					{
						int SlashIndex = PathRemaining.IndexOf(Path.DirectorySeparatorChar);
						string SplitDirectory;
						if (SlashIndex != -1)
						{
							SplitDirectory = PathRemaining.Substring(0, SlashIndex);
							PathRemaining = PathRemaining.Substring(SplitDirectory.Length + 1);
						}
						else
						{
							SplitDirectory = PathRemaining;
							PathRemaining = "";
						}
						if (!String.IsNullOrEmpty(PathSoFar))
						{
							PathSoFar += Path.DirectorySeparatorChar;
						}
						PathSoFar += SplitDirectory;

						AllDirectoriesInPath.Add(PathSoFar);
					}
					else
					{
						break;
					}
				}

				foreach (string LeadingDirectory in AllDirectoriesInPath)
				{
					if (!FilterDirectories.Contains(LeadingDirectory))
					{
						FilterDirectories.Add(LeadingDirectory);

						// Generate a unique GUID for this folder
						// NOTE: When saving generated project files, we ignore differences in GUIDs if every other part of the file
						//       matches identically with the pre-existing file
						string FilterGUID = Guid.NewGuid().ToString("B").ToUpperInvariant();

						VCFiltersFileContent.AppendLine("    <Filter Include=\"{0}\">", EscapeFileName(LeadingDirectory));
						VCFiltersFileContent.AppendLine("      <UniqueIdentifier>{0}</UniqueIdentifier>", FilterGUID);
						VCFiltersFileContent.AppendLine("    </Filter>");

						FiltersFileIsNeeded = true;
					}
				}
			}

			return FiltersFileIsNeeded;
		}

		/// <summary>
		/// Returns the VCFileType element name based on the file path.
		/// </summary>
		/// <param name="Path">The path of the file to return type for.</param>
		/// <returns>Name of the element in MSBuild project file for this file.</returns>
		private string GetVCFileType(string Path)
		{
			// What type of file is this?
			if (Path.EndsWith(".h", StringComparison.InvariantCultureIgnoreCase) ||
				Path.EndsWith(".inl", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClInclude";
			}
			else if (Path.EndsWith(".cpp", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ClCompile";
			}
			else if (Path.EndsWith(".rc", StringComparison.InvariantCultureIgnoreCase))
			{
				return "ResourceCompile";
			}
			else if (Path.EndsWith(".manifest", StringComparison.InvariantCultureIgnoreCase))
			{
				return "Manifest";
			}
			else
			{
				return "None";
			}
		}

		// Anonymous function that writes pre-Default.props configuration data
		private void WritePreDefaultPropsConfiguration(UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration TargetConfiguration, string ProjectPlatformName, string ProjectConfigurationName, PlatformProjectGeneratorCollection PlatformProjectGenerators, StringBuilder VCProjectFileContent)
		{
			PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(TargetPlatform, true);
			if (((ProjGenerator == null) && (TargetPlatform != UnrealTargetPlatform.Unknown)))
			{
				return;
			}

			string ProjectConfigurationAndPlatformName = ProjectConfigurationName + "|" + ProjectPlatformName;
			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + ProjectConfigurationAndPlatformName + "'\"";

			if(ProjGenerator != null)
			{
				StringBuilder PlatformToolsetString = new StringBuilder();
				ProjGenerator.GetVisualStudioPreDefaultString(TargetPlatform, TargetConfiguration, PlatformToolsetString);

				if (PlatformToolsetString.Length > 0)
				{
					VCProjectFileContent.AppendLine("  <PropertyGroup " + ConditionString + " Label=\"Configuration\">", ConditionString);
					VCProjectFileContent.Append(PlatformToolsetString);
					VCProjectFileContent.AppendLine("  </PropertyGroup>");
				}
			}
		}

		// Anonymous function that writes post-Default.props configuration data
		private void WritePostDefaultPropsConfiguration(UnrealTargetPlatform TargetPlatform, UnrealTargetConfiguration TargetConfiguration, string ProjectPlatformName, string ProjectConfigurationName, PlatformProjectGeneratorCollection PlatformProjectGenerators, StringBuilder VCProjectFileContent)
		{
			PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(TargetPlatform, true);

			string ProjectConfigurationAndPlatformName = ProjectConfigurationName + "|" + ProjectPlatformName;
			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + ProjectConfigurationAndPlatformName + "'\"";

			StringBuilder PlatformToolsetString = new StringBuilder();
			if(ProjGenerator != null)
			{
				ProjGenerator.GetVisualStudioPlatformToolsetString(TargetPlatform, TargetConfiguration, ProjectFileFormat, PlatformToolsetString);
			}
			if (PlatformToolsetString.Length == 0)
			{
				PlatformToolsetString.AppendLine("    <PlatformToolset>" + VCProjectFileGenerator.GetProjectFilePlatformToolsetVersionString(ProjectFileFormat) + "</PlatformToolset>");
			}

			string PlatformConfigurationType = (ProjGenerator == null) ? "Makefile" : ProjGenerator.GetVisualStudioPlatformConfigurationType(TargetPlatform, ProjectFileFormat);
			VCProjectFileContent.AppendLine("  <PropertyGroup {0} Label=\"Configuration\">", ConditionString);
			VCProjectFileContent.AppendLine("    <ConfigurationType>{0}</ConfigurationType>", PlatformConfigurationType);
			VCProjectFileContent.Append(PlatformToolsetString);
			VCProjectFileContent.AppendLine("  </PropertyGroup>");
		}

		// Anonymous function that writes project configuration data
		private void WriteConfiguration(string ProjectName, ProjectConfigAndTargetCombination Combination, StringBuilder VCProjectFileContent, PlatformProjectGeneratorCollection PlatformProjectGenerators, StringBuilder VCUserFileContent)
		{
			UnrealTargetPlatform Platform = Combination.Platform;
			UnrealTargetConfiguration Configuration = Combination.Configuration;

			PlatformProjectGenerator ProjGenerator = PlatformProjectGenerators.GetPlatformProjectGenerator(Platform, true);

			string UProjectPath = "";
			if (IsForeignProject)
			{
				UProjectPath = "\"$(SolutionDir)$(ProjectName).uproject\"";
			}

			string ConditionString = "Condition=\"'$(Configuration)|$(Platform)'=='" + Combination.ProjectConfigurationAndPlatformName + "'\"";

			{
				VCProjectFileContent.AppendLine("  <ImportGroup {0} Label=\"PropertySheets\">", ConditionString);
				VCProjectFileContent.AppendLine("    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />");
				if(ProjGenerator != null)
				{
					ProjGenerator.GetVisualStudioImportGroupProperties(Platform, VCProjectFileContent);
				}
				VCProjectFileContent.AppendLine("  </ImportGroup>");

				DirectoryReference ProjectDirectory = ProjectFilePath.Directory;

				if (IsStubProject)
				{
					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Unused"));

					VCProjectFileContent.AppendLine("  <PropertyGroup {0}>", ConditionString);
					VCProjectFileContent.AppendLine("    <OutDir>{0}{1}</OutDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
					VCProjectFileContent.AppendLine("    <IntDir>{0}{1}</IntDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
					VCProjectFileContent.AppendLine("    <NMakeBuildCommandLine>@rem Nothing to do.</NMakeBuildCommandLine>");
					VCProjectFileContent.AppendLine("    <NMakeReBuildCommandLine>@rem Nothing to do.</NMakeReBuildCommandLine>");
					VCProjectFileContent.AppendLine("    <NMakeCleanCommandLine>@rem Nothing to do.</NMakeCleanCommandLine>");
					VCProjectFileContent.AppendLine("    <NMakeOutput/>");
					VCProjectFileContent.AppendLine("  </PropertyGroup>");
				}
				else if (UnrealBuildTool.IsEngineInstalled() && Combination.ProjectTarget != null && Combination.ProjectTarget.TargetRules != null && !Combination.ProjectTarget.SupportedPlatforms.Contains(Combination.Platform))
				{
					string ProjectRelativeUnusedDirectory = NormalizeProjectPath(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Unused"));

					string TargetName = Combination.ProjectTarget.TargetFilePath.GetFileNameWithoutAnyExtensions();
					string ValidPlatforms = String.Join(", ", Combination.ProjectTarget.SupportedPlatforms.Select(x => x.ToString()));

					VCProjectFileContent.AppendLine("  <PropertyGroup {0}>", ConditionString);
					VCProjectFileContent.AppendLine("    <OutDir>{0}{1}</OutDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
					VCProjectFileContent.AppendLine("    <IntDir>{0}{1}</IntDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
					VCProjectFileContent.AppendLine("    <NMakeBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeBuildCommandLine>", Combination.Platform, TargetName, ValidPlatforms);
					VCProjectFileContent.AppendLine("    <NMakeReBuildCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeReBuildCommandLine>", Combination.Platform, TargetName, ValidPlatforms);
					VCProjectFileContent.AppendLine("    <NMakeCleanCommandLine>@echo {0} is not a supported platform for {1}. Valid platforms are {2}.</NMakeCleanCommandLine>", Combination.Platform, TargetName, ValidPlatforms);
					VCProjectFileContent.AppendLine("    <NMakeOutput/>");
					VCProjectFileContent.AppendLine("  </PropertyGroup>");
				}
				else
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;
					FileReference TargetFilePath = Combination.ProjectTarget.TargetFilePath;
					string TargetName = TargetFilePath.GetFileNameWithoutAnyExtensions();
					string UBTPlatformName = Platform.ToString();
					string UBTConfigurationName = Configuration.ToString();

					// Setup output path
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

					// Figure out if this is a monolithic build
					bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);
					if(!bShouldCompileMonolithic)
					{
						bShouldCompileMonolithic = (Combination.ProjectTarget.CreateRulesDelegate(Platform, Configuration).LinkType == TargetLinkType.Monolithic);
					}

					// Get the output directory
					DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;
					if (TargetRulesObject.Type != TargetType.Program && (bShouldCompileMonolithic || TargetRulesObject.BuildEnvironment == TargetBuildEnvironment.Unique))
					{
						if(Combination.ProjectTarget.UnrealProjectFilePath != null)
						{
							RootDirectory = Combination.ProjectTarget.UnrealProjectFilePath.Directory;
						}
					}

					if (TargetRulesObject.Type == TargetType.Program && Combination.ProjectTarget.UnrealProjectFilePath != null)
					{
						RootDirectory = Combination.ProjectTarget.UnrealProjectFilePath.Directory;
					}

					// Get the output directory
					DirectoryReference OutputDirectory = DirectoryReference.Combine(RootDirectory, "Binaries", UBTPlatformName);

					if (!string.IsNullOrEmpty(TargetRulesObject.ExeBinariesSubFolder))
					{
						OutputDirectory = DirectoryReference.Combine(OutputDirectory, TargetRulesObject.ExeBinariesSubFolder);
					}

					// Get the executable name (minus any platform or config suffixes)
					string BaseExeName = TargetName;
					if (!bShouldCompileMonolithic && TargetRulesObject.Type != TargetType.Program && TargetRulesObject.BuildEnvironment != TargetBuildEnvironment.Unique)
					{
						BaseExeName = "UE4" + TargetRulesObject.Type.ToString();
					}

					// Make the output file path
					FileReference NMakePath = FileReference.Combine(OutputDirectory, BaseExeName);
					if (Configuration != TargetRulesObject.UndecoratedConfiguration)
					{
						NMakePath += "-" + UBTPlatformName + "-" + UBTConfigurationName;
					}
					NMakePath += TargetRulesObject.Architecture;
					NMakePath += BuildPlatform.GetBinaryExtension(UEBuildBinaryType.Executable);

					VCProjectFileContent.AppendLine("  <PropertyGroup {0}>", ConditionString);

					StringBuilder PathsStringBuilder = new StringBuilder();
					if(ProjGenerator != null)
					{
						ProjGenerator.GetVisualStudioPathsEntries(Platform, Configuration, TargetRulesObject.Type, TargetFilePath, ProjectFilePath, NMakePath, ProjectFileFormat, PathsStringBuilder);
					}

					string PathStrings = PathsStringBuilder.ToString();
					if (string.IsNullOrEmpty(PathStrings) || (PathStrings.Contains("<IntDir>") == false))
					{
						string ProjectRelativeUnusedDirectory = "$(ProjectDir)..\\Build\\Unused";
						VCProjectFileContent.Append(PathStrings);

						VCProjectFileContent.AppendLine("    <OutDir>{0}{1}</OutDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
						VCProjectFileContent.AppendLine("    <IntDir>{0}{1}</IntDir>", ProjectRelativeUnusedDirectory, Path.DirectorySeparatorChar);
					}
					else
					{
						VCProjectFileContent.Append(PathStrings);
					}

					// This is the standard UE4 based project NMake build line:
					//	..\..\Build\BatchFiles\Build.bat <TARGETNAME> <PLATFORM> <CONFIGURATION>
					//	ie ..\..\Build\BatchFiles\Build.bat BlankProgram Win64 Debug

					StringBuilder BuildArguments = new StringBuilder();

					BuildArguments.AppendFormat("{0} {1} {2}", TargetName, UBTPlatformName, UBTConfigurationName);
					if (IsForeignProject)
					{
						BuildArguments.AppendFormat(" -Project={0}", UProjectPath);
					}

					if (bUsePrecompiled)
					{
						BuildArguments.Append(" -UsePrecompiled");
					}
					else if(TargetRulesObject.Type == TargetType.Editor && bEditorDependsOnShaderCompileWorker && !UnrealBuildTool.IsEngineInstalled())
					{
						BuildArguments.Replace("\"", "\\\"");
						BuildArguments.Insert(0, "-Target=\"ShaderCompileWorker Win64 Development\" -Target=\"");
						BuildArguments.Append("\"");
					}

					// Always wait for the mutex between UBT invocations, so that building the whole solution doesn't fail.
					BuildArguments.Append(" -WaitMutex");

					// Always include a flag to format log messages for MSBuild
					BuildArguments.Append(" -FromMsBuild");

					if (bUseFastPDB)
					{
						// Pass Fast PDB option to make use of Visual Studio's /DEBUG:FASTLINK option
						BuildArguments.Append(" -FastPDB");
					}

					DirectoryReference BatchFilesDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "BatchFiles");

					if(BuildToolOverride != null)
					{
						BuildArguments.AppendFormat(" {0}", BuildToolOverride);
					}

					// NMake Build command line
					VCProjectFileContent.AppendLine("    <NMakeBuildCommandLine>{0} {1}</NMakeBuildCommandLine>", EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Build.bat"))), BuildArguments.ToString());
					VCProjectFileContent.AppendLine("    <NMakeReBuildCommandLine>{0} {1}</NMakeReBuildCommandLine>", EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Rebuild.bat"))), BuildArguments.ToString());
					VCProjectFileContent.AppendLine("    <NMakeCleanCommandLine>{0} {1}</NMakeCleanCommandLine>", EscapePath(NormalizeProjectPath(FileReference.Combine(BatchFilesDirectory, "Clean.bat"))), BuildArguments.ToString());
					VCProjectFileContent.AppendLine("    <NMakeOutput>{0}</NMakeOutput>", NormalizeProjectPath(NMakePath.FullName));

					if (TargetRulesObject.Type == TargetType.Game || TargetRulesObject.Type == TargetType.Client || TargetRulesObject.Type == TargetType.Server)
					{
						// Allow platforms to add any special properties they require... like aumid override for Xbox One
						PlatformProjectGenerators.GenerateGamePlatformSpecificProperties(Platform, Configuration, TargetRulesObject.Type, VCProjectFileContent, RootDirectory, TargetFilePath);
					}

					VCProjectFileContent.AppendLine("  </PropertyGroup>");

					if(ProjGenerator != null)
					{
						VCProjectFileContent.Append(ProjGenerator.GetVisualStudioLayoutDirSection(Platform, Configuration, ConditionString, Combination.ProjectTarget.TargetRules.Type, Combination.ProjectTarget.TargetFilePath, ProjectFilePath, NMakePath, ProjectFileFormat));
					}
				}

				if (VCUserFileContent != null && Combination.ProjectTarget != null)
				{
					TargetRules TargetRulesObject = Combination.ProjectTarget.TargetRules;

					if ((Platform == UnrealTargetPlatform.Win32) || (Platform == UnrealTargetPlatform.Win64))
					{
						VCUserFileContent.AppendLine("  <PropertyGroup {0}>", ConditionString);
						if (TargetRulesObject.Type != TargetType.Game)
						{
							string DebugOptions = "";

							if (IsForeignProject)
							{
								DebugOptions += UProjectPath;
								DebugOptions += " -skipcompile";
							}
							else if (TargetRulesObject.Type == TargetType.Editor && ProjectName != "UE4")
							{
								DebugOptions += ProjectName;
							}

							VCUserFileContent.AppendLine("    <LocalDebuggerCommandArguments>{0}</LocalDebuggerCommandArguments>", DebugOptions);
						}
						VCUserFileContent.AppendLine("    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>");
						VCUserFileContent.AppendLine("  </PropertyGroup>");
					}

					if(ProjGenerator != null)
					{
						VCUserFileContent.Append(ProjGenerator.GetVisualStudioUserFileStrings(Platform, Configuration, ConditionString, TargetRulesObject, Combination.ProjectTarget.TargetFilePath, ProjectFilePath));
					}
				}
			}
		}
	}


	/// <summary>
	/// A Visual C# project.
	/// </summary>
	class VCSharpProjectFile : MSBuildProjectFile
	{
		/// <summary>
		/// This is the GUID that Visual Studio uses to identify a C# project file in the solution
		/// </summary>
		public override string ProjectTypeGUID
		{
			get { return "{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}"; }
		}

		/// <summary>
		/// Platforms that this project supports
		/// </summary>
		public HashSet<string> Platforms = new HashSet<string>();

		/// <summary>
		/// Configurations that this project supports
		/// </summary>
		public HashSet<string> Configurations = new HashSet<string>();

		/// <summary>
		/// Constructs a new project file object
		/// </summary>
		/// <param name="InitFilePath">The path to the project file on disk</param>
		public VCSharpProjectFile(FileReference InitFilePath)
			: base(InitFilePath)
		{
			try
			{
				XmlDocument Document = new XmlDocument();
				Document.Load(InitFilePath.FullName);

				// Check the root element is the right type
				if (Document.DocumentElement.Name != "Project")
				{
					throw new BuildException("Unexpected root element '{0}' in project file", Document.DocumentElement.Name);
				}

				// Parse all the configurations and platforms
				// Parse the basic structure of the document, updating properties and recursing into other referenced projects as we go
				foreach (XmlElement Element in Document.DocumentElement.ChildNodes.OfType<XmlElement>())
				{
					if(Element.Name == "PropertyGroup")
					{
						string Condition = Element.GetAttribute("Condition");
						if(!String.IsNullOrEmpty(Condition))
						{
							Match Match = Regex.Match(Condition, "^\\s*'\\$\\(Configuration\\)\\|\\$\\(Platform\\)'\\s*==\\s*'(.+)\\|(.+)'\\s*$");
							if(Match.Success && Match.Groups.Count == 3)
							{
								Configurations.Add(Match.Groups[1].Value);
								Platforms.Add(Match.Groups[2].Value);
							}
							else
							{
								Log.TraceWarning("Unable to parse configuration/platform from condition '{0}': {1}", InitFilePath, Condition);
							}
						}
					}
				}
			}
			catch(Exception Ex)
			{
				Log.TraceWarning("Unable to parse {0}: {1}", InitFilePath, Ex.ToString());
			}
		}

		/// <summary>
		/// Extract information from the csproj file based on the supplied configuration
		/// </summary>
		public CsProjectInfo GetProjectInfo(UnrealTargetConfiguration InConfiguration)
		{
			if (CachedProjectInfo.ContainsKey(InConfiguration))
			{
				return CachedProjectInfo[InConfiguration];
			}

			CsProjectInfo Info;

			Dictionary<string, string> Properties = new Dictionary<string, string>();
			Properties.Add("Platform", "AnyCPU");
			Properties.Add("Configuration", InConfiguration.ToString());
			if (CsProjectInfo.TryRead(ProjectFilePath, Properties, out Info))
			{
				CachedProjectInfo.Add(InConfiguration, Info);
			}

			return Info;
		}

		/// <summary>
		/// Determine if this project is a .NET Core project
		/// </summary>
		public bool IsDotNETCoreProject()
		{
			CsProjectInfo Info = GetProjectInfo(UnrealTargetConfiguration.Debug);
			return Info.IsDotNETCoreProject();
		}

		/// <summary>
		/// Get the project context for the given solution context
		/// </summary>
		/// <param name="SolutionTarget">The solution target type</param>
		/// <param name="SolutionConfiguration">The solution configuration</param>
		/// <param name="SolutionPlatform">The solution platform</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generators</param>
		/// <returns>Project context matching the given solution context</returns>
		public override MSBuildProjectContext GetMatchingProjectContext(TargetType SolutionTarget, UnrealTargetConfiguration SolutionConfiguration, UnrealTargetPlatform SolutionPlatform, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			// Find the matching platform name
			string ProjectPlatformName;
			if(SolutionPlatform == UnrealTargetPlatform.Win32 && Platforms.Contains("x86"))
			{
				ProjectPlatformName = "x86";
			}
			else if(Platforms.Contains("x64"))
			{
				ProjectPlatformName = "x64";
			}
			else
			{
				ProjectPlatformName = "Any CPU";
			}

			// Find the matching configuration
			string ProjectConfigurationName;
			if(Configurations.Contains(SolutionConfiguration.ToString()))
			{
				ProjectConfigurationName = SolutionConfiguration.ToString();
			}
			else if(Configurations.Contains("Development"))
			{
				ProjectConfigurationName = "Development";
			}
			else
			{
				ProjectConfigurationName = "Release";
			}

			// Figure out whether to build it by default
			bool bBuildByDefault = ShouldBuildByDefaultForSolutionTargets;
			if(SolutionTarget == TargetType.Game || SolutionTarget == TargetType.Editor)
			{
				bBuildByDefault = true;
			}

			// Create the context
			return new MSBuildProjectContext(ProjectConfigurationName, ProjectPlatformName){ bBuildByDefault = bBuildByDefault };
		}

		/// <summary>
		/// Basic csproj file support. Generates C# library project with one build config.
		/// </summary>
		/// <param name="InPlatforms">Not used.</param>
		/// <param name="InConfigurations">Not Used.</param>
		/// <param name="PlatformProjectGenerators">Set of platform project generators</param>
		/// <returns>true if the opration was successful, false otherwise</returns>
		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations, PlatformProjectGeneratorCollection PlatformProjectGenerators)
		{
			throw new BuildException("Support for writing C# projects from UnrealBuildTool has been removed.");
		}

		/// Cache of parsed info about this project
		protected readonly Dictionary<UnrealTargetConfiguration, CsProjectInfo> CachedProjectInfo = new Dictionary<UnrealTargetConfiguration, CsProjectInfo>();
	}

}
