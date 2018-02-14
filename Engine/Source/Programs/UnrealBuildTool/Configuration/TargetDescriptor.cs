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
		public string RemoteRoot;
		public List<OnlyModule> OnlyModules;
		public FileReference ForeignPlugin;
		public string ForceReceiptFileName;

		public static List<TargetDescriptor> ParseCommandLine(string[] Arguments, ref FileReference ProjectFile)
		{
			UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
			UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
			List<string> TargetNames = new List<string>();
			string Architecture = null;
			string RemoteRoot = null;
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
					switch (Arguments[ArgumentIndex].ToUpperInvariant())
					{
						case "-MODULE":
							// Specifies a module to recompile.  Can be specified more than once on the command-line to compile multiple specific modules.
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected module name after -Module argument, but found nothing.");
								}
								string OnlyModuleName = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName));
							}
							break;

						case "-MODULEWITHSUFFIX":
							{
								// Specifies a module name to compile along with a suffix to append to the DLL file name.  Can be specified more than once on the command-line to compile multiple specific modules.
								if (ArgumentIndex + 2 >= Arguments.Length)
								{
									throw new BuildException("Expected module name and module suffix -ModuleWithSuffix argument");
								}

								string OnlyModuleName = Arguments[++ArgumentIndex];
								string OnlyModuleSuffix = Arguments[++ArgumentIndex];

								OnlyModules.Add(new OnlyModule(OnlyModuleName, OnlyModuleSuffix));
							}
							break;

						case "-PLUGIN":
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected plugin filename after -Plugin argument, but found nothing.");
								}
								if(ForeignPlugin != null)
								{
									throw new BuildException("Only one foreign plugin to compile may be specified per invocation");
								}
								ForeignPlugin = new FileReference(Arguments[++ArgumentIndex]);
							}
							break;

						case "-RECEIPT":
							{
								if (ArgumentIndex + 1 >= Arguments.Length)
								{
									throw new BuildException("Expected path to the generated receipt after -Receipt argument, but found nothing.");
								}

								ForceReceiptFileName = Arguments[++ArgumentIndex];
							}
							break;

						// -RemoteRoot <RemoteRoot> sets where the generated binaries are CookerSynced.
						case "-REMOTEROOT":
							if (ArgumentIndex + 1 >= Arguments.Length)
							{
								throw new BuildException("Expected path after -RemoteRoot argument, but found nothing.");
							}
							ArgumentIndex++;
							if (Arguments[ArgumentIndex].StartsWith("xe:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("xe:\\".Length);
							}
							else if (Arguments[ArgumentIndex].StartsWith("devkit:\\") == true)
							{
								RemoteRoot = Arguments[ArgumentIndex].Substring("devkit:\\".Length);
							}
							break;

						case "-DEPLOY":
							// Does nothing at the moment...
							break;

						case "-EDITORRECOMPILE":
							{
								bIsEditorRecompile = true;
							}
							break;

						default:
							break;
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
						RemoteRoot = RemoteRoot,
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
	}
}
