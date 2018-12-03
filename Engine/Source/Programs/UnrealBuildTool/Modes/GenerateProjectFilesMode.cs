// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Generates project files for one or more projects
	/// </summary>
	[ToolMode("GenerateProjectFiles")]
	class GenerateProjectFilesMode : ToolMode
	{
		/// <summary>
		/// Types of project files to generate
		/// </summary>
		[CommandLine("-ProjectFileFormat")]
		[CommandLine("-2012unsupported", Value = nameof(ProjectFileFormat.VisualStudio2012))]
		[CommandLine("-2013unsupported", Value = nameof(ProjectFileFormat.VisualStudio2013))]
		[CommandLine("-2015", Value = nameof(ProjectFileFormat.VisualStudio2015))] // + override compiler
		[CommandLine("-2017", Value = nameof(ProjectFileFormat.VisualStudio2017))] // + override compiler
		[CommandLine("-Makefile", Value = nameof(ProjectFileFormat.Make))]
		[CommandLine("-CMakefile", Value = nameof(ProjectFileFormat.CMake))]
		[CommandLine("-QMakefile", Value = nameof(ProjectFileFormat.QMake))]
		[CommandLine("-KDevelopfile", Value = nameof(ProjectFileFormat.KDevelop))]
		[CommandLine("-CodeLiteFiles", Value = nameof(ProjectFileFormat.CodeLite))]
		[CommandLine("-XCodeProjectFiles", Value = nameof(ProjectFileFormat.XCode))]
		[CommandLine("-EddieProjectFiles", Value = nameof(ProjectFileFormat.Eddie))]
		[CommandLine("-VSCode", Value = nameof(ProjectFileFormat.VisualStudioCode))]
		[CommandLine("-VSMac", Value = nameof(ProjectFileFormat.VisualStudioMac))]
		[CommandLine("-CLion", Value = nameof(ProjectFileFormat.CLion))]
		HashSet<ProjectFileFormat> ProjectFileFormats = new HashSet<ProjectFileFormat>();

		/// <summary>
		/// Execute the tool mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			using(SingleInstanceMutex.Acquire(SingleInstanceMutexType.PerBranch, Arguments))
			{
				// Apply any command line arguments to this class
				Arguments.ApplyTo(this);

				// Parse rocket-specific arguments.
				FileReference ProjectFile;
				UnrealBuildTool.TryParseProjectFileArgument(Arguments, out ProjectFile);

				// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
				// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
				DirectoryReference.SetCurrentDirectory(UnrealBuildTool.EngineSourceDirectory);

				// Read the XML configuration files
				XmlConfig.ReadConfigFiles(null);

				// Find and register all tool chains, build platforms, etc. that are present
				UnrealBuildTool.RegisterAllUBTClasses(false);

				// If there aren't any formats set, read the default project file format from the config file
				if (ProjectFileFormats.Count == 0)
				{
					// Read from the XML config
					if (!String.IsNullOrEmpty(ProjectFileGeneratorSettings.Format))
					{
						ProjectFileFormats.UnionWith(ProjectFileGeneratorSettings.ParseFormatList(ProjectFileGeneratorSettings.Format));
					}

					// Read from the editor config
					ProjectFileFormat PreferredSourceCodeAccessor;
					if (ProjectFileGenerator.GetPreferredSourceCodeAccessor(ProjectFile, out PreferredSourceCodeAccessor))
					{
						ProjectFileFormats.Add(PreferredSourceCodeAccessor);
					}

					// If there's still nothing set, get the default project file format for this platform
					if (ProjectFileFormats.Count == 0)
					{
						ProjectFileFormats.UnionWith(BuildHostPlatform.Current.GetDefaultProjectFileFormats());
					}
				}

				// Register all the platform project generators
				PlatformProjectGeneratorCollection PlatformProjectGenerators = new PlatformProjectGeneratorCollection();
				foreach (Type CheckType in Assembly.GetExecutingAssembly().GetTypes())
				{
					if (CheckType.IsClass && !CheckType.IsAbstract && CheckType.IsSubclassOf(typeof(PlatformProjectGenerator)))
					{
						PlatformProjectGenerator Generator = (PlatformProjectGenerator)Activator.CreateInstance(CheckType, Arguments);
						foreach(UnrealTargetPlatform Platform in Generator.GetPlatforms())
						{
							Log.TraceVerbose("Registering project generator {0} for {1}", CheckType, Platform);
							PlatformProjectGenerators.RegisterPlatformProjectGenerator(Platform, Generator);
						}
					}
				}

				// Create each project generator and run it
				List<ProjectFileGenerator> Generators = new List<ProjectFileGenerator>();
				foreach (ProjectFileFormat ProjectFileFormat in ProjectFileFormats.Distinct())
				{
					ProjectFileGenerator Generator;
					switch (ProjectFileFormat)
					{
						case ProjectFileFormat.Make:
							Generator = new MakefileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.CMake:
							Generator = new CMakefileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.QMake:
							Generator = new QMakefileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.KDevelop:
							Generator = new KDevelopGenerator(ProjectFile);
							break;
						case ProjectFileFormat.CodeLite:
							Generator = new CodeLiteGenerator(ProjectFile, Arguments);
							break;
						case ProjectFileFormat.VisualStudio:
							Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.Default, Arguments);
							break;
						case ProjectFileFormat.VisualStudio2012:
							Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2012, Arguments);
							break;
						case ProjectFileFormat.VisualStudio2013:
							Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2013, Arguments);
							break;
						case ProjectFileFormat.VisualStudio2015:
							Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2015, Arguments);
							break;
						case ProjectFileFormat.VisualStudio2017:
							Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2017, Arguments);
							break;
						case ProjectFileFormat.XCode:
							Generator = new XcodeProjectFileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.Eddie:
							Generator = new EddieProjectFileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.VisualStudioCode:
							Generator = new VSCodeProjectFileGenerator(ProjectFile);
							break;
						case ProjectFileFormat.CLion:
							Generator = new CLionGenerator(ProjectFile);
							break;
						case ProjectFileFormat.VisualStudioMac:
							Generator = new VCMacProjectFileGenerator(ProjectFile, Arguments);
							break;
						default:
							throw new BuildException("Unhandled project file type '{0}", ProjectFileFormat);
					}
					Generators.Add(Generator);
				}

				// Check there are no superfluous command line arguments
				Arguments.CheckAllArgumentsUsed();

				// Now generate project files
				ProjectFileGenerator.bGenerateProjectFiles = true;
				foreach(ProjectFileGenerator Generator in Generators)
				{
					if (!Generator.GenerateProjectFiles(PlatformProjectGenerators, Arguments.GetRawArray()))
					{
						return (int)ECompilationResult.OtherCompilationError;
					}
				}
			}
			return (int)ECompilationResult.Succeeded;
		}
	}
}
