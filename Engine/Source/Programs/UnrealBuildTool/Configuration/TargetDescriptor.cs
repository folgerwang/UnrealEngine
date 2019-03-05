// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Describes all of the information needed to initialize a UEBuildTarget object
	/// </summary>
	class TargetDescriptor
	{
		public FileReference ProjectFile;
		public string Name;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string Architecture;
		public CommandLineArguments AdditionalArguments;

		/// <summary>
		/// Foreign plugin to compile against this target
		/// </summary>
		[CommandLine("-Plugin=")]
		public FileReference ForeignPlugin = null;

		/// <summary>
		/// Set of module names to compile.
		/// </summary>
		[CommandLine("-Module=")]
		public HashSet<string> OnlyModuleNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Single file to compile
		/// </summary>
		[CommandLine("-SingleFile=")]
		public FileReference SingleFileToCompile = null;

		/// <summary>
		/// Whether to perform hot reload for this target
		/// </summary>
		[CommandLine("-NoHotReload", Value = nameof(HotReloadMode.Disabled))]
		[CommandLine("-ForceHotReload", Value = nameof(HotReloadMode.FromIDE))]
		[CommandLine("-LiveCoding", Value = nameof(HotReloadMode.LiveCoding))]
		public HotReloadMode HotReloadMode = HotReloadMode.Default;

		/// <summary>
		/// Map of module name to suffix for hot reloading from the editor
		/// </summary>
		public Dictionary<string, int> HotReloadModuleNameToSuffix = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Path to the manifest for passing info about the output to live coding
		/// </summary>
		[CommandLine("-LiveCodingManifest=")]
		public FileReference LiveCodingManifest = null;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="TargetName">Name of the target to build</param>
		/// <param name="Platform">Platform to build for</param>
		/// <param name="Configuration">Configuration to build</param>
		/// <param name="Architecture">Architecture to build for</param>
		/// <param name="Arguments">Other command-line arguments for the target</param>
		public TargetDescriptor(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, CommandLineArguments Arguments)
		{
			this.ProjectFile = ProjectFile;
			this.Name = TargetName;
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;

			// If there are any additional command line arguments
			List<string> AdditionalArguments = new List<string>();
			if(Arguments != null)
			{
				// Apply the arguments to this object
				Arguments.ApplyTo(this);

				// Parse all the hot-reload module names
				foreach(string ModuleWithSuffix in Arguments.GetValues("-ModuleWithSuffix="))
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

				// Pull out all the arguments that haven't been used so far
				for(int Idx = 0; Idx < Arguments.Count; Idx++)
				{
					if(!Arguments.HasBeenUsed(Idx))
					{
						AdditionalArguments.Add(Arguments[Idx]);
					}
				}
			}
			this.AdditionalArguments = new CommandLineArguments(AdditionalArguments.ToArray());
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <returns>List of target descriptors</returns>
		public static List<TargetDescriptor> ParseCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile)
		{
			List<TargetDescriptor> TargetDescriptors = new List<TargetDescriptor>();
			ParseCommandLine(Arguments, bUsePrecompiled, bSkipRulesCompile, TargetDescriptors);
			return TargetDescriptors;
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <param name="TargetDescriptors">Receives the list of parsed target descriptors</param>
		public static void ParseCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile, List<TargetDescriptor> TargetDescriptors)
		{
			List<string> TargetLists;
			Arguments = Arguments.Remove("-TargetList=", out TargetLists);

			List<string> Targets;
			Arguments = Arguments.Remove("-Target=", out Targets);

			if(TargetLists.Count > 0 || Targets.Count > 0)
			{
				// Try to parse multiple arguments from a single command line
				foreach(string TargetList in TargetLists)
				{
					string[] Lines = File.ReadAllLines(TargetList);
					foreach(string Line in Lines)
					{
						string TrimLine = Line.Trim();
						if(TrimLine.Length > 0 && TrimLine[0] != ';')
						{
							CommandLineArguments NewArguments = Arguments.Append(CommandLineArguments.Split(TrimLine));
							ParseCommandLine(NewArguments, bUsePrecompiled, bSkipRulesCompile, TargetDescriptors);
						}
					}
				}

				foreach(string Target in Targets)
				{
					CommandLineArguments NewArguments = Arguments.Append(CommandLineArguments.Split(Target));
					ParseCommandLine(NewArguments, bUsePrecompiled, bSkipRulesCompile, TargetDescriptors);
				}
			}
			else
			{
				// Otherwise just process the whole command line together
				ParseSingleCommandLine(Arguments, bUsePrecompiled, bSkipRulesCompile, TargetDescriptors);
			}
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <param name="TargetDescriptors">List of target descriptors</param>
		public static void ParseSingleCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile, List<TargetDescriptor> TargetDescriptors)
		{
			List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
			List<UnrealTargetConfiguration> Configurations = new List<UnrealTargetConfiguration>();
			List<string> TargetNames = new List<string>();
			FileReference ProjectFile = Arguments.GetFileReferenceOrDefault("-Project=", null);

			// Settings for creating/using static libraries for the engine
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Count; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex];
				if(Argument.Length > 0 && Argument[0] != '-')
				{
					// Mark this argument as used. We'll interpret it as one thing or another.
					Arguments.MarkAsUsed(ArgumentIndex);

					// Check if it's a project file argument
					if(Argument.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
					{
						FileReference NewProjectFile = new FileReference(Argument);
						if(ProjectFile != null && ProjectFile != NewProjectFile)
						{
							throw new BuildException("Multiple project files specified on command line (first {0}, then {1})", ProjectFile, NewProjectFile);
						}
						ProjectFile = new FileReference(Argument);
						continue;
					}

					// Split it into separate arguments
					string[] InlineArguments = Argument.Split('+');

					// Try to parse them as platforms
					UnrealTargetPlatform ParsedPlatform;
					if(Enum.TryParse(InlineArguments[0], true, out ParsedPlatform) && ParsedPlatform != UnrealTargetPlatform.Unknown)
					{
						Platforms.Add(ParsedPlatform);
						for(int InlineArgumentIdx = 1; InlineArgumentIdx < InlineArguments.Length; InlineArgumentIdx++)
						{
							string InlineArgument = InlineArguments[InlineArgumentIdx];
							if(!Enum.TryParse(InlineArgument, true, out ParsedPlatform) || ParsedPlatform == UnrealTargetPlatform.Unknown)
							{
								throw new BuildException("Invalid platform '{0}'", InlineArgument);
							}
							Platforms.Add(ParsedPlatform);
						}
						continue;
					}

					// Try to parse them as configurations
					UnrealTargetConfiguration ParsedConfiguration;
					if(Enum.TryParse(InlineArguments[0], true, out ParsedConfiguration))
					{
						Configurations.Add(ParsedConfiguration);
						for(int InlineArgumentIdx = 1; InlineArgumentIdx < InlineArguments.Length; InlineArgumentIdx++)
						{
							string InlineArgument = InlineArguments[InlineArgumentIdx];
							if(!Enum.TryParse(InlineArgument, true, out ParsedConfiguration))
							{
								throw new BuildException("Invalid configuration '{0}'", InlineArgument);
							}
							Configurations.Add(ParsedConfiguration);
						}
						continue;
					}

					// Otherwise assume they are target names
					TargetNames.AddRange(InlineArguments);
				}
			}

			if (Platforms.Count == 0)
			{
				throw new BuildException("No platforms specified for target");
			}
			if (Configurations.Count == 0)
			{
				throw new BuildException("No configurations specified for target");
			}

			// Make sure the project file exists
			if(ProjectFile != null && !FileReference.Exists(ProjectFile))
			{
				throw new BuildException("Unable to find project '{0}'.", ProjectFile);
			}

			// Expand all the platforms, architectures and configurations
			foreach(UnrealTargetPlatform Platform in Platforms)
			{
				// Parse the architecture parameter, or get the default for the platform
				List<string> Architectures = new List<string>(Arguments.GetValues("-Architecture=", '+'));
				if(Architectures.Count == 0)
				{
					Architectures.Add(UEBuildPlatform.GetBuildPlatform(Platform).GetDefaultArchitecture(ProjectFile));
				}

				foreach(string Architecture in Architectures)
				{
					foreach(UnrealTargetConfiguration Configuration in Configurations)
					{
						// Create all the target descriptors for targets specified by type
						foreach(string TargetTypeString in Arguments.GetValues("-TargetType="))
						{
							TargetType TargetType;
							if(!Enum.TryParse(TargetTypeString, out TargetType))
							{
								throw new BuildException("Invalid target type '{0}'", TargetTypeString);
							}

							if (ProjectFile == null)
							{
								throw new BuildException("-TargetType=... requires a project file to be specified");
							}
							else
							{
								TargetNames.Add(RulesCompiler.CreateProjectRulesAssembly(ProjectFile, bUsePrecompiled, bSkipRulesCompile).GetTargetNameByType(TargetType, Platform, Configuration, Architecture, ProjectFile));
							}
						}

						// Make sure we could parse something
						if (TargetNames.Count == 0)
						{
							throw new BuildException("No target name was specified on the command-line.");
						}

						// Create all the target descriptors
						foreach(string TargetName in TargetNames)
						{
							// If a project file was not specified see if we can find one
							if (ProjectFile == null && NativeProjects.TryGetProjectForTarget(TargetName, out ProjectFile))
							{
								Log.TraceVerbose("Found project file for {0} - {1}", TargetName, ProjectFile);
							}

							// Create the target descriptor
							TargetDescriptors.Add(new TargetDescriptor(ProjectFile, TargetName, Platform, Configuration, Architecture, Arguments));
						}
					}
				}
			}
		}

		/// <summary>
		/// Try to parse the project file from the command line
		/// </summary>
		/// <param name="Arguments">The command line arguments</param>
		/// <param name="ProjectFile">The project file that was parsed</param>
		/// <returns>True if the project file was parsed, false otherwise</returns>
		public static bool TryParseProjectFileArgument(CommandLineArguments Arguments, out FileReference ProjectFile)
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

			if(UnrealBuildTool.IsProjectInstalled())
			{
				ProjectFile = UnrealBuildTool.GetInstalledProjectFile();
				return true;
			}

			ProjectFile = null;
			return false;
		}

		/// <summary>
		/// Parse a single argument value, of the form -Foo=Bar
		/// </summary>
		/// <param name="Argument">The argument to parse</param>
		/// <param name="Prefix">The argument prefix, eg. "-Foo="</param>
		/// <param name="Value">Receives the value of the argument</param>
		/// <returns>True if the argument could be parsed, false otherwise</returns>
		private static bool ParseArgumentValue(string Argument, string Prefix, out string Value)
		{
			if(Argument.StartsWith(Prefix, StringComparison.InvariantCultureIgnoreCase))
			{
				Value = Argument.Substring(Prefix.Length);
				return true;
			}
			else
			{
				Value = null;
				return false;
			}
		}

		/// <summary>
		/// Format this object for the debugger
		/// </summary>
		/// <returns>String representation of this target descriptor</returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			Result.AppendFormat("{0} {1} {2}", Name, Platform, Configuration);
			if(!String.IsNullOrEmpty(Architecture))
			{
				Result.AppendFormat(" -Architecture={0}", Architecture);
			}
			if(ProjectFile != null)
			{
				Result.AppendFormat(" -Project={0}", Utils.MakePathSafeToUseWithCommandLine(ProjectFile));
			}
			if(AdditionalArguments != null && AdditionalArguments.Count > 0)
			{
				Result.AppendFormat(" {0}", AdditionalArguments);
			}
			return Result.ToString();
		}
	}
}
