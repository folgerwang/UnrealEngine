// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;
using System.Security.Cryptography.X509Certificates;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about how a local directory maps to a remote directory
	/// </summary>
	[DebuggerDisplay("{LocalDirectory}")]
	class RemoteMapping
	{
		public DirectoryReference LocalDirectory;
		public string RemoteDirectory;

		public RemoteMapping(DirectoryReference LocalDirectory, string RemoteDirectory)
		{
			this.LocalDirectory = LocalDirectory;
			this.RemoteDirectory = RemoteDirectory;
		}
	}

	/// <summary>
	/// Handles uploading and building on a remote Mac
	/// </summary>
	class RemoteMac
	{
		/// <summary>
		/// These two variables will be loaded from XML config file in XmlConfigLoader.Init()
		/// </summary>
		[XmlConfigFile]
		private readonly string ServerName;

		/// <summary>
		/// The remote username
		/// </summary>
		[XmlConfigFile]
		private readonly string UserName;

		/// <summary>
		/// Instead of looking for RemoteToolChainPrivate.key in the usual places (Documents/Unreal Engine/UnrealBuildTool/SSHKeys, Engine/Build/SSHKeys), this private key will be used if set
		/// </summary>
		[XmlConfigFile]
		private FileReference SshPrivateKey;

		/// <summary>
		/// The authentication used for Rsync (for the -e rsync flag)
		/// </summary>
		[XmlConfigFile]
		private string RsyncAuthentication = "ssh -i '${CYGWIN_SSH_PRIVATE_KEY}'";

		/// <summary>
		/// The authentication used for SSH (probably similar to RsyncAuthentication)
		/// </summary>
		[XmlConfigFile]
		private string SshAuthentication = "-i '${CYGWIN_SSH_PRIVATE_KEY}'";

		/// <summary>
		/// Save the specified port so that RemoteServerName is the machine address only
		/// </summary>
		private readonly int ServerPort = 22;	// Default ssh port

		/// <summary>
		/// Path to Rsync
		/// </summary>
		private readonly FileReference RsyncExe;

		/// <summary>
		/// Path to SSH
		/// </summary>
		private readonly FileReference SshExe;

		/// <summary>
		/// The project being built. Settings will be read from config files in this project.
		/// </summary>
		private readonly FileReference ProjectFile;

		/// <summary>
		/// The directory containing the project being built.
		/// </summary>
		private readonly DirectoryReference ProjectDirectory;

		/// <summary>
		/// The base directory on the remote machine
		/// </summary>
		private string RemoteBaseDir;

		/// <summary>
		/// Mappings from local directories to remote directories
		/// </summary>
		private List<RemoteMapping> Mappings;

		/// <summary>
		/// Arguments that are used by every Ssh call
		/// </summary>
		private List<string> CommonSshArguments;

		/// <summary>
		/// Arguments that are used by every Rsync call
		/// </summary>
		private List<string> CommonRsyncArguments;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">Project to read settings from</param>
		public RemoteMac(FileReference ProjectFile)
		{
			this.RsyncExe = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Extras", "ThirdPartyNotUE", "DeltaCopy", "Binaries", "Rsync.exe");
			this.SshExe = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Extras", "ThirdPartyNotUE", "DeltaCopy", "Binaries", "Ssh.exe");
			this.ProjectFile = ProjectFile;
			this.ProjectDirectory = DirectoryReference.FromFile(ProjectFile);

			// Apply settings from the XML file
			XmlConfig.ApplyTo(this);

			// Get the project config file path
			DirectoryReference EngineIniPath = ProjectFile != null ? ProjectFile.Directory : null;
			if (EngineIniPath == null && UnrealBuildTool.GetRemoteIniPath() != null)
			{
				EngineIniPath = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath());
			}
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, EngineIniPath, UnrealTargetPlatform.IOS);

			// Read the project settings if we don't have anything in the build configuration settings
			if(String.IsNullOrEmpty(ServerName))
			{
				// Read the server name
				string IniServerName;
				if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "RemoteServerName", out IniServerName) && !String.IsNullOrEmpty(IniServerName))
				{
					this.ServerName = IniServerName;
				}
				else
				{
					throw new BuildException("Remote compiling requires a server name. Use the editor (Project Settings > IOS) to set up your remote compilation settings.");
				}

				// Parse the username
				string IniUserName;
				if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "RSyncUsername", out IniUserName) && !String.IsNullOrEmpty(IniUserName))
				{
					this.UserName = IniUserName;
				}
			}

			// Split port out from the server name
			int PortIdx = ServerName.LastIndexOf(':');
			if(PortIdx != -1)
			{
				string Port = ServerName.Substring(PortIdx + 1);
				if(!int.TryParse(Port, out ServerPort))
				{
					throw new BuildException("Unable to parse port number from '{0}'", ServerName);
				}
				ServerName = ServerName.Substring(0, PortIdx);
			}

			// If a user name is not set, use the current user
			if (String.IsNullOrEmpty(UserName))
			{
				UserName = Environment.UserName;
			}

			// Print out the server info
			Log.TraceInformation("[Remote] Using remote server '{0}' on port {1} (user '{2}')", ServerName, ServerPort, UserName);

			// Get the path to the SSH private key
			string OverrideSshPrivateKeyPath;
			if (Ini.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "SSHPrivateKeyOverridePath", out OverrideSshPrivateKeyPath) && !String.IsNullOrEmpty(OverrideSshPrivateKeyPath))
			{
				SshPrivateKey = new FileReference(OverrideSshPrivateKeyPath);
				if (!FileReference.Exists(SshPrivateKey))
				{
					throw new BuildException("SSH private key specified in config file ({0}) does not exist.", SshPrivateKey);
				}
			}

			// If it's not set, look in the standard locations. If that fails, spawn the batch file to generate one.
			if (SshPrivateKey == null && !TryGetSshPrivateKey(out SshPrivateKey))
			{
				Log.TraceWarning("No SSH private key found for {0}@{1}. Launching SSH to generate one.", UserName, ServerName);

				StringBuilder CommandLine = new StringBuilder();
				CommandLine.AppendFormat("/C \"\"{0}\"", FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "BatchFiles", "MakeAndInstallSSHKey.bat"));
				CommandLine.AppendFormat(" \"{0}\"", SshExe);
				CommandLine.AppendFormat(" \"{0}\"", ServerPort);
				CommandLine.AppendFormat(" \"{0}\"", RsyncExe);
				CommandLine.AppendFormat(" \"{0}\"", UserName);
				CommandLine.AppendFormat(" \"{0}\"", ServerName);
				CommandLine.AppendFormat(" \"{0}\"", DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.MyDocuments));
				CommandLine.AppendFormat(" \"{0}\"", GetLocalCygwinPath(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.MyDocuments)));
				CommandLine.AppendFormat(" \"{0}\"", UnrealBuildTool.EngineDirectory);
				CommandLine.Append("\"");

				using(Process ChildProcess = Process.Start("C:\\Windows\\System32\\Cmd.exe", CommandLine.ToString()))
				{
					ChildProcess.WaitForExit();
				}

				if(!TryGetSshPrivateKey(out SshPrivateKey))
				{
					throw new BuildException("Failed to generate SSH private key for {0}@{1}.", UserName, ServerName);
				}
			}

			// resolve the rest of the strings
			RsyncAuthentication = ExpandVariables(RsyncAuthentication);
			SshAuthentication = ExpandVariables(SshAuthentication);

			// Get the remote base directory
			RemoteBaseDir = String.Format("/Users/{0}/UE4/Builds/{1}", UserName, Environment.MachineName);

			// Build the list of directory mappings between the local and remote machines
			Mappings = new List<RemoteMapping>();
			Mappings.Add(new RemoteMapping(UnrealBuildTool.EngineDirectory, GetRemotePath(UnrealBuildTool.EngineDirectory)));
			if(ProjectFile != null && !ProjectFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
			{
				Mappings.Add(new RemoteMapping(ProjectFile.Directory, GetRemotePath(ProjectFile.Directory)));
			}

			// Build a list of arguments for SSH
			CommonSshArguments = new List<string>();
			CommonSshArguments.Add("-o BatchMode=yes");
			CommonSshArguments.Add(SshAuthentication);
			CommonSshArguments.Add(String.Format("-p {0}", ServerPort));
			CommonSshArguments.Add(String.Format("\"{0}@{1}\"", UserName, ServerName));

			// Build a list of arguments for Rsync
			CommonRsyncArguments = new List<string>();
			CommonRsyncArguments.Add("--compress");
			CommonRsyncArguments.Add("--recursive");
			CommonRsyncArguments.Add("--delete"); // Delete anything not in the source directory
			CommonRsyncArguments.Add("--delete-excluded"); // Delete anything not in the source directory
			CommonRsyncArguments.Add("--times"); // Preserve modification times
			CommonRsyncArguments.Add("--verbose");
			CommonRsyncArguments.Add("-m");
			CommonRsyncArguments.Add("--chmod=ug=rwX,o=rxX");
			CommonRsyncArguments.Add(String.Format("--rsh=\"{0} -p {1}\"", RsyncAuthentication, ServerPort));
		}

		/// <summary>
		/// Attempts to get the SSH private key from the standard locations
		/// </summary>
		/// <param name="OutPrivateKey">If successful, receives the location of the private key that was found</param>
		/// <returns>True if a private key was found, false otherwise</returns>
		private bool TryGetSshPrivateKey(out FileReference OutPrivateKey)
		{
			// Build a list of all the places to look for a private key
			List<DirectoryReference> Locations = new List<DirectoryReference>();
			Locations.Add(DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ApplicationData), "Unreal Engine", "UnrealBuildTool"));
			Locations.Add(DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.Personal), "Unreal Engine", "UnrealBuildTool"));
			if (ProjectFile != null)
			{
				Locations.Add(DirectoryReference.Combine(ProjectFile.Directory, "Build", "NotForLicensees"));
				Locations.Add(DirectoryReference.Combine(ProjectFile.Directory, "Build", "NoRedist"));
				Locations.Add(DirectoryReference.Combine(ProjectFile.Directory, "Build"));
			}
			Locations.Add(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "NotForLicensees"));
			Locations.Add(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "NoRedist"));
			Locations.Add(DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Build"));

			// Find the first that exists
			foreach (DirectoryReference Location in Locations)
			{
				FileReference KeyFile = FileReference.Combine(Location, "SSHKeys", ServerName, UserName, "RemoteToolChainPrivate.key");
				if (FileReference.Exists(KeyFile))
				{
					OutPrivateKey = KeyFile;
					return true;
				}
			}

			// Nothing found
			OutPrivateKey = null;
			return false;
		}

		/// <summary>
		/// Expand all the variables in the given string
		/// </summary>
		/// <param name="Input">The input string</param>
		/// <returns>String with any variables expanded</returns>
		private string ExpandVariables(string Input)
		{
			string Result = Input;
			Result = Result.Replace("${SSH_PRIVATE_KEY}", SshPrivateKey.FullName);
			Result = Result.Replace("${CYGWIN_SSH_PRIVATE_KEY}", GetLocalCygwinPath(SshPrivateKey));
			return Result;
		}

		/// <summary>
		/// Flush the remote machine, removing all existing files
		/// </summary>
		public void FlushRemote()
		{
			Log.TraceInformation("[Remote] Deleting all files under {0}...", RemoteBaseDir);
			Execute("/", String.Format("rm -rf \"{0}\"", RemoteBaseDir));
		}

		/// <summary>
		/// Build a target remotely
		/// </summary>
		/// <param name="TargetDesc">Descriptor for the target to build</param>
		/// <param name="RemoteLogFile">Path to store the remote log file</param>
		/// <returns>True if the build succeeded, false otherwise</returns>
		public bool Build(TargetDescriptor TargetDesc, FileReference RemoteLogFile)
		{
			// Get the directory for working files
			DirectoryReference BaseDir = DirectoryReference.FromFile(TargetDesc.ProjectFile) ?? UnrealBuildTool.EngineDirectory;
			DirectoryReference TempDir = DirectoryReference.Combine(BaseDir, "Intermediate", "Remote", TargetDesc.Name, TargetDesc.Platform.ToString(), TargetDesc.Configuration.ToString());
			DirectoryReference.CreateDirectory(TempDir);

			// Compile the rules assembly
			RulesCompiler.CreateTargetRulesAssembly(TargetDesc.ProjectFile, TargetDesc.Name, false, false, TargetDesc.ForeignPlugin);

			// Path to the local manifest file. This has to be translated from the remote format after the build is complete.
			List<FileReference> LocalManifestFiles = new List<FileReference>();

			// Path to the remote manifest file
			FileReference RemoteManifestFile = FileReference.Combine(TempDir, "Manifest.xml");

			// Prepare the arguments we will pass to the remote build
			List<string> RemoteArguments = new List<string>();
			RemoteArguments.Add(TargetDesc.Name);
			RemoteArguments.Add(TargetDesc.Platform.ToString());
			RemoteArguments.Add(TargetDesc.Configuration.ToString());
			RemoteArguments.Add("-SkipRulesCompile"); // Use the rules assembly built locally
			RemoteArguments.Add("-ForceXmlConfigCache"); // Use the XML config cache built locally, since the remote won't have it
			RemoteArguments.Add(String.Format("-Log={0}", GetRemotePath(RemoteLogFile)));
			RemoteArguments.Add(String.Format("-Manifest={0}", GetRemotePath(RemoteManifestFile)));

			if(TargetDesc.ProjectFile != null)
			{
				RemoteArguments.Add(String.Format("-Project={0}", GetRemotePath(TargetDesc.ProjectFile)));
			}

			foreach(string LocalArgument in TargetDesc.AdditionalArguments)
			{
				int EqualsIdx = LocalArgument.IndexOf('=');
				if(EqualsIdx == -1)
				{
					RemoteArguments.Add(LocalArgument);
					continue;
				}

				string Key = LocalArgument.Substring(0, EqualsIdx);
				string Value = LocalArgument.Substring(EqualsIdx + 1);

				if(Key.Equals("-Log", StringComparison.InvariantCultureIgnoreCase))
				{
					// We are already writing to the local log file. The remote will produce a different log (RemoteLogFile)
					continue;
				}
				if(Key.Equals("-Manifest", StringComparison.InvariantCultureIgnoreCase))
				{
					LocalManifestFiles.Add(new FileReference(Value));
					continue;
				}

				string RemoteArgument = LocalArgument;
				foreach(RemoteMapping Mapping in Mappings)
				{
					if(Value.StartsWith(Mapping.LocalDirectory.FullName, StringComparison.InvariantCultureIgnoreCase))
					{
						RemoteArgument = String.Format("{0}={1}", Key, GetRemotePath(Value));
						break;
					}
				}
				RemoteArguments.Add(RemoteArgument);
			}

			// Handle any per-platform setup that is required
			if(TargetDesc.Platform == UnrealTargetPlatform.IOS || TargetDesc.Platform == UnrealTargetPlatform.TVOS)
			{
				// Always generate a .stub
				RemoteArguments.Add("-CreateStub");

				// Get the provisioning data for this project
				IOSProvisioningData ProvisioningData = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(TargetDesc.Platform)).ReadProvisioningData(TargetDesc.ProjectFile);
				if(ProvisioningData == null || ProvisioningData.MobileProvisionFile == null)
				{
					throw new BuildException("Unable to find mobile provision for {0}. See log for more information.", TargetDesc.Name);
				}

				// Create a local copy of the provision
				FileReference MobileProvisionFile = FileReference.Combine(TempDir, ProvisioningData.MobileProvisionFile.GetFileName());
				if(FileReference.Exists(MobileProvisionFile))
				{
					FileReference.SetAttributes(MobileProvisionFile, FileAttributes.Normal);
				}
				FileReference.Copy(ProvisioningData.MobileProvisionFile, MobileProvisionFile, true);
				Log.TraceInformation("[Remote] Uploading {0}", MobileProvisionFile);
				UploadFile(MobileProvisionFile);

				// Extract the certificate for the project
				FileReference CertificateFile = FileReference.Combine(TempDir, "Certificate.p12");
				if(!FileReference.Exists(CertificateFile))
				{
					StringBuilder Arguments = new StringBuilder("ExportCertificate");
					if(TargetDesc.ProjectFile == null)
					{
						Arguments.AppendFormat(" \"{0}\"", UnrealBuildTool.EngineSourceDirectory);
					}
					else
					{
						Arguments.AppendFormat(" \"{0}\"", TargetDesc.ProjectFile.Directory);
					}
					if(TargetDesc.Platform == UnrealTargetPlatform.TVOS)
					{
						Arguments.Append(" -tvos");
					}
					Arguments.AppendFormat(" -outputcertificate \"{0}\"", CertificateFile);
					Arguments.AppendFormat(" -provisioninguuid {0}", ProvisioningData.MobileProvisionUUID);

					ProcessStartInfo StartInfo = new ProcessStartInfo();
					StartInfo.FileName = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Binaries", "DotNET", "IOS", "IPhonePackager.exe").FullName;
					StartInfo.Arguments = Arguments.ToString();
					if(Utils.RunLocalProcessAndLogOutput(StartInfo) != 0)
					{
						throw new BuildException("IphonePackager failed.");
					}
				}

				// Upload the certificate to the remote
				Log.TraceInformation("[Remote] Uploading {0}", CertificateFile);
				UploadFile(CertificateFile);

				// Tell the remote UBT instance to use them
				RemoteArguments.Add(String.Format("-ImportProvision={0}", GetRemotePath(MobileProvisionFile)));
				RemoteArguments.Add(String.Format("-ImportCertificate={0}", GetRemotePath(CertificateFile)));
				RemoteArguments.Add(String.Format("-ImportCertificatePassword=A"));
			}

			// Upload the workspace files
			Log.TraceInformation("[Remote] Uploading workspace files");
			UploadWorkspace(TempDir);

			// Fixup permissions on any shell scripts
			Execute(RemoteBaseDir, String.Format("chmod +x {0}/Build/BatchFiles/Mac/*.sh", EscapeShellArgument(GetRemotePath(UnrealBuildTool.EngineDirectory))));

			// Execute the compile
			Log.TraceInformation("[Remote] Executing build");

			StringBuilder BuildCommandLine = new StringBuilder("Engine/Build/BatchFiles/Mac/Build.sh");
			foreach(string RemoteArgument in RemoteArguments)
			{
				BuildCommandLine.AppendFormat(" {0}", EscapeShellArgument(RemoteArgument));
			}

			int Result = Execute(GetRemotePath(UnrealBuildTool.RootDirectory), BuildCommandLine.ToString());
			if(Result != 0)
			{
				if(RemoteLogFile != null)
				{
					Log.TraceInformation("[Remote] Downloading {0}", RemoteLogFile);
					DownloadFile(RemoteLogFile);
				}
				return false;
			}

			// Download the manifest
			Log.TraceInformation("[Remote] Downloading {0}", RemoteManifestFile);
			DownloadFile(RemoteManifestFile);

			// Convert the manifest to local form
			BuildManifest Manifest = Utils.ReadClass<BuildManifest>(RemoteManifestFile.FullName);
			for(int Idx = 0; Idx < Manifest.BuildProducts.Count; Idx++)
			{
				Manifest.BuildProducts[Idx] = GetLocalPath(Manifest.BuildProducts[Idx]).FullName;
			}

			// Download the files from the remote
			if(TargetDesc.AdditionalArguments.Any(x => x.Equals("-GenerateManifest", StringComparison.InvariantCultureIgnoreCase)))
			{
				LocalManifestFiles.Add(FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", "Manifest.xml"));
			}
			else
			{
				Log.TraceInformation("[Remote] Downloading build products");

				List<FileReference> FilesToDownload = new List<FileReference>();
				FilesToDownload.Add(RemoteLogFile);
				FilesToDownload.AddRange(Manifest.BuildProducts.Select(x => new FileReference(x)));
				DownloadFiles(FilesToDownload);
			}

			// Write out all the local manifests
			foreach(FileReference LocalManifestFile in LocalManifestFiles)
			{
				Log.TraceInformation("[Remote] Writing {0}", LocalManifestFile);
				Utils.WriteClass<BuildManifest>(Manifest, LocalManifestFile.FullName, "");
			}
			return true;
		}

		/// <summary>
		/// Runs the actool utility on a directory to create an Assets.car file
		/// </summary>
		/// <param name="Platform">The target platform</param>
		/// <param name="InputDir">Input directory containing assets</param>
		/// <param name="OutputFile">Path to the Assets.car file to produce</param>
		public void RunAssetCatalogTool(CppPlatform Platform, DirectoryReference InputDir, FileReference OutputFile)
		{
			Log.TraceInformation("Running asset catalog tool for {0}: {1} -> {2}", Platform, InputDir, OutputFile);

			string RemoteInputDir = GetRemotePath(InputDir);
			UploadDirectory(InputDir);

			string RemoteOutputFile = GetRemotePath(OutputFile);
			Execute(RemoteBaseDir, String.Format("rm -f {0}", EscapeShellArgument(RemoteOutputFile)));

			string RemoteOutputDir = Path.GetDirectoryName(RemoteOutputFile).Replace(Path.DirectorySeparatorChar, '/');
			Execute(RemoteBaseDir, String.Format("mkdir -p {0}", EscapeShellArgument(RemoteOutputDir)));

			string RemoteArguments = IOSToolChain.GetAssetCatalogArgs(Platform, RemoteInputDir, RemoteOutputDir); 
			if(Execute(RemoteBaseDir, String.Format("/usr/bin/xcrun {0}", RemoteArguments)) != 0)
			{
				throw new BuildException("Failed to run actool.");
			}
			DownloadFile(OutputFile);
		}

		/// <summary>
		/// Convers a remote path into local form
		/// </summary>
		/// <param name="RemotePath">The remote filename</param>
		/// <returns>Local filename corresponding to the remote path</returns>
		private FileReference GetLocalPath(string RemotePath)
		{
			foreach(RemoteMapping Mapping in Mappings)
			{
				if(RemotePath.StartsWith(Mapping.RemoteDirectory, StringComparison.InvariantCultureIgnoreCase) && RemotePath.Length > Mapping.RemoteDirectory.Length && RemotePath[Mapping.RemoteDirectory.Length] == '/')
				{
					return FileReference.Combine(Mapping.LocalDirectory, RemotePath.Substring(Mapping.RemoteDirectory.Length + 1));
				}
			}
			throw new BuildException("Unable to map remote path '{0}' to local path", RemotePath);
		}

		/// <summary>
		/// Converts a local path into a remote one
		/// </summary>
		/// <param name="LocalPath">The local path to convert</param>
		/// <returns>Equivalent remote path</returns>
		private string GetRemotePath(FileSystemReference LocalPath)
		{
			return GetRemotePath(LocalPath.FullName);
		}

		/// <summary>
		/// Converts a local path into a remote one
		/// </summary>
		/// <param name="LocalPath">The local path to convert</param>
		/// <returns>Equivalent remote path</returns>
		private string GetRemotePath(string LocalPath)
		{
			return String.Format("{0}/{1}", RemoteBaseDir, LocalPath.Replace(":", "").Replace("\\", "/").Replace(" ", "_"));
		}

		/// <summary>
		/// Gets the local path in Cygwin format (eg. /cygdrive/C/...)
		/// </summary>
		/// <param name="InPath">Local path</param>
		/// <returns>Path in cygwin format</returns>
		private static string GetLocalCygwinPath(FileSystemReference InPath)
		{
			if(InPath.FullName.Length < 2 || InPath.FullName[1] != ':')
			{
				throw new BuildException("Invalid local path for converting to cygwin format ({0}).", InPath);
			}
			return String.Format("/cygdrive/{0}{1}", InPath.FullName.Substring(0, 1), InPath.FullName.Substring(2).Replace('\\', '/'));
		}

		/// <summary>
		/// Escapes spaces in a shell command argument
		/// </summary>
		/// <param name="Argument">The argument to escape</param>
		/// <returns>The escaped argument</returns>
		private static string EscapeShellArgument(string Argument)
		{
			return Argument.Replace(" ", "\\ ");
		}

		/// <summary>
		/// Upload a single file to the remote
		/// </summary>
		/// <param name="LocalFile">The file to upload</param>
		void UploadFile(FileReference LocalFile)
		{
			string RemoteFile = GetRemotePath(LocalFile);
			string RemoteDirectory = GetRemotePath(LocalFile.Directory);

			List<string> Arguments = new List<string>(CommonRsyncArguments);
			Arguments.Add(String.Format("--rsync-path=\"mkdir -p {0} && rsync\"", RemoteDirectory));
			Arguments.Add(String.Format("\"{0}\"", GetLocalCygwinPath(LocalFile)));
			Arguments.Add(String.Format("\"{0}@{1}\":'{2}/'", UserName, ServerName, RemoteDirectory));
			Arguments.Add("-q");

			int Result = Rsync(String.Join(" ", Arguments));
			if(Result != 0)
			{
				throw new BuildException("Error while running Rsync (exit code {0})", Result);
			}
		}

		/// <summary>
		/// Upload a single directory to the remote
		/// </summary>
		/// <param name="LocalDirectory">The local directory to upload</param>
		void UploadDirectory(DirectoryReference LocalDirectory)
		{
			string RemoteDirectory = GetRemotePath(LocalDirectory);

			List<string> Arguments = new List<string>(CommonRsyncArguments);
			Arguments.Add(String.Format("--rsync-path=\"mkdir -p {0} && rsync\"", RemoteDirectory));
			Arguments.Add(String.Format("\"{0}/\"", GetLocalCygwinPath(LocalDirectory)));
			Arguments.Add(String.Format("\"{0}@{1}\":'{2}/'", UserName, ServerName, RemoteDirectory));
			Arguments.Add("-q");

			int Result = Rsync(String.Join(" ", Arguments));
			if(Result != 0)
			{
				throw new BuildException("Error while running Rsync (exit code {0})", Result);
			}
		}

		/// <summary>
		/// Uploads a directory to the remote using a specific filter list
		/// </summary>
		/// <param name="LocalDirectory">The local directory to copy from</param>
		/// <param name="RemoteDirectory">The remote directory to copy to</param>
		/// <param name="FilterLocations">List of paths to filter</param>
		void UploadDirectory(DirectoryReference LocalDirectory, string RemoteDirectory, List<FileReference> FilterLocations)
		{
			List<string> Arguments = new List<string>(CommonRsyncArguments);
			Arguments.Add(String.Format("--rsync-path=\"mkdir -p {0} && rsync\"", RemoteDirectory));
			foreach(FileReference FilterLocation in FilterLocations)
			{
				Arguments.Add(String.Format("--filter=\"merge {0}\"", GetLocalCygwinPath(FilterLocation)));
			}
			Arguments.Add("--exclude='*'");
			Arguments.Add(String.Format("\"{0}/\"", GetLocalCygwinPath(LocalDirectory)));
			Arguments.Add(String.Format("\"{0}@{1}\":'{2}/'", UserName, ServerName, RemoteDirectory));

			int Result = Rsync(String.Join(" ", Arguments));
			if(Result != 0)
			{
				throw new BuildException("Error while running Rsync (exit code {0})", Result);
			}
		}

		/// <summary>
		/// Upload all the files in the workspace for the current project
		/// </summary>
		void UploadWorkspace(DirectoryReference TempDir)
		{
			// Path to the scripts to be uploaded
			FileReference ScriptPathsFileName = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Rsync", "RsyncEngineScripts.txt");

			// Read the list of scripts to be uploaded
			List<string> ScriptPaths = new List<string>();
			foreach(string Line in FileReference.ReadAllLines(ScriptPathsFileName))
			{
				string FileToUpload = Line.Trim();
				if(FileToUpload.Length > 0 && FileToUpload[0] != ';')
				{
					ScriptPaths.Add(FileToUpload);
				}
			}

			// Fixup the line endings
			List<FileReference> TargetFiles = new List<FileReference>();
			foreach(string ScriptPath in ScriptPaths)
			{
				FileReference SourceFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, ScriptPath.TrimStart('/'));
				if(!FileReference.Exists(SourceFile))
				{
					throw new BuildException("Missing script required for remote upload: {0}", SourceFile);
				}

				FileReference TargetFile = FileReference.Combine(TempDir, SourceFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory));
				if(!FileReference.Exists(TargetFile) || FileReference.GetLastWriteTimeUtc(TargetFile) < FileReference.GetLastWriteTimeUtc(SourceFile))
				{
					DirectoryReference.CreateDirectory(TargetFile.Directory);
					string ScriptText = FileReference.ReadAllText(SourceFile);
					FileReference.WriteAllText(TargetFile, ScriptText.Replace("\r\n", "\n"));
				}
				TargetFiles.Add(TargetFile);
			}

			// Write a file that protects all the scripts from being overridden by the standard engine filters
			FileReference ScriptUploadList = FileReference.Combine(TempDir, "RsyncEngineScripts-Upload.txt");
			using(StreamWriter Writer = new StreamWriter(ScriptUploadList.FullName))
			{
				foreach(string ScriptPath in ScriptPaths)
				{
					for(int SlashIdx = ScriptPath.IndexOf('/', 1); SlashIdx != -1; SlashIdx = ScriptPath.IndexOf('/', SlashIdx + 1))
					{
						Writer.WriteLine("+ {0}", ScriptPath.Substring(0, SlashIdx));
					}
					Writer.WriteLine("+ {0}", ScriptPath);
				}
				Writer.WriteLine("protect *");
			}

			// Write a file that protects all the scripts from being overridden by the standard engine filters
			FileReference ScriptProtectList = FileReference.Combine(TempDir, "RsyncEngineScripts-Protect.txt");
			using(StreamWriter Writer = new StreamWriter(ScriptProtectList.FullName))
			{
				foreach(string ScriptPath in ScriptPaths)
				{
					Writer.WriteLine("protect {0}", ScriptPath);
				}
			}

			// Upload these files to the remote
			List<FileReference> FilterLocations = new List<FileReference>();
			FilterLocations.Add(ScriptUploadList);
			UploadDirectory(TempDir, GetRemotePath(UnrealBuildTool.EngineDirectory), FilterLocations);

			// Upload the engine files
			List<FileReference> EngineFilters = new List<FileReference>();
			EngineFilters.Add(ScriptProtectList);
			EngineFilters.Add(FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Rsync", "RsyncEngine.txt"));
			UploadDirectory(UnrealBuildTool.EngineDirectory, GetRemotePath(UnrealBuildTool.EngineDirectory), EngineFilters);

			// Upload the project files
			if(ProjectFile != null && !ProjectFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
			{
				List<FileReference> ProjectFilters = new List<FileReference>();

				FileReference CustomProjectFilter = FileReference.Combine(ProjectFile.Directory, "Build", "Rsync", "RsyncProject.txt");
				if(FileReference.Exists(CustomProjectFilter))
				{
					ProjectFilters.Add(CustomProjectFilter);
				}
				ProjectFilters.Add(FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Rsync", "RsyncProject.txt"));

				UploadDirectory(ProjectFile.Directory, GetRemotePath(ProjectFile.Directory), ProjectFilters);
			}
		}

		/// <summary>
		/// Downloads a single file from the remote
		/// </summary>
		/// <param name="LocalFile">The file to download</param>
		void DownloadFile(FileReference LocalFile)
		{
			RemoteMapping Mapping = Mappings.FirstOrDefault(x => LocalFile.IsUnderDirectory(x.LocalDirectory));
			if(Mapping == null)
			{
				throw new BuildException("File for download '{0}' is not under any mapped directory.", LocalFile);
			}

			List<string> Arguments = new List<string>(CommonRsyncArguments);
			Arguments.Add(String.Format("\"{0}@{1}\":'{2}/{3}'", UserName, ServerName, Mapping.RemoteDirectory, LocalFile.MakeRelativeTo(Mapping.LocalDirectory).Replace('\\', '/')));
			Arguments.Add(String.Format("\"{0}/\"", GetLocalCygwinPath(LocalFile.Directory)));
			Arguments.Add("-q");

			int Result = Rsync(String.Join(" ", Arguments));
			if(Result != 0)
			{
				throw new BuildException("Unable to download '{0}' from the remote Mac (exit code {1}).", LocalFile, Result);
			}
		}

		/// <summary>
		/// Download multiple files from the remote Mac
		/// </summary>
		/// <param name="Files">List of local files to download</param>
		void DownloadFiles(IEnumerable<FileReference> Files)
		{
			List<FileReference>[] FileGroups = new List<FileReference>[Mappings.Count];
			for(int Idx = 0; Idx < Mappings.Count; Idx++)
			{
				FileGroups[Idx] = new List<FileReference>();
			}
			foreach(FileReference File in Files)
			{
				int MappingIdx = Mappings.FindIndex(x => File.IsUnderDirectory(x.LocalDirectory));
				if(MappingIdx == -1)
				{
					throw new BuildException("File for download '{0}' is not under the engine or project directory.", File);
				}
				FileGroups[MappingIdx].Add(File);
			}
			for(int Idx = 0; Idx < Mappings.Count; Idx++)
			{
				if(FileGroups[Idx].Count > 0)
				{
					FileReference DownloadListLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Rsync", "Download.txt");
					DirectoryReference.CreateDirectory(DownloadListLocation.Directory);
					FileReference.WriteAllLines(DownloadListLocation, FileGroups[Idx].Select(x => x.MakeRelativeTo(Mappings[Idx].LocalDirectory).Replace('\\', '/')));

					List<string> Arguments = new List<string>(CommonRsyncArguments);
					Arguments.Add(String.Format("--files-from=\"{0}\"", GetLocalCygwinPath(DownloadListLocation)));
					Arguments.Add(String.Format("\"{0}@{1}\":'{2}/'", UserName, ServerName, Mappings[Idx].RemoteDirectory));
					Arguments.Add(String.Format("\"{0}/\"", GetLocalCygwinPath(Mappings[Idx].LocalDirectory)));

					int Result = Rsync(String.Join(" ", Arguments));
					if(Result != 0)
					{
						throw new BuildException("Unable to download files from remote Mac (exit code {0})", Result);
					}
				}
			}
		}

		/// <summary>
		/// Execute Rsync
		/// </summary>
		/// <param name="Arguments">Arguments for the Rsync command</param>
		/// <returns>Exit code from Rsync</returns>
		private int Rsync(string Arguments)
		{
			using(Process RsyncProcess = new Process())
			{
				RsyncProcess.StartInfo.FileName = RsyncExe.FullName;
				RsyncProcess.StartInfo.Arguments = Arguments;
				RsyncProcess.StartInfo.WorkingDirectory = SshExe.Directory.FullName;
				RsyncProcess.OutputDataReceived += RsyncOutput;
				RsyncProcess.ErrorDataReceived += RsyncOutput;

				Log.TraceLog("[Rsync] {0} {1}", Utils.MakePathSafeToUseWithCommandLine(RsyncProcess.StartInfo.FileName), RsyncProcess.StartInfo.Arguments);
				return Utils.RunLocalProcess(RsyncProcess);
			}
		}

		/// <summary>
		/// Handles data output by rsync
		/// </summary>
		/// <param name="Sender">The object sending the messag</param>
		/// <param name="Args">The received data</param>e
		private void RsyncOutput(object Sender, DataReceivedEventArgs Args)
		{
			if(Args.Data != null)
			{
				Log.TraceInformation("  {0}", Args.Data);
			}
		}

		/// <summary>
		/// Execute a command on the remote in the remote equivalent of a local directory
		/// </summary>
		/// <param name="WorkingDir"></param>
		/// <param name="Command"></param>
		/// <returns></returns>
		public int Execute(DirectoryReference WorkingDir, string Command)
		{
			return Execute(GetRemotePath(WorkingDir), Command);
		}

		/// <summary>
		/// Execute a remote command, capturing the output text
		/// </summary>
		/// <param name="WorkingDirectory">The remote working directory</param>
		/// <param name="Command">Command to be executed</param>
		/// <returns></returns>
		protected int Execute(string WorkingDirectory, string Command)
		{
			string FullCommand = String.Format("cd {0} && {1}", EscapeShellArgument(WorkingDirectory), Command);
			using(Process SSHProcess = new Process())
			{
				DataReceivedEventHandler OutputHandler = (E, Args) => { SshOutput(Args); };

				SSHProcess.StartInfo.FileName = SshExe.FullName;
				SSHProcess.StartInfo.WorkingDirectory = SshExe.Directory.FullName;
				SSHProcess.StartInfo.Arguments = String.Format("{0} {1}", String.Join(" ", CommonSshArguments), FullCommand.Replace("\"", "\\\""));
				SSHProcess.OutputDataReceived += OutputHandler;
				SSHProcess.ErrorDataReceived += OutputHandler;

				Log.TraceLog("[SSH] {0} {1}", Utils.MakePathSafeToUseWithCommandLine(SSHProcess.StartInfo.FileName), SSHProcess.StartInfo.Arguments);
				return Utils.RunLocalProcess(SSHProcess);
			}
		}

		/// <summary>
		/// Handler for output from running remote SSH commands
		/// </summary>
		/// <param name="Args"></param>
		private void SshOutput(DataReceivedEventArgs Args)
		{
			if(Args.Data != null)
			{
				string FormattedOutput = ConvertRemotePathsToLocal(Args.Data);
				Log.TraceInformation("  {0}", FormattedOutput);
			}
		}

		/// <summary>
		/// Converts any remote paths within the given string to local format
		/// </summary>
		/// <param name="Text">The text containing strings to convert</param>
		/// <returns>The string with paths converted to local format</returns>
		private string ConvertRemotePathsToLocal(string Text)
		{
			// Try to match any source file with the remote base directory in front of it
			string Pattern = String.Format("(?<![a-zA-Z=]){0}[^:]*\\.(?:cpp|inl|h|hpp|hh|txt)(?![a-zA-Z])", Regex.Escape(RemoteBaseDir));

			// Find the matches, and early out if there are none
			MatchCollection Matches = Regex.Matches(Text, Pattern, RegexOptions.IgnoreCase);
			if(Matches.Count == 0)
			{
				return Text;
			}

			// Replace any remote paths with local ones
			StringBuilder Result = new StringBuilder();
			int StartIdx = 0;
			foreach(Match Match in Matches)
			{
				// Append the text leading up to this path
				Result.Append(Text, StartIdx, Match.Index - StartIdx);

				// Try to convert the path
				string Path = Match.Value;
				foreach(RemoteMapping Mapping in Mappings)
				{
					if(Path.StartsWith(Mapping.RemoteDirectory))
					{
						Path = Mapping.LocalDirectory + Path.Substring(Mapping.RemoteDirectory.Length).Replace('/', '\\');
						break;
					}
				}

				// Append the path to the output string
				Result.Append(Path);

				// Move past this match
				StartIdx = Match.Index + Match.Length;
			}
			Result.Append(Text, StartIdx, Text.Length - StartIdx);
			return Result.ToString();
		}
	}
}
