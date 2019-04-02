// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	[ToolMode("GenerateProjectFiles", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance)]
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
		[CommandLine("-2019", Value = nameof(ProjectFileFormat.VisualStudio2019))] // + override compiler
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
		/// Platforms to disable native project file generators for. Platforms with native project file generators typically require IDE extensions to be installed.
		/// </summary>
		[XmlConfigFile(Category = "ProjectFileGenerator")]
		string[] DisablePlatformProjectGenerators = null;

		/// <summary>
		/// Execute the tool mode
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			// Apply any command line arguments to this class
			Arguments.ApplyTo(this);

			// Apply the XML config to this class
			XmlConfig.ApplyTo(this);

			// Parse rocket-specific arguments.
			FileReference ProjectFile;
			TryParseProjectFileArgument(Arguments, out ProjectFile);

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
						if(DisablePlatformProjectGenerators == null || !DisablePlatformProjectGenerators.Any(x => x.Equals(Platform.ToString(), StringComparison.OrdinalIgnoreCase)))
						{
							Log.TraceVerbose("Registering project generator {0} for {1}", CheckType, Platform);
							PlatformProjectGenerators.RegisterPlatformProjectGenerator(Platform, Generator);
						}
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
					case ProjectFileFormat.VisualStudio2019:
						Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2019, Arguments);
						break;
					case ProjectFileFormat.XCode:
						Generator = new XcodeProjectFileGenerator(ProjectFile, Arguments);
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
			// TODO (still pass raw arguments below)
			// Arguments.CheckAllArgumentsUsed();

			// Now generate project files
			ProjectFileGenerator.bGenerateProjectFiles = true;
			foreach(ProjectFileGenerator Generator in Generators)
			{
				if (!Generator.GenerateProjectFiles(PlatformProjectGenerators, Arguments.GetRawArray()))
				{
					return (int)CompilationResult.OtherCompilationError;
				}
			}
			return (int)CompilationResult.Succeeded;
		}

		/// <summary>
		/// Try to parse the project file from the command line
		/// </summary>
		/// <param name="Arguments">The command line arguments</param>
		/// <param name="ProjectFile">The project file that was parsed</param>
		/// <returns>True if the project file was parsed, false otherwise</returns>
		private static bool TryParseProjectFileArgument(CommandLineArguments Arguments, out FileReference ProjectFile)
		{
			FileReference ExplicitProjectFile;
			if(Arguments.TryGetValue("-Project=", out ExplicitProjectFile))
			{
				ProjectFile = ExplicitProjectFile;
				return true;
			}

			for(int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				if(Arguments[Idx][0] != '-' && Arguments[Idx].EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
				{
					Arguments.MarkAsUsed(Idx);
					ProjectFile = new FileReference(Arguments[Idx]);
					return true;
				}
			}

			FileReference InstalledProjectFile = UnrealBuildTool.GetInstalledProjectFile();
			if(InstalledProjectFile != null)
			{
				ProjectFile = InstalledProjectFile;
				return true;
			}

			ProjectFile = null;
			return false;
		}
	}
}
