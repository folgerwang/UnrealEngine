// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
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
		public string TargetName;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string Architecture;
		public bool bIsEditorRecompile;
		public List<OnlyModule> OnlyModules;
		public FileReference ForeignPlugin;
		public string ForceReceiptFileName;

		public static List<TargetDescriptor> ParseCommandLine(string[] Arguments, ref FileReference ProjectFile)
		{
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			List<string> TargetNames = new List<string>();
			string Architecture = null;
			List<OnlyModule> OnlyModules = new List<OnlyModule>();
			FileReference ForeignPlugin = null;
			string ForceReceiptFileName = null;

			// If true, the recompile was launched by the editor.
			bool bIsEditorRecompile = false;

			// Settings for creating/using static libraries for the engine
			List<string> PossibleTargetNames = new List<string>();
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

					PossibleTargetNames.Add(Argument);
				}
				else
				{
					string Value;
					if(ParseArgumentValue(Argument, "-Module=", out Value))
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
					else if(Argument.Equals("-EditorRecompile", StringComparison.InvariantCultureIgnoreCase))
					{
						bIsEditorRecompile = true;
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

			List<TargetDescriptor> Targets = new List<TargetDescriptor>();
			if (PossibleTargetNames.Count > 0)
			{
				// We have possible targets!
				string PossibleTargetName = PossibleTargetNames[0];

				// If Engine is installed, the PossibleTargetName could contain a path
				string TargetName = PossibleTargetName;

				// If a project file was not specified see if we can find one
				if (ProjectFile == null && UProjectInfo.TryGetProjectForTarget(TargetName, out ProjectFile))
				{
					Log.TraceVerbose("Found project file for {0} - {1}", TargetName, ProjectFile);
				}

				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

				if(Architecture == null)
				{
					Architecture = BuildPlatform.GetDefaultArchitecture(ProjectFile);
				}

				Targets.Add(new TargetDescriptor()
					{
						ProjectFile = ProjectFile,
						TargetName = TargetName,
						Platform = Platform,
						Configuration = Configuration,
						Architecture = Architecture,
						bIsEditorRecompile = bIsEditorRecompile,
						OnlyModules = OnlyModules,
						ForeignPlugin = ForeignPlugin,
						ForceReceiptFileName = ForceReceiptFileName
					});
			}
			if (Targets.Count == 0)
			{
				throw new BuildException("No target name was specified on the command-line.");
			}
			return Targets;
		}

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
