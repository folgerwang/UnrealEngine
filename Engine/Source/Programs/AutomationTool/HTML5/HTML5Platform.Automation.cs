// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using System.Net.Http;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

public class HTML5Platform : Platform
{
	// ini configurations
	static bool Compressed = false;
	static bool enableIndexedDB = false; // experimental for now...
	static bool enableMultithreading = true;
	static bool bMultithreading_UseOffscreenCanvas = true;

	public HTML5Platform()
		: base(UnrealTargetPlatform.HTML5)
	{
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		LogInformation("Package {0}", Params.RawProjectPath);

		LogInformation("Setting Emscripten SDK for packaging..");
		HTML5SDKInfo.SetupEmscriptenTemp();
		HTML5SDKInfo.SetUpEmscriptenConfigFile();

		// ----------------------------------------
		// ini configurations
		ConfigHierarchy ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.HTML5);

		ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableMultithreading", out enableMultithreading);
		ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "OffscreenCanvas", out bMultithreading_UseOffscreenCanvas);

		// Debug and Development builds are not compressed to:
		// - speed up iteration times
		// - ensure (IndexedDB) data are not cached/used
		// Shipping builds "can be":
		// - compressed
		// - (IndexedDB) cached
		string ProjectConfiguration = Params.ClientConfigsToBuild[0].ToString();
		if (ProjectConfiguration == "Shipping")
		{
			ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "Compressed", out Compressed);
			ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableIndexedDB", out enableIndexedDB);
		}
		LogInformation("HTML5Platform.Automation: Compressed = "       + Compressed      );
		LogInformation("HTML5Platform.Automation: EnableIndexedDB = "  + enableIndexedDB );
		LogInformation("HTML5Platform.Automation: Multithreading = "   + enableMultithreading );
		LogInformation("HTML5Platform.Automation: OffscreenCanvas = "  + bMultithreading_UseOffscreenCanvas );

		// ----------------------------------------
		// package directory
		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");
		if (!Directory.Exists(PackagePath))
		{
			Directory.CreateDirectory(PackagePath);
		}
		string _ProjectNameExtra = ProjectConfiguration != "Development" ? "-HTML5-" + ProjectConfiguration : "";
		string _ProjectFullpath = Params.GetProjectExeForPlatform(UnrealTargetPlatform.HTML5).ToString();
		string _ProjectFilename = Path.GetFileNameWithoutExtension(_ProjectFullpath) + _ProjectNameExtra;
		string SrcUE4GameBasename = Path.Combine(Path.GetDirectoryName(_ProjectFullpath), _ProjectFilename);
		string UE4GameBasename = Path.Combine(PackagePath, _ProjectFilename);
		string ProjectBasename = Path.Combine(PackagePath, Params.ShortProjectName + _ProjectNameExtra);

		// ----------------------------------------
		// packaging
		if (HTMLPakAutomation.CanCreateMapPaks(Params))
		{
			HTMLPakAutomation PakAutomation = new HTMLPakAutomation(Params, SC);

			// Create Necessary Paks.
			PakAutomation.CreateEnginePak();
			PakAutomation.CreateGamePak();
			PakAutomation.CreateContentDirectoryPak();

			// Create Emscripten Package from Necessary Paks. - This will be the VFS.
			PakAutomation.CreateEmscriptenDataPackage(PackagePath, ProjectBasename + ".data");

			// Create All Map Paks which  will be downloaded on the fly.
			PakAutomation.CreateMapPak();

			// Create Delta Paks if setup.
			List<string> Paks = new List<string>();
			ConfigCache.GetArray("/Script/HTML5PlatformEditor.HTML5TargetSettings", "LevelTransitions", out Paks);

			if (Paks != null)
			{
				foreach (var Pak in Paks)
				{
					var Matched = Regex.Matches(Pak, "\"[^\"]+\"", RegexOptions.IgnoreCase);
					string MapFrom = Path.GetFileNameWithoutExtension(Matched[0].ToString().Replace("\"", ""));
					string MapTo = Path.GetFileNameWithoutExtension(Matched[1].ToString().Replace("\"", ""));
					PakAutomation.CreateDeltaMapPaks(MapFrom, MapTo);
				}
			}
		}
		else
		{
			// we need to operate in the root
			string PythonPath = HTML5SDKInfo.Python().FullName;
			string EmPackagerPath = HTML5SDKInfo.EmscriptenPackager();

			using (new ScopedEnvVar("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN))
			{
				using (new PushedDirectory(Path.Combine(Params.BaseStageDirectory, "HTML5")))
				{
					string CmdLine = string.Format("\"{0}\" \"{1}\" --preload . --js-output=\"{1}.js\" --no-heap-copy", EmPackagerPath, ProjectBasename + ".data");
					RunAndLog(CmdEnv, PythonPath, CmdLine);
				}
			}
		}


		// ----------------------------------------
		// copy to package directory

		// ensure the ue4game binary exists, if applicable
		if (!FileExists_NoExceptions(SrcUE4GameBasename + ".js"))
		{
            LogInformation("Failed to find game application " + SrcUE4GameBasename + ".js");
			throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find application {0}. You may need to build the UE4 project with your target configuration and platform.", SrcUE4GameBasename + ".js");
		}

		if ( !Params.IsCodeBasedProject )
		{
			// template project - need to copy over UE4Game.*
			File.Copy(SrcUE4GameBasename + ".wasm", UE4GameBasename + ".wasm", true);
			File.Copy(SrcUE4GameBasename + ".js",   UE4GameBasename + ".js",   true);

			File.SetAttributes(UE4GameBasename + ".wasm", FileAttributes.Normal);
			File.SetAttributes(UE4GameBasename + ".js" ,  FileAttributes.Normal);

			if (File.Exists(SrcUE4GameBasename + ".js.symbols"))
			{
				File.Copy(      SrcUE4GameBasename + ".js.symbols", UE4GameBasename + ".js.symbols", true);
				File.SetAttributes(UE4GameBasename + ".js.symbols", FileAttributes.Normal);
			}

			if (enableMultithreading)
			{
				File.Copy(      SrcUE4GameBasename + ".js.mem", UE4GameBasename + ".js.mem", true);
				File.SetAttributes(UE4GameBasename + ".js.mem", FileAttributes.Normal);

				File.Copy(Path.Combine(Path.GetDirectoryName(_ProjectFullpath), "pthread-main.js"), Path.Combine(PackagePath, "pthread-main.js"), true);
				File.SetAttributes(Path.Combine(PackagePath, "pthread-main.js"), FileAttributes.Normal);
			}
			else
			{
				// nuke possibly old deployed pthread-main.js, which is not needed for singlethreaded build.
				File.Delete(Path.Combine(PackagePath, "pthread-main.js"));
			}
		}
		// else, c++ projects will compile "to" PackagePath

		// note: ( ProjectBasename + ".data" ) already created above (!HTMLPakAutomation.CanCreateMapPaks())


		// ----------------------------------------
		// generate HTML files to the package directory

		// custom HTML, JS (if any), and CSS (if any) template files
		string LocalBuildPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5");
		string BuildPath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Build", "HTML5");
		string TemplateFile = CombinePaths(BuildPath, "project_template.html");
		if ( !File.Exists(TemplateFile) )
		{
			// fall back to default UE4 template files
			BuildPath = LocalBuildPath;
			TemplateFile = CombinePaths(BuildPath, "project_template.html");
		}
		GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".html", Params, ConfigCache);

		TemplateFile = CombinePaths(BuildPath, "project_template.js");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".UE4.js", Params, ConfigCache);
		}

		TemplateFile = CombinePaths(BuildPath, "project_template.css");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile, ProjectBasename + ".css", Params, ConfigCache);
		}


		// ----------------------------------------
		// (development) support files
		string MacBashTemplateFile = CombinePaths(LocalBuildPath, "RunMacHTML5LaunchHelper_template.command");
		string MacBashOutputFile = Path.Combine(PackagePath, "RunMacHTML5LaunchHelper.command");
		string MonoPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/SetupMono.sh");
		GenerateMacCommandFromTemplate(MacBashTemplateFile, MacBashOutputFile, MonoPath);
		// ........................................
		string htaccesspath = Path.Combine(PackagePath, ".htaccess");
		if ( File.Exists(htaccesspath) )
		{
			FileAttributes attributes = File.GetAttributes(htaccesspath);
			if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
			{
				attributes &= ~FileAttributes.ReadOnly;
				File.SetAttributes(htaccesspath, attributes);
			}
		}
		File.Copy(CombinePaths(LocalBuildPath, "htaccess_template.txt"), htaccesspath, true);


		// ----------------------------------------
		// final copies

		// Gather utlity .js files and combine into one file
		string DestinationFile = PackagePath + "/Utility.js";
		File.Delete(DestinationFile);
		// spelling this out - one file at a time (i.e. don't slurp in project_template.js)
		File.AppendAllText(DestinationFile, File.ReadAllText(CombinePaths(LocalBuildPath, "json2.js")));
		File.AppendAllText(DestinationFile, File.ReadAllText(CombinePaths(LocalBuildPath, "jstorage.js")));
		File.AppendAllText(DestinationFile, File.ReadAllText(CombinePaths(LocalBuildPath, "moz_binarystring.js")));

		if (Compressed)
		{
			LogInformation("Build configuration is " + ProjectConfiguration + ", so (gzip) compressing files for web servers.");

			// Compress all files. These are independent tasks which can be threaded.
			List<Task> CompressionTasks = new List<Task>();

			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(UE4GameBasename + ".wasm",       UE4GameBasename + ".wasmgz")));				// main game code
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(UE4GameBasename + ".js",         UE4GameBasename + ".jsgz")));				// main js (emscripten)
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(PackagePath + "/Utility.js",     PackagePath + "/Utility.jsgz")));			// Utility
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(ProjectBasename + ".data",       ProjectBasename + ".datagz")));				// DATA file
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(ProjectBasename + ".data.js" ,   ProjectBasename + ".data.jsgz")));			// DATA file .js driver (emscripten)
			if ( File.Exists(UE4GameBasename + ".js.symbols") )
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(UE4GameBasename + ".js.symbols", UE4GameBasename + ".js.symbolsgz")));	// symbols fil.
			}
			if ( File.Exists(ProjectBasename + ".UE4.js") )
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(ProjectBasename + ".UE4.js" , ProjectBasename + ".UE4.jsgz")));			// UE4 js
			}
			if ( File.Exists(ProjectBasename + ".css") )
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(ProjectBasename + ".css" , ProjectBasename + ".cssgz")));					// UE4 css
			}
			if (enableMultithreading)
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(UE4GameBasename + ".js.mem", UE4GameBasename + ".js.memgz")));			// mem init file
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(PackagePath + "/pthread-main.js", PackagePath + "/pthread-main.jsgz")));	// pthread file
			}
			Task.WaitAll(CompressionTasks.ToArray());
		}
		else
		{
			LogInformation("Build configuration is " + ProjectConfiguration + ", so not compressing. Build Shipping configuration to compress files to save space.");

			// nuke old compressed files to prevent using stale files
			File.Delete(UE4GameBasename + ".wasmgz");
			File.Delete(UE4GameBasename + ".jsgz");
			File.Delete(UE4GameBasename + ".js.symbolsgz");
			File.Delete(UE4GameBasename + ".js.memgz");
			File.Delete(PackagePath + "/pthread-main.jsgz");
			File.Delete(PackagePath + "/Utility.jsgz");
			File.Delete(ProjectBasename + ".datagz");
			File.Delete(ProjectBasename + ".data.jsgz");
			File.Delete(ProjectBasename + ".UE4.jsgz");
			File.Delete(ProjectBasename + ".cssgz");
		}

		File.Copy(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe"),CombinePaths(PackagePath, "HTML5LaunchHelper.exe"),true);
//		Task.WaitAll(CompressionTasks);
		PrintRunTime();
	}

	void CompressFile(string Source, string Destination)
	{
		LogInformation(" Compressing " + Source);
		bool DeleteSource = false;

		if(  Source == Destination )
		{
			string CopyOrig = Source + ".Copy";
			File.Copy(Source, CopyOrig);
			Source = CopyOrig;
			DeleteSource = true;
		}

		using (System.IO.Stream input = System.IO.File.OpenRead(Source))
		{
			using (var raw = System.IO.File.Create(Destination))
			{
				using (Stream compressor = new Ionic.Zlib.GZipStream(raw, Ionic.Zlib.CompressionMode.Compress,Ionic.Zlib.CompressionLevel.BestCompression))
				{
					byte[] buffer = new byte[2048];
					int SizeRead = 0;
					while ((SizeRead = input.Read(buffer, 0, buffer.Length)) != 0)
					{
						compressor.Write(buffer, 0, SizeRead);
					}
				}
			}
		}

		if (DeleteSource)
		{
			File.Delete(Source);
		}
	}

	protected void GenerateFileFromTemplate(string InTemplateFile, string InOutputFile, ProjectParams Params, ConfigHierarchy ConfigCache)
	{
		bool IsContentOnly = !Params.IsCodeBasedProject;
		string ProjectConfiguration = Params.ClientConfigsToBuild[0].ToString();

		string UE4GameName = IsContentOnly ? "UE4Game" : Params.ShortProjectName;
		string ProjectName = Params.ShortProjectName;
		if (ProjectConfiguration != "Development")
		{
			UE4GameName += "-HTML5-" + ProjectConfiguration;
			ProjectName += "-HTML5-" + ProjectConfiguration;
		}

		string CanvasScaleMode;
		ConfigCache.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "CanvasScalingMode", out CanvasScaleMode);

		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("%TIMESTAMP%"))
				{
					string TimeStamp = DateTime.UtcNow.ToString("yyyyMMddHHmm");
					LineStr = LineStr.Replace("%TIMESTAMP%", TimeStamp);
				}

				if (LineStr.Contains("%SHORTNAME%"))
				{
					LineStr = LineStr.Replace("%SHORTNAME%", Params.ShortProjectName);
				}

				if (LineStr.Contains("%UE4GAMENAME%"))
				{
					LineStr = LineStr.Replace("%UE4GAMENAME%", UE4GameName);
				}

				if (LineStr.Contains("%PROJECTNAME%"))
				{
					LineStr = LineStr.Replace("%PROJECTNAME%", ProjectName);
				}

				if (LineStr.Contains("%SERVE_COMPRESSED%"))
				{
					LineStr = LineStr.Replace("%SERVE_COMPRESSED%", Compressed ? "true" : "false");
				}

				if (LineStr.Contains("%DISABLE_INDEXEDDB%"))
				{
					LineStr = LineStr.Replace("%DISABLE_INDEXEDDB%",
							enableIndexedDB ? "" : "enableReadFromIndexedDB = false;\nenableWriteToIndexedDB = false;");
				}

				if (LineStr.Contains("%UE4CMDLINE%"))
				{
					string ArgumentString = "'../../../" + Params.ShortProjectName + "/" + Params.ShortProjectName + ".uproject',";
					ArgumentString += "'-stdout',"; // suppress double printing to console.log
					LineStr = LineStr.Replace("%UE4CMDLINE%", ArgumentString);
				}

				if (LineStr.Contains("%CANVASSCALEMODE%"))
				{
					string mode = "2 /*ASPECT*/"; // default
					if ( CanvasScaleMode.Equals("stretch", StringComparison.InvariantCultureIgnoreCase))
					{
						mode = "1 /*STRETCH*/";
					}
					else if ( CanvasScaleMode.Equals("fixed", StringComparison.InvariantCultureIgnoreCase))
					{
						mode = "3 /*FIXED*/";
					}
					LineStr = LineStr.Replace("%CANVASSCALEMODE%", mode);
				}

				if (LineStr.Contains("%MULTITHREADED%"))
				{
					LineStr = LineStr.Replace("%MULTITHREADED%", enableMultithreading ? "true" : "false");
				}

				if (LineStr.Contains("%OFFSCREENCANVAS%"))
				{
					LineStr = LineStr.Replace("%OFFSCREENCANVAS%", bMultithreading_UseOffscreenCanvas ? "true" : "false");
				}

				if (LineStr.Contains("%EMSDK_VERSION%"))
				{
					string escpath = HTML5SDKInfo.EMSCRIPTEN_ROOT.Replace("\\", "/");
					LineStr = LineStr.Replace("%EMSDK_VERSION%", (ProjectConfiguration == "Shipping") ? HTML5SDKInfo.EmscriptenVersion() : escpath);
				}

				if (LineStr.Contains("%EMSDK_CONFIG%"))
				{
					string escpath = HTML5SDKInfo.DOT_EMSCRIPTEN.Replace("\\", "/");
					LineStr = LineStr.Replace("%EMSDK_CONFIG%", (ProjectConfiguration == "Shipping") ? "" : escpath);
				}

				outputContents.AppendLine(LineStr);
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				File.WriteAllText(InOutputFile, outputContents.ToString(), Encoding.UTF8);
			}
			catch (Exception)
			{
				// Unable to write to the project file.
			}
		}
	}

	protected void GenerateMacCommandFromTemplate(string InTemplateFile, string InOutputFile, string InMonoPath)
	{
		StringBuilder outputContents = new StringBuilder();
		using (StreamReader reader = new StreamReader(InTemplateFile))
		{
			string InMonoPathParent = Path.GetDirectoryName(InMonoPath);
			string LineStr = null;
			while (reader.Peek() != -1)
			{
				LineStr = reader.ReadLine();
				if (LineStr.Contains("${unreal_mono_pkg_path}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path}", InMonoPath);
				}
				if (LineStr.Contains("${unreal_mono_pkg_path_base}"))
				{
					LineStr = LineStr.Replace("${unreal_mono_pkg_path_base}", InMonoPathParent);
				}

				outputContents.Append(LineStr + '\n');
			}
		}

		if (outputContents.Length > 0)
		{
			// Save the file. We Copy the template file to keep any permissions set to it.
			try
			{
				Directory.CreateDirectory(Path.GetDirectoryName(InOutputFile));
				if (File.Exists(InOutputFile))
				{
					File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
					File.Delete(InOutputFile);
				}
				File.Copy(InTemplateFile, InOutputFile);
				File.SetAttributes(InOutputFile, File.GetAttributes(InOutputFile) & ~FileAttributes.ReadOnly);
//				System.Diagnostics.Process.Start("chmod", "+x " + InOutputFile);
				using (var CmdFile = File.Open(InOutputFile, FileMode.OpenOrCreate | FileMode.Truncate))
				{
					Byte[] BytesToWrite = new UTF8Encoding(true).GetBytes(outputContents.ToString());
					CmdFile.Write(BytesToWrite, 0, BytesToWrite.Length);
				}
			}
			catch (Exception)
			{
				// Unable to write to the project file.
			}
		}
	}

	// --------------------------------------------------------------------------------
	// ArchiveCommand.Automation.cs

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		// copy to archive directory
		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");

		string UE4GameBasename = Path.GetFileNameWithoutExtension(Params.GetProjectExeForPlatform(UnrealTargetPlatform.HTML5).ToString());
		string ProjectBasename = Params.ShortProjectName;
		string ProjectConfiguration = Params.ClientConfigsToBuild[0].ToString();
		if (ProjectConfiguration != "Development")
		{
			UE4GameBasename += "-HTML5-" + ProjectConfiguration;
			ProjectBasename += "-HTML5-" + ProjectConfiguration;
		}

		SC.ArchiveFiles(PackagePath, UE4GameBasename + ".wasm");			// MAIN game code
		SC.ArchiveFiles(PackagePath, UE4GameBasename + ".js");				// MAIN js file (emscripten)
		SC.ArchiveFiles(PackagePath, "Utility.js");							// utilities
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".data");			// DATA file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".data.js");			// DATA file js driver (emscripten)
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".UE4.js");			// UE4 js file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".css");				// UE4 css file
		SC.ArchiveFiles(PackagePath, ProjectBasename + ".html");			// landing page.

		if (File.Exists(CombinePaths(PackagePath, UE4GameBasename + ".symbols")))
		{
			SC.ArchiveFiles(PackagePath, UE4GameBasename + ".js.symbols");	// symbols file
		}

		if (enableMultithreading)
		{
			SC.ArchiveFiles(PackagePath, UE4GameBasename + ".js.mem");		// memory init file
			SC.ArchiveFiles(PackagePath, "pthread-main.js");
		}

		// Archive HTML5 Server and a Readme.
		SC.ArchiveFiles(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/"), "HTML5LaunchHelper.exe");
		SC.ArchiveFiles(CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5/"), "Readme.txt");
		SC.ArchiveFiles(PackagePath, "RunMacHTML5LaunchHelper.command");
		SC.ArchiveFiles(PackagePath, ".htaccess");

		if (Compressed)
		{
			SC.ArchiveFiles(PackagePath, UE4GameBasename + ".wasmgz");
			SC.ArchiveFiles(PackagePath, UE4GameBasename + ".jsgz");
			SC.ArchiveFiles(PackagePath, "Utility.jsgz");
			SC.ArchiveFiles(PackagePath, ProjectBasename + ".datagz");
			SC.ArchiveFiles(PackagePath, ProjectBasename + ".data.jsgz");
			SC.ArchiveFiles(PackagePath, ProjectBasename + ".UE4.jsgz");
			SC.ArchiveFiles(PackagePath, ProjectBasename + ".cssgz");
			if (File.Exists(CombinePaths(PackagePath, UE4GameBasename + ".js.symbolsgz")))
			{
				SC.ArchiveFiles(PackagePath, UE4GameBasename + ".js.symbolsgz");
			}
			if (enableMultithreading)
			{
				SC.ArchiveFiles(PackagePath, UE4GameBasename + ".js.memgz");
				SC.ArchiveFiles(PackagePath, "pthread-main.jsgz");
			}
		}
		else
		{
			// nuke old compressed files to prevent using stale files
			File.Delete(UE4GameBasename + ".wasmgz");
			File.Delete(UE4GameBasename + ".jsgz");
			File.Delete(UE4GameBasename + ".js.symbolsgz");
			File.Delete("pthread-main.jsgz");
			File.Delete("Utility.jsgz");
			File.Delete(ProjectBasename + ".datagz");
			File.Delete(ProjectBasename + ".data.jsgz");
			File.Delete(ProjectBasename + ".UE4.jsgz");
			File.Delete(ProjectBasename + ".cssgz");
		}

		if (HTMLPakAutomation.CanCreateMapPaks(Params))
		{
			// find all paks.
			string[] Files = Directory.GetFiles(Path.Combine(PackagePath, Params.ShortProjectName), "*", SearchOption.AllDirectories);
			foreach(string PakFile in Files)
			{
				SC.ArchivedFiles.Add(PakFile, Path.GetFileName(PakFile));
			}
		}

		UploadToS3(SC, ProjectBasename + ".html");
	}

	// --------------------------------------------------------------------------------
	// RunProjectCommand.Automation.cs

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		// look for browser
		string BrowserPath = Params.Devices[0].Replace("HTML5@", "");

		// open the webpage
		Int32 ServerPort = 8000; // HTML5LaunchHelper default

		var ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.HTML5);
		ConfigCache.GetInt32("/Script/HTML5PlatformEditor.HTML5TargetSettings", "DeployServerPort", out ServerPort); // LaunchOn via Editor or FrontEnd
		string WorkingDirectory = Path.GetDirectoryName(ClientApp);
		string url = Path.GetFileName(ClientApp) +".html";

// UE-64628: seems proxy port is no longer used anymore -- leaving details here for future reference...
//		// WARNING: splitting the following situation
//		// if cookonthefly is used: tell the browser to use the PROXY at DEFAULT_HTTP_FILE_SERVING_PORT
//		// leave the normal HTML5LaunchHelper port (ServerPort) alone -- otherwise it will collide with the PROXY port
		if (ClientCmdLine.Contains("filehostip"))
		{
			url += "?cookonthefly=true";
//			Int32 ProxyPort = 41898; // DEFAULT_HTTP_FILE_SERVING_PORT
//			url = String.Format("http://localhost:{0}/{1}", ProxyPort, url);
		}
//		else
//		{
			url = String.Format("http://localhost:{0}/{1}", ServerPort, url);
//		}

		// Check HTML5LaunchHelper source for command line args

		var LowerBrowserPath = BrowserPath.ToLower();
		var ProfileDirectory = Path.Combine(Utils.GetUserSettingDirectory().FullName, "UE4_HTML5", "user");

		string BrowserCommandline = url;

		if (LowerBrowserPath.Contains("chrome"))
		{
			ProfileDirectory = Path.Combine(ProfileDirectory, "chrome");
			// removing [--enable-logging] otherwise, chrome breaks with a bunch of the following errors:
			// > ERROR:process_info.cc(631)] range at 0x7848406c00000000, size 0x1a4 fully unreadable
			// leaving this note here for future reference: UE-45078
			BrowserCommandline  += "  " + String.Format("--user-data-dir=\\\"{0}\\\"   --no-first-run", ProfileDirectory);
		}
		else if (LowerBrowserPath.Contains("firefox"))
		{
			ProfileDirectory = Path.Combine(ProfileDirectory, "firefox");
			BrowserCommandline += "  " +  String.Format("-no-remote -profile \\\"{0}\\\"", ProfileDirectory);
		}
		else if (LowerBrowserPath.Contains("safari"))
		{
			// NOT SUPPORTED: cannot have a separate UE4 profile for safari
			// -- this "can" be done with a different user (e.g. guest) account...
			//    (which is not a turn key solution that can be done within UE4)
			// -- some have tried using symlinks to "mimic" this
			//    https://discussions.apple.com/thread/3327990
			// -- but, none of these are fool proof with an existing/running safari instance

			// -- also, "Safari Extensions JS" has been officially deprecated as of Safari 12
			//    (in favor of using "Safari App Extension")
			//    https://developer.apple.com/documentation/safariextensions

			// this means, Safari "LaunchOn" (UE4 Editor -> Launch -> Safari) will run with your FULL
			// Safari profile -- so, all of your "previously opened" tabs will all also be opened...
		}

		// TODO: test on other platforms to remove this first if() check
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
		{
			if (!Directory.Exists(ProfileDirectory))
			{
				Directory.CreateDirectory(ProfileDirectory);
			}
		}

		string LauncherArguments = string.Format(" -Browser=\"{0}\" + -BrowserCommandLine=\"{1}\" -ServerPort=\"{2}\" -ServerRoot=\"{3}\" ",
				new object[] { BrowserPath, BrowserCommandline, ServerPort, WorkingDirectory });

		var LaunchHelperPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe");
		IProcessResult BrowserProcess = Run(LaunchHelperPath, LauncherArguments, null, ClientRunFlags | ERunOptions.NoWaitForExit);

		return BrowserProcess;
	}

	public override List<FileReference> GetExecutableNames(DeploymentContext SC)
	{
		List<FileReference> ExecutableNames = new List<FileReference>();
		ExecutableNames.Add(FileReference.Combine(SC.ProjectRoot, "Binaries", "HTML5", SC.ShortProjectName));
		return ExecutableNames;
	}

	// --------------------------------------------------------------------------------
	// PackageCommand.Automation.cs

	public override bool RequiresPackageToDeploy
	{
		get { return true; }
	}

	// --------------------------------------------------------------------------------
	// CookCommand.Automation.cs

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "HTML5";
	}

	public override string GetCookExtraCommandLine(ProjectParams Params)
	{
		return HTMLPakAutomation.CanCreateMapPaks(Params) ? " -GenerateDependenciesForMaps " : "";
	}

	// --------------------------------------------------------------------------------
	// CopyBuildToStagingDirectory.Automation.cs

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// must implement -- "empty" here
	}

	public override PakType RequiresPak(ProjectParams Params)
	{
		return HTMLPakAutomation.CanCreateMapPaks(Params) ? PakType.Never : PakType.Always;
	}

	public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return Compressed ? " -compress" : "";
	}

	// --------------------------------------------------------------------------------
	// AutomationUtils/Platform.cs

	public override bool IsSupported { get { return true; } }

	// --------------------------------------------------------------------------------
	// --------------------------------------------------------------------------------

#region AMAZON S3
	public void UploadToS3(DeploymentContext SC, string OutputFilename)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool Upload = false;

		string Region = "";
		string KeyId = "";
		string AccessKey = "";
		string BucketName = "";
		string FolderName = "";

		if (! Ini.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "UploadToS3", out Upload) || ! Upload )
		{
			return;
		}

		bool AmazonIdentity = Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3Region", out Region) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3KeyID", out KeyId) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3SecretAccessKey", out AccessKey) &&
								Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3BucketName", out BucketName);

		if ( !AmazonIdentity )
		{
			LogInformation("Amazon S3 Incorrectly configured");
			return;
		}

		Ini.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "S3FolderName", out FolderName);
		if ( FolderName == "" )
		{
			FolderName = SC.ShortProjectName;
		}
		else
		{
			// strip any before and after folder "/"
			FolderName = Regex.Replace(Regex.Replace(FolderName, "^/+", "" ), "/+$", "");
		}

		List<Task> UploadTasks = new List<Task>();
		long msTimeOut = 0;
		foreach (KeyValuePair<string, string> Entry in SC.ArchivedFiles)
		{
			FileInfo Info = new FileInfo(Entry.Key);
			UploadTasks.Add(UploadToS3Worker(Info, Region, KeyId, AccessKey, BucketName, FolderName));
			if ( msTimeOut < Info.Length )
			{
				msTimeOut = Info.Length;
			}
		}
		msTimeOut /= 100; // [miliseconds] give 10 secs for each ~MB ( (10s * 1000ms) / ( 1024KB * 1024MB * 1000ms ) )
		if ( msTimeOut < (100*1000) ) // HttpClient: default timeout is 100 sec
		{
			msTimeOut = 100*1000;
		}
		LogInformation("Upload Timeout set to: " + (msTimeOut/1000) + "secs");
		Task.WaitAll(UploadTasks.ToArray(), (int)msTimeOut); // set timeout [miliseconds]

		string URL = "https://" + BucketName + ".s3.amazonaws.com/" + FolderName + "/" + OutputFilename;
		LogInformation("Your project's shareable link is: " + URL);

		LogInformation("Upload Tasks finished.");
	}

	private static IDictionary<string, string> MimeTypeMapping = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase)
		{
			// the following will default to: "application/octet-stream"
			// .data .datagz

			{ ".wasm", "application/wasm" },
			{ ".wasmgz", "application/wasm" },
			{ ".htaccess", "text/plain"},
			{ ".html", "text/html"},
			{ ".css", "text/css"},
			{ ".cssgz", "text/css"},
			{ ".js", "application/x-javascript" },
			{ ".jsgz", "application/x-javascript" },
			{ ".symbols", "text/plain"},
			{ ".symbolsgz", "text/plain"},
			{ ".txt", "text/plain"}
		};

	static async Task UploadToS3Worker(FileInfo Info, string Region, string KeyId, string AccessKey, string BucketName, string FolderName)
	{
		// --------------------------------------------------
		// "AWS Signature Version 4"
		// http://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
		// --------------------------------------------------
		LogInformation(" Uploading " + Info.Name);

		// --------------------------------------------------
		// http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-post-example.html
		string TimeStamp = DateTime.UtcNow.ToString("yyyyMMddTHHmmssZ");
		string DateExpire = DateTime.UtcNow.AddDays(1).ToString("yyyy-MM-dd");
		string AWSDate = DateTime.UtcNow.AddDays(1).ToString("yyyyMMdd");
		string MimeType = (MimeTypeMapping.ContainsKey(Info.Extension))
							? MimeTypeMapping[Info.Extension]
							: "application/octet-stream";
		string MimePath = MimeType.Split('/')[0];
		string AWSCredential = KeyId + "/" + AWSDate + "/" + Region + "/s3/aws4_request";

		// --------------------------------------------------
		string policy = "{ \"expiration\": \"" + DateExpire + "T12:00:00.000Z\"," +
						" \"conditions\": [" +
						" { \"bucket\": \"" + BucketName + "\" }," +
						" [ \"starts-with\", \"$key\", \"" + FolderName + "/\" ]," +
						" { \"acl\": \"public-read\" }," +
						" [ \"starts-with\", \"$content-type\", \"" + MimePath + "/\" ],";
		if (Info.Extension.EndsWith("gz"))
		{
			policy += " [ \"starts-with\", \"$content-encoding\", \"gzip\" ],";
		}
		policy +=		" { \"x-amz-credential\": \"" + AWSCredential + "\" }," +
						" { \"x-amz-algorithm\": \"AWS4-HMAC-SHA256\" }," +
						" { \"x-amz-date\": \"" + TimeStamp + "\" }" +
						" ]" +
						"}";
		string policyBase64 = System.Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(policy), Base64FormattingOptions.InsertLineBreaks);

		// --------------------------------------------------
		// http://docs.aws.amazon.com/general/latest/gr/signature-v4-examples.html
		var kha = KeyedHashAlgorithm.Create("HmacSHA256");
		kha.Key = Encoding.UTF8.GetBytes(("AWS4" + AccessKey).ToCharArray()); // kSecret
		byte[] sig = kha.ComputeHash(Encoding.UTF8.GetBytes(AWSDate));
		kha.Key = sig; // kDate

		sig = kha.ComputeHash(Encoding.UTF8.GetBytes(Region));
		kha.Key = sig; // kRegion

		sig = kha.ComputeHash(Encoding.UTF8.GetBytes("s3"));
		kha.Key = sig; // kService

		sig = kha.ComputeHash(Encoding.UTF8.GetBytes("aws4_request"));
		kha.Key = sig; // kSigning

		sig = kha.ComputeHash(Encoding.UTF8.GetBytes(policyBase64));
		string signature = BitConverter.ToString(sig).Replace("-", "").ToLower(); // for Authorization

		// debugging...
		//Console.WriteLine("policy: [" + policy + "]");
		//Console.WriteLine("policyBase64: [" + policyBase64 + "]");
		//Console.WriteLine("signature: [" + signature + "]");

		// --------------------------------------------------
		HttpClient httpClient = new HttpClient();
		var formData = new MultipartFormDataContent();
		formData.Add(new StringContent(FolderName + "/" + Info.Name), "key");
		formData.Add(new StringContent("public-read"), "acl");
		formData.Add(new StringContent(AWSCredential), "X-Amz-Credential");
		formData.Add(new StringContent("AWS4-HMAC-SHA256"), "X-Amz-Algorithm");
		formData.Add(new StringContent(signature), "X-Amz-Signature");
		formData.Add(new StringContent(TimeStamp), "X-Amz-Date");
		formData.Add(new StringContent(policyBase64), "Policy");
		formData.Add(new StringContent(MimeType), "Content-Type");
		if ( Info.Extension.EndsWith("gz") )
		{
			formData.Add(new StringContent("gzip"), "Content-Encoding");
		}
		// debugging...
		//Console.WriteLine("key: [" + FolderName + "/" + Info.Name + "]");
		//Console.WriteLine("AWSCredential: [" + AWSCredential + "]");
		//Console.WriteLine("TimeStamp: [" + TimeStamp + "]");
		//Console.WriteLine("MimeType: [" + MimeType + "]");

		// the file ----------------------------------------
		var fileContent = new ByteArrayContent(System.IO.File.ReadAllBytes(Info.FullName));
		fileContent.Headers.ContentType = System.Net.Http.Headers.MediaTypeHeaderValue.Parse(MimeType);
		formData.Add(fileContent, "file", Info.Name);
		int adjustTimeout = (int)(Info.Length / (100*1000)); // [seconds] give 10 secs for each ~MB ( (10s * 1000ms) / ( 1024KB * 1024MB * 1000ms ) )
		if ( adjustTimeout > 100 ) // default timeout is 100 sec
		{
			httpClient.Timeout = TimeSpan.FromSeconds(adjustTimeout); // increase timeout
		}
		//Console.WriteLine("httpClient Timeout: [" + httpClient.Timeout + "]" );

		// upload ----------------------------------------
		string URL = "https://" + BucketName + ".s3.amazonaws.com/";
		var response = await httpClient.PostAsync(URL, formData);
		if (response.IsSuccessStatusCode)
		{
			LogInformation("Upload done: " + Info.Name);
		}
		else
		{
			var contents = response.Content.ReadAsStringAsync();
			var reason = Regex.Replace(
/* grab inner block */ Regex.Replace(contents.Result, "<[^>]+>\n<[^>]+>([^<]+)</[^>]+>", "$1")
/* strip tags */       , "<([^>]+)>([^<]+)</[^>]+>", "$1 - $2\n");

			//Console.WriteLine("Fail to Upload: " + Info.Name + " Header - " + response.ToString());
			Console.WriteLine("Fail to Upload: " + Info.Name + "\nResponse - " + reason);
			throw new Exception("FAILED TO UPLOAD: " + Info.Name);
		}
	}
	#endregion
}
