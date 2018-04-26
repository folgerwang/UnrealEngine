// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
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
		public List<OnlyModule> OnlyModules;
		public FileReference ForeignPlugin;
		public string ForceReceiptFileName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="TargetName">Name of the target to build</param>
		/// <param name="Platform">Platform to build for</param>
		/// <param name="Configuration">Configuration to build</param>
		/// <param name="Architecture">Architecture to build for</param>
		public TargetDescriptor(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture)
		{
			this.ProjectFile = ProjectFile;
			this.Name = TargetName;
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="ProjectFile">The project file, if already set. May be updated if not.</param>
		/// <returns>List of target descriptors</returns>
		public static List<TargetDescriptor> ParseCommandLine(string[] Arguments, ref FileReference ProjectFile)
		{
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			List<string> TargetNames = new List<string>();
			List<TargetType> TargetTypes = new List<TargetType>();
			string Architecture = null;
			List<OnlyModule> OnlyModules = new List<OnlyModule>();
			FileReference ForeignPlugin = null;
			string ForceReceiptFileName = null;

			// Settings for creating/using static libraries for the engine
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Length; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex];
				if(!Argument.StartsWith("-"))
				{
					UnrealTargetPlatform ParsedPlatform;
					if(Enum.TryParse(Argument, true, out ParsedPlatform) && ParsedPlatform != UnrealTargetPlatform.Unknown)
					{
						if(Platform != UnrealTargetPlatform.Unknown)
						{
							throw new BuildException("Multiple platforms specified on command line (first {0}, then {1})", Platform, ParsedPlatform);
						}
						Platform = ParsedPlatform;
						continue;
					}

					UnrealTargetConfiguration ParsedConfiguration;
					if(Enum.TryParse(Argument, true, out ParsedConfiguration) && ParsedConfiguration != UnrealTargetConfiguration.Unknown)
					{
						if(Configuration != UnrealTargetConfiguration.Unknown)
						{
							throw new BuildException("Multiple configurations specified on command line (first {0}, then {1})", Configuration, ParsedConfiguration);
						}
						Configuration = ParsedConfiguration;
						continue;
					}

					// Make sure the target name is valid. It may be the path to a project file.
					if(Argument.IndexOfAny(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar, '.' }) == -1)
					{
						TargetNames.Add(Argument);
					}
				}
				else
				{
					string Value;
					if(ParseArgumentValue(Argument, "-TargetType=", out Value))
					{
						TargetType Type;
						if(!Enum.TryParse(Value, true, out Type))
						{
							throw new BuildException("Invalid target type: '{0}'", Value);
						}
						TargetTypes.Add(Type);
					}
					else if(ParseArgumentValue(Argument, "-Module=", out Value))
					{
						OnlyModules.Add(new OnlyModule(Value));
					}
					else if(ParseArgumentValue(Argument, "-ModuleWithSuffix=", out Value))
					{
						int SuffixIdx = Value.LastIndexOf(',');
						if(SuffixIdx == -1)
						{
							throw new BuildException("Missing suffix argument from -ModuleWithSuffix=Name,Suffix");
						}
						OnlyModules.Add(new OnlyModule(Value.Substring(0, SuffixIdx), Value.Substring(SuffixIdx + 1)));
					}
					else if(ParseArgumentValue(Argument, "-Plugin=", out Value))
					{
						if(ForeignPlugin != null)
						{
							throw new BuildException("Only one foreign plugin to compile may be specified per invocation");
						}
						ForeignPlugin = new FileReference(Value);
					}
					else if(ParseArgumentValue(Argument, "-Receipt=", out Value))
					{
						ForceReceiptFileName = Value;
					}
					else
					{
						switch (Arguments[ArgumentIndex].ToUpperInvariant())
						{
							case "-MODULE":
								throw new BuildException("'-Module <Name>' syntax is no longer supported on the command line. Use '-Module=<Name>' instead.");
							case "-MODULEWITHSUFFIX":
								throw new BuildException("'-ModuleWithSuffix <Name> <Suffix>' syntax is no longer supported on the command line. Use '-Module=<Name>,<Suffix>' instead.");
							case "-PLUGIN":
								throw new BuildException("'-Plugin <Path>' syntax is no longer supported on the command line. Use '-Plugin=<Path>' instead.");
							case "-RECEIPT":
								throw new BuildException("'-Receipt <Path>' syntax is no longer supported on the command line. Use '-Receipt=<Path>' instead.");
						}
					}
				}
			}

			if (Platform == UnrealTargetPlatform.Unknown)
			{
				throw new BuildException("Couldn't find platform name.");
			}
			if (Configuration == UnrealTargetConfiguration.Unknown)
			{
				throw new BuildException("Couldn't determine configuration name.");
			}

			if(Architecture == null)
			{
				Architecture = UEBuildPlatform.GetBuildPlatform(Platform).GetDefaultArchitecture(ProjectFile);
			}

			// Create all the target descriptors for targets specified by type
			foreach(TargetType Type in TargetTypes)
			{
				if (ProjectFile == null)
				{
					throw new BuildException("-TargetType=... requires a project file to be specified");
				}
				else
				{
					TargetNames.Add(RulesCompiler.CreateProjectRulesAssembly(ProjectFile).GetTargetNameByType(Type, Platform, Configuration, Architecture, ProjectFile, new ReadOnlyBuildVersion(BuildVersion.ReadDefault())));
				}
			}

			// Create all the target descriptor
			List<TargetDescriptor> Targets = new List<TargetDescriptor>();
			foreach(string TargetName in TargetNames)
			{
				// If a project file was not specified see if we can find one
				if (ProjectFile == null && UProjectInfo.TryGetProjectForTarget(TargetName, out ProjectFile))
				{
					Log.TraceVerbose("Found project file for {0} - {1}", TargetName, ProjectFile);
				}

				TargetDescriptor Target = new TargetDescriptor(ProjectFile, TargetName, Platform, Configuration, Architecture);
				Target.OnlyModules = OnlyModules;
				Target.ForeignPlugin = ForeignPlugin;
				Target.ForceReceiptFileName = ForceReceiptFileName;
				Targets.Add(Target);
			}

			// Make sure we could parse something
			if (Targets.Count == 0)
			{
				throw new BuildException("No target name was specified on the command-line.");
			}
			return Targets;
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
	}
}
