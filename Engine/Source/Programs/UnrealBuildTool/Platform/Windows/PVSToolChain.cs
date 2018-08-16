// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Partial representation of PVS-Studio main settings file
	/// </summary>
	[XmlRoot("ApplicationSettings")]
	public class PVSApplicationSettings
	{
		/// <summary>
		/// Masks for paths excluded for analysis
		/// </summary>
		public string[] PathMasks;

		/// <summary>
		/// Registered username
		/// </summary>
		public string UserName;

		/// <summary>
		/// Registered serial number
		/// </summary>
		public string SerialNumber;
	}

	class PVSToolChain : UEToolChain
	{
		VCEnvironment EnvVars;
		FileReference AnalyzerFile;
		FileReference LicenseFile;
		PVSApplicationSettings ApplicationSettings;

		public PVSToolChain(CppPlatform Platform, ReadOnlyTargetRules Target) : base(Platform)
		{
			EnvVars = VCEnvironment.Create(Target.WindowsPlatform.Compiler, Platform, Target.WindowsPlatform.CompilerVersion, Target.WindowsPlatform.WindowsSdkVersion);

			AnalyzerFile = FileReference.Combine(new DirectoryReference(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86)), "PVS-Studio", "x64", "PVS-Studio.exe");
			if(!FileReference.Exists(AnalyzerFile))
			{
				FileReference EngineAnalyzerFile = FileReference.Combine(UnrealBuildTool.RootDirectory, "Engine", "Extras", "ThirdPartyNotUE", "NoRedist", "PVS-Studio", "PVS-Studio.exe");
				if (FileReference.Exists(EngineAnalyzerFile))
				{
					AnalyzerFile = EngineAnalyzerFile;
				}
				else
				{
					throw new BuildException("Unable to find PVS-Studio at {0} or {1}", AnalyzerFile, EngineAnalyzerFile);
				}
			}

			FileReference SettingsPath = FileReference.Combine(new DirectoryReference(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)), "PVS-Studio", "Settings.xml");
			if (FileReference.Exists(SettingsPath))
			{
				try
				{
					XmlSerializer Serializer = new XmlSerializer(typeof(PVSApplicationSettings));
					using(FileStream Stream = new FileStream(SettingsPath.FullName, FileMode.Open, FileAccess.Read, FileShare.Read))
					{
						ApplicationSettings = (PVSApplicationSettings)Serializer.Deserialize(Stream);
					}
				}
				catch(Exception Ex)
				{
					throw new BuildException(Ex, "Unable to read PVS-Studio settings file from {0}", SettingsPath);
				}
			}

			if(ApplicationSettings != null && !String.IsNullOrEmpty(ApplicationSettings.UserName) && !String.IsNullOrEmpty(ApplicationSettings.SerialNumber))
			{
				LicenseFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "PVS", "PVS-Studio.lic");
				FileItem.CreateIntermediateTextFile(LicenseFile, new string[]{ ApplicationSettings.UserName, ApplicationSettings.SerialNumber });
			}
			else
			{
				FileReference DefaultLicenseFile = AnalyzerFile.ChangeExtension(".lic");
				if(FileReference.Exists(DefaultLicenseFile))
				{
					LicenseFile = DefaultLicenseFile;
				}
			}
		}

		static string GetFullIncludePath(string IncludePath)
		{
			return Path.GetFullPath(ActionThread.ExpandEnvironmentVariables(IncludePath));
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, ActionGraph ActionGraph)
		{
			// Get the MSVC arguments required to compile all files in this batch
			List<string> SharedArguments = new List<string>();
			SharedArguments.Add("/nologo");
			SharedArguments.Add("/P"); // Preprocess
			SharedArguments.Add("/C"); // Preserve comments when preprocessing
			SharedArguments.Add("/D PVS_STUDIO");
			SharedArguments.Add("/wd4005");
			if (EnvVars.Compiler >= WindowsCompiler.VisualStudio2015)
			{
				SharedArguments.Add("/D _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS=1");
			}
			foreach (DirectoryReference IncludePath in CompileEnvironment.IncludePaths.UserIncludePaths)
			{
				SharedArguments.Add(String.Format("/I \"{0}\"", IncludePath));
			}
			foreach (DirectoryReference IncludePath in CompileEnvironment.IncludePaths.SystemIncludePaths)
			{
				SharedArguments.Add(String.Format("/I \"{0}\"", IncludePath));
			}
			foreach (DirectoryReference IncludePath in EnvVars.IncludePaths)
			{
				SharedArguments.Add(String.Format("/I \"{0}\"", IncludePath));
			}
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				SharedArguments.Add(String.Format("/D \"{0}\"", Definition));
			}
			foreach(FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
			{
				SharedArguments.Add(String.Format("/FI\"{0}\"", ForceIncludeFile.Location));
			}

			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				// Get the file names for everything we need
				string BaseFileName = SourceFile.Location.GetFileName();

				// Write the response file
				FileReference PreprocessedFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".i");

				List<string> Arguments = new List<string>(SharedArguments);
				Arguments.Add(String.Format("/Fi\"{0}\"", PreprocessedFileLocation)); // Preprocess to a file
				Arguments.Add(String.Format("\"{0}\"", SourceFile.AbsolutePath));

				FileReference ResponseFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".i.response");
				FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponseFileLocation, String.Join("\n", Arguments));

				// Preprocess the source file
				FileItem PreprocessedFileItem = FileItem.GetItemByFileReference(PreprocessedFileLocation);

				Action PreprocessAction = ActionGraph.Add(ActionType.Compile);
				PreprocessAction.CommandPath = EnvVars.CompilerPath.FullName;
				PreprocessAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				PreprocessAction.CommandArguments = " @\"" + ResponseFileItem.AbsolutePath + "\"";
				PreprocessAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);
				PreprocessAction.PrerequisiteItems.Add(SourceFile);
				PreprocessAction.PrerequisiteItems.Add(ResponseFileItem);
				PreprocessAction.ProducedItems.Add(PreprocessedFileItem);
				PreprocessAction.bShouldOutputStatusDescription = false;

				// Write the PVS studio config file
				StringBuilder ConfigFileContents = new StringBuilder();
				foreach(DirectoryReference IncludePath in EnvVars.IncludePaths)
				{
					ConfigFileContents.AppendFormat("exclude-path={0}\n", IncludePath.FullName);
				}
				foreach(string PathMask in ApplicationSettings.PathMasks)
				{
					if (PathMask.Contains(":") || PathMask.Contains("\\") || PathMask.Contains("/"))
					{
						if(Path.IsPathRooted(PathMask) && !PathMask.Contains(":"))
						{
							ConfigFileContents.AppendFormat("exclude-path=*{0}*\n", PathMask);
						}
						else
						{
							ConfigFileContents.AppendFormat("exclude-path={0}\n", PathMask);
						}
					}
				}
				if (CppPlatform == CppPlatform.Win64)
				{
					ConfigFileContents.Append("platform=x64\n");
				}
				else if(CppPlatform == CppPlatform.Win32)
				{
					ConfigFileContents.Append("platform=Win32\n");
				}
				else
				{
					throw new BuildException("PVS-Studio does not support this platform");
				}
				ConfigFileContents.Append("preprocessor=visualcpp\n");
				ConfigFileContents.Append("language=C++\n");
				ConfigFileContents.Append("skip-cl-exe=yes\n");
				ConfigFileContents.AppendFormat("i-file={0}\n", PreprocessedFileItem.Location.FullName);

				FileReference ConfigFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".cfg");
				FileItem ConfigFileItem = FileItem.CreateIntermediateTextFile(ConfigFileLocation, ConfigFileContents.ToString());

				// Run the analzyer on the preprocessed source file
				FileReference OutputFileLocation = FileReference.Combine(OutputDir, BaseFileName + ".pvslog");
				FileItem OutputFileItem = FileItem.GetItemByFileReference(OutputFileLocation);

				Action AnalyzeAction = ActionGraph.Add(ActionType.Compile);
				AnalyzeAction.CommandDescription = "Analyzing";
				AnalyzeAction.StatusDescription = BaseFileName;
				AnalyzeAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				AnalyzeAction.CommandPath = AnalyzerFile.FullName;
				AnalyzeAction.CommandArguments = String.Format("--cl-params \"{0}\" --source-file \"{1}\" --output-file \"{2}\" --cfg \"{3}\" --analysis-mode 4", PreprocessAction.CommandArguments, SourceFile.AbsolutePath, OutputFileLocation, ConfigFileItem.AbsolutePath);
				if (LicenseFile != null)
				{
					AnalyzeAction.CommandArguments += String.Format(" --lic-file \"{0}\"", LicenseFile);
					AnalyzeAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(LicenseFile));
				}
				AnalyzeAction.PrerequisiteItems.Add(ConfigFileItem);
				AnalyzeAction.PrerequisiteItems.Add(PreprocessedFileItem);
				AnalyzeAction.ProducedItems.Add(OutputFileItem);
				AnalyzeAction.DeleteItems.Add(OutputFileItem); // PVS Studio will append by default, so need to delete produced items

				Result.ObjectFiles.AddRange(AnalyzeAction.ProducedItems);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			throw new BuildException("Unable to link with PVS toolchain.");
		}

		public override void FinalizeOutput(ReadOnlyTargetRules Target, List<FileItem> OutputItems, ActionGraph ActionGraph)
		{
			FileReference OutputFile;
			if (Target.ProjectFile == null)
			{
				OutputFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}
			else
			{
				OutputFile = FileReference.Combine(Target.ProjectFile.Directory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}

			List<FileReference> InputFiles = OutputItems.Select(x => x.Location).Where(x => x.HasExtension(".pvslog")).ToList();

			Action AnalyzeAction = ActionGraph.Add(ActionType.Compile);
			AnalyzeAction.CommandPath = "Dummy.exe";
			AnalyzeAction.CommandArguments = "";
			AnalyzeAction.CommandDescription = "Combining output from PVS-Studio";
			AnalyzeAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			AnalyzeAction.PrerequisiteItems.AddRange(OutputItems);
			AnalyzeAction.ProducedItems.Add(FileItem.GetItemByFileReference(OutputFile));
			AnalyzeAction.ActionHandler = (Action Action, out int ExitCode, out string Output) => WriteResults(OutputFile, InputFiles, out ExitCode, out Output);
			AnalyzeAction.DeleteItems.AddRange(AnalyzeAction.ProducedItems);

			OutputItems.AddRange(AnalyzeAction.ProducedItems);
		}

		void WriteResults(FileReference OutputFile, List<FileReference> InputFiles, out int ExitCode, out string Output)
		{
			StringBuilder OutputBuilder = new StringBuilder();
			OutputBuilder.Append("Processing PVS-Studio output...\n");

			// Create the combined output file, and print the diagnostics to the log
			HashSet<string> UniqueItems = new HashSet<string>();
			using (StreamWriter RawWriter = new StreamWriter(OutputFile.FullName))
			{
				foreach (FileReference InputFile in InputFiles)
				{
					string[] Lines = File.ReadAllLines(InputFile.FullName);
					for(int LineIdx = 0; LineIdx < Lines.Length; LineIdx++)
					{
						string Line = Lines[LineIdx];
						if (!String.IsNullOrWhiteSpace(Line) && UniqueItems.Add(Line))
						{
							bool bCanParse = false;

							string[] Tokens = Line.Split(new string[] { "<#~>" }, StringSplitOptions.None);
							if(Tokens.Length >= 9)
							{
								string Trial = Tokens[1];
								string LineNumberStr = Tokens[2];
								string FileName = Tokens[3];
								string WarningCode = Tokens[5];
								string WarningMessage = Tokens[6];
								string FalseAlarmStr = Tokens[7];
								string LevelStr = Tokens[8];

								int LineNumber;
								bool bFalseAlarm;
								int Level;
								if(int.TryParse(LineNumberStr, out LineNumber) && bool.TryParse(FalseAlarmStr, out bFalseAlarm) && int.TryParse(LevelStr, out Level))
								{
									bCanParse = true;

									// Ignore anything in ThirdParty folders
									if(FileName.Replace('/', '\\').IndexOf("\\ThirdParty\\", StringComparison.InvariantCultureIgnoreCase) == -1)
									{
										// Output the line to the raw output file
										RawWriter.WriteLine(Line);

										// Output the line to the log
										if (!bFalseAlarm && Level == 1)
										{
											OutputBuilder.AppendFormat("{0}({1}): warning {2}: {3}\n", FileName, LineNumber, WarningCode, WarningMessage);
										}
									}
								}
							}

							if(!bCanParse)
							{
								Log.WriteLine(LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}({1}): warning: Unable to parse PVS output line '{2}' (tokens=|{3}|)", InputFile, LineIdx + 1, Line, String.Join("|", Tokens));
							}
						}
					}
				}
			}
			OutputBuilder.AppendFormat("Written {0} {1} to {2}.", UniqueItems.Count, (UniqueItems.Count == 1)? "diagnostic" : "diagnostics", OutputFile.FullName);

			// Return the text to be output. We don't have a custom output handler, so we can just return an empty string.
			Output = OutputBuilder.ToString();
			ExitCode = 0;
		}
	}
}
