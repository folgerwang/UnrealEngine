// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
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

	public HTML5Platform()
		: base(UnrealTargetPlatform.HTML5)
	{
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		Log("Package {0}", Params.RawProjectPath);

		Log("Setting Emscripten SDK for packaging..");
		HTML5SDKInfo.SetupEmscriptenTemp();
		HTML5SDKInfo.SetUpEmscriptenConfigFile();

		// ----------------------------------------
		// target output
		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");
		if (!Directory.Exists(PackagePath))
		{
			Directory.CreateDirectory(PackagePath);
		}

		// ----------------------------------------
		// ini configurations
		var ConfigCache = UnrealBuildTool.ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.HTML5);

		// Debug and Development builds are not compressed to:
		// - speed up iteration times
		// - ensure (IndexedDB) data are not cached/used
		// Shipping builds "can be":
		// - compressed
		// - (IndexedDB) cached
		if (Params.ClientConfigsToBuild[0].ToString() == "Shipping")
		{
			ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "Compressed", out Compressed);
			ConfigCache.GetBool("/Script/HTML5PlatformEditor.HTML5TargetSettings", "EnableIndexedDB", out enableIndexedDB);
		}
		Log("HTML5Platform.Automation: Compressed = "       + Compressed      );
		Log("HTML5Platform.Automation: EnableIndexedDB = "  + enableIndexedDB );

		string FinalDataLocation = Path.Combine(PackagePath, Params.ShortProjectName) + ".data";

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
			PakAutomation.CreateEmscriptenDataPackage(PackagePath, FinalDataLocation);

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
			string PythonPath = HTML5SDKInfo.Python();
			string PackagerPath = HTML5SDKInfo.EmscriptenPackager();

			using (new ScopedEnvVar("EM_CONFIG", HTML5SDKInfo.DOT_EMSCRIPTEN))
			{
				using (new PushedDirectory(Path.Combine(Params.BaseStageDirectory, "HTML5")))
				{
					string CmdLine = string.Format("\"{0}\" \"{1}\" --preload . --js-output=\"{1}.js\" --no-heap-copy", PackagerPath, FinalDataLocation);
					RunAndLog(CmdEnv, PythonPath, CmdLine);
				}
			}
		}

		// ----------------------------------------
		// copy the "Executable" to the package directory
		string ProjectGameExeFilename = Params.GetProjectExeForPlatform(UnrealTargetPlatform.HTML5).ToString();
		string GameBasename = Path.GetFileNameWithoutExtension(ProjectGameExeFilename);
		if (Params.ClientConfigsToBuild[0].ToString() != "Development")
		{
			GameBasename += "-HTML5-" + Params.ClientConfigsToBuild[0].ToString();
		}
		// no extension
		string GameBasepath = Path.GetDirectoryName(ProjectGameExeFilename);
		string FullGameBasePath = Path.Combine(GameBasepath, GameBasename);
		string FullPackageGameBasePath = Path.Combine(PackagePath, GameBasename);

		// with extension
		string GameExe = GameBasename + ".js"; // emscripten
		string FullGameExePath = Path.Combine(GameBasepath, GameExe);
		string FullPackageGameExePath = Path.Combine(PackagePath, GameExe);

		// ensure the ue4game binary exists, if applicable
		if (!FileExists_NoExceptions(FullGameExePath))
		{
			Log("Failed to find game application " + FullGameExePath);
			throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find application {0}. You may need to build the UE4 project with your target configuration and platform.", FullGameExePath);
		}

		if (FullGameExePath != FullPackageGameExePath) // TODO: remove this check
		{
			File.Copy(FullGameExePath + ".symbols", FullPackageGameExePath + ".symbols", true);
			File.Copy(FullGameBasePath + ".wasm", FullPackageGameBasePath + ".wasm", true);
			File.Copy(FullGameExePath, FullPackageGameExePath, true);
		}

		File.SetAttributes(FullPackageGameExePath + ".symbols", FileAttributes.Normal);
		File.SetAttributes(FullPackageGameBasePath + ".wasm", FileAttributes.Normal);
		File.SetAttributes(FullPackageGameExePath, FileAttributes.Normal);

		// ----------------------------------------
		// generate HTML files to the package directory

		// ini setting
		string CanvasScaleMode;
		ConfigCache.GetString("/Script/HTML5PlatformEditor.HTML5TargetSettings", "CanvasScalingMode", out CanvasScaleMode);

		// output base name
		string OutputFile = Path.Combine(PackagePath,
				(Params.ClientConfigsToBuild[0].ToString() != "Development" ?
				 (Params.ShortProjectName + "-HTML5-" + Params.ClientConfigsToBuild[0].ToString()) :
				 Params.ShortProjectName)); // + ".html";

		// custom HTML, JS (if any), and CSS (if any) template files
		string BuildPath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Build", "HTML5");
		string TemplateFile = CombinePaths(BuildPath, "GameX.html.template");
		if ( !File.Exists(TemplateFile) )
		{
			// fall back to default UE4 template files
			BuildPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5");
			TemplateFile = CombinePaths(BuildPath, "GameX.html.template");
		}
		GenerateFileFromTemplate(TemplateFile,
				OutputFile + ".html",
				Params.ShortProjectName,
				Params.ClientConfigsToBuild[0].ToString(),
				!Params.IsCodeBasedProject,
				HTML5SDKInfo.HeapSize(ConfigCache, Params.ClientConfigsToBuild[0].ToString()),
				CanvasScaleMode
			);
		TemplateFile = CombinePaths(BuildPath, "GameX.js.template");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile,
					OutputFile + ".UE4.js",
					Params.ShortProjectName,
					Params.ClientConfigsToBuild[0].ToString(),
					!Params.IsCodeBasedProject,
					HTML5SDKInfo.HeapSize(ConfigCache, Params.ClientConfigsToBuild[0].ToString()),
					CanvasScaleMode
				);
		}
		TemplateFile = CombinePaths(BuildPath, "GameX.css.template");
		if ( File.Exists(TemplateFile) )
		{
			GenerateFileFromTemplate(TemplateFile,
					OutputFile + ".css",
					Params.ShortProjectName,
					Params.ClientConfigsToBuild[0].ToString(),
					!Params.IsCodeBasedProject,
					HTML5SDKInfo.HeapSize(ConfigCache, Params.ClientConfigsToBuild[0].ToString()),
					CanvasScaleMode
				);
		}

		// ----------------------------------------
		// (development) support files
		string MacBashTemplateFile = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5/RunMacHTML5LaunchHelper.command.template");
		string MacBashOutputFile = Path.Combine(PackagePath, "RunMacHTML5LaunchHelper.command");
		string MonoPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/SetupMono.sh");
		GenerateMacCommandFromTemplate(MacBashTemplateFile, MacBashOutputFile, MonoPath);

		string htaccessTemplate = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5/htaccess.template");
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
		File.Copy(htaccessTemplate, htaccesspath, true);

		// ----------------------------------------
		// final copies
		string JSDir = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5");
		string OutDir = PackagePath;

		// Gather utlity .js files and combine into one file
		string[] UtilityJavaScriptFiles = Directory.GetFiles(JSDir, "*.js");

		string DestinationFile = OutDir + "/Utility.js";
		File.Delete(DestinationFile);
		foreach( var UtilityFile in UtilityJavaScriptFiles)
		{
			string Data = File.ReadAllText(UtilityFile);
			File.AppendAllText(DestinationFile, Data);
		}

		if (Compressed)
		{
			Log("Build configuration is " + Params.ClientConfigsToBuild[0].ToString() + ", so (gzip) compressing files for web servers.");

			// Compress all files. These are independent tasks which can be threaded.
			List<Task> CompressionTasks = new List<Task>();

			// DATA file
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FinalDataLocation, FinalDataLocation + "gz")));

			// DATA file .js driver (emscripten)
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FinalDataLocation + ".js" , FinalDataLocation + ".jsgz")));

			// main game code
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FullPackageGameBasePath + ".wasm", FullPackageGameBasePath + ".wasmgz")));
			// main js (emscripten)
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FullPackageGameExePath, FullPackageGameExePath + "gz")));

			// symbols fil.
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FullPackageGameExePath + ".symbols", FullPackageGameExePath + ".symbolsgz")));

			// Utility
			CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(OutDir + "/Utility.js", OutDir + "/Utility.jsgz")));

			// UE4 js
			if ( File.Exists(FullPackageGameBasePath + ".UE4.js") )
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FullPackageGameBasePath + ".UE4.js" , FinalDataLocation + ".UE4.jsgz")));
			}

			// UE4 css
			if ( File.Exists(FullPackageGameBasePath + ".css") )
			{
				CompressionTasks.Add(Task.Factory.StartNew(() => CompressFile(FullPackageGameBasePath + ".css" , FinalDataLocation + ".cssgz")));
			}

			Task.WaitAll(CompressionTasks.ToArray());
		}
		else
		{
			Log("Build configuration is " + Params.ClientConfigsToBuild[0].ToString() + ", so not compressing. Build Shipping configuration to compress files to save space.");

			// nuke old compressed files to prevent using stale files
			File.Delete(FinalDataLocation + "gz");
			File.Delete(FinalDataLocation + ".jsgz");
			File.Delete(FullPackageGameExePath + "gz");
			File.Delete(FullPackageGameBasePath + ".wasmgz");
			File.Delete(FullPackageGameExePath + ".symbolsgz");
			File.Delete(OutDir + "/Utility.jsgz");
			File.Delete(FullPackageGameBasePath + ".UE4.jsgz");
			File.Delete(FullPackageGameBasePath + ".cssgz");
		}

		File.Copy(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/HTML5LaunchHelper.exe"),CombinePaths(OutDir, "HTML5LaunchHelper.exe"),true);
//		Task.WaitAll(CompressionTasks);
		PrintRunTime();
	}

	void CompressFile(string Source, string Destination)
	{
		Log(" Compressing " + Source);
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

	protected void GenerateFileFromTemplate(string InTemplateFile, string InOutputFile, string InGameName, string InGameConfiguration, bool IsContentOnly, int HeapSize, string CanvasScaleMode)
	{
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

				if (LineStr.Contains("%GAME%"))
				{
					LineStr = LineStr.Replace("%GAME%", InGameName);
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

				if (LineStr.Contains("%HEAPSIZE%"))
				{
					LineStr = LineStr.Replace("%HEAPSIZE%", HeapSize.ToString() + " * 1024 * 1024");
				}

				if (LineStr.Contains("%CONFIG%"))
				{
					string TempGameName = InGameName;
					if (IsContentOnly)
						TempGameName = "UE4Game";
					LineStr = LineStr.Replace("%CONFIG%", (InGameConfiguration != "Development" ? (TempGameName + "-HTML5-" + InGameConfiguration) : TempGameName));
				}

				if (LineStr.Contains("%UE4CMDLINE%"))
				{
					string ArgumentString = "'../../../" + InGameName + "/" + InGameName + ".uproject',";
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

		string PackagePath = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", "HTML5");
		string ProjectDataName = Params.ShortProjectName + ".data";

		// copy the "Executable" to the archive directory
		string GameBasename = Path.GetFileNameWithoutExtension(Params.GetProjectExeForPlatform(UnrealTargetPlatform.HTML5).ToString());
		if (Params.ClientConfigsToBuild[0].ToString() != "Development")
		{
			GameBasename += "-HTML5-" + Params.ClientConfigsToBuild[0].ToString();
		}
		string GameExe = GameBasename + ".js"; // emscripten

		// put the HTML file to the package directory
		string OutputFilename = (Params.ClientConfigsToBuild[0].ToString() != "Development" ?
				 (Params.ShortProjectName + "-HTML5-" + Params.ClientConfigsToBuild[0].ToString()) :
				 Params.ShortProjectName) + ".html";

		// DATA file
		SC.ArchiveFiles(PackagePath, ProjectDataName);
		// DATA file js driver (emscripten)
		SC.ArchiveFiles(PackagePath, ProjectDataName + ".js");
		// MAIN game code
		SC.ArchiveFiles(PackagePath, GameBasename + ".wasm");
		// MAIN js file (emscripten)
		SC.ArchiveFiles(PackagePath, GameExe);
		// symbols file
		SC.ArchiveFiles(PackagePath, GameExe + ".symbols");
		// utilities
		SC.ArchiveFiles(PackagePath, "Utility.js");
		// UE4 js file
		SC.ArchiveFiles(PackagePath, GameBasename + ".UE4.js");
		// UE4 css file
		SC.ArchiveFiles(PackagePath, GameBasename + ".css");
		// landing page.
		SC.ArchiveFiles(PackagePath, OutputFilename);

		// Archive HTML5 Server and a Readme.
		SC.ArchiveFiles(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/"), "HTML5LaunchHelper.exe");
		SC.ArchiveFiles(CombinePaths(CmdEnv.LocalRoot, "Engine/Build/HTML5/"), "Readme.txt");
		SC.ArchiveFiles(PackagePath, "RunMacHTML5LaunchHelper.command");
		SC.ArchiveFiles(PackagePath, ".htaccess");

		if (Compressed)
		{
			SC.ArchiveFiles(PackagePath, ProjectDataName + "gz");
			SC.ArchiveFiles(PackagePath, ProjectDataName + ".jsgz");
			SC.ArchiveFiles(PackagePath, GameExe + "gz");
			SC.ArchiveFiles(PackagePath, GameBasename + ".wasmgz");
			SC.ArchiveFiles(PackagePath, GameExe + ".symbolsgz");
			SC.ArchiveFiles(PackagePath, "Utility.jsgz");
			SC.ArchiveFiles(PackagePath, GameBasename + ".UE4.jsgz");
			SC.ArchiveFiles(PackagePath, GameBasename + ".cssgz");
		}
		else
		{
			// nuke old compressed files to prevent using stale files
			File.Delete(ProjectDataName + "gz");
			File.Delete(ProjectDataName + ".jsgz");
			File.Delete(GameExe + "gz");
			File.Delete(GameBasename + ".wasmgz");
			File.Delete(GameExe + ".symbolsgz");
			File.Delete("Utility.jsgz");
			File.Delete(GameBasename + ".UE4.jsgz");
			File.Delete(GameBasename + ".cssgz");
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

		UploadToS3(SC, OutputFilename);
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

		// WARNING: splitting the following situation
		// if cookonthefly is used: tell the browser to use the PROXY at DEFAULT_HTTP_FILE_SERVING_PORT
		// leave the normal HTML5LaunchHelper port (ServerPort) alone -- otherwise it will collide with the PROXY port
		if (ClientCmdLine.Contains("filehostip"))
		{
			url += "?cookonthefly=true";
			Int32 ProxyPort = 41898; // DEFAULT_HTTP_FILE_SERVING_PORT
			url = String.Format("http://localhost:{0}/{1}", ProxyPort, url);
		}
		else
		{
			url = String.Format("http://localhost:{0}/{1}", ServerPort, url);
		}

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
			Log("Amazon S3 Incorrectly configured");
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
		Log("Upload Timeout set to: " + (msTimeOut/1000) + "secs");
		Task.WaitAll(UploadTasks.ToArray(), (int)msTimeOut); // set timeout [miliseconds]

		string URL = "https://" + BucketName + ".s3.amazonaws.com/" + FolderName + "/" + OutputFilename;
		Log("Your project's shareable link is: " + URL);

		Log("Upload Tasks finished.");
	}

	private static IDictionary<string, string> MimeTypeMapping = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase)
		{
			// the following will default to: "appication/octet-stream"
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
		Log(" Uploading " + Info.Name);

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
			Log("Upload done: " + Info.Name);
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
