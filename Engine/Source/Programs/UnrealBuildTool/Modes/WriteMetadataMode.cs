using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Parameters for the WriteMetadata mode
	/// </summary>
	[Serializable]
	class WriteMetadataTargetInfo
	{
		/// <summary>
		/// The project file
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// Output location for the version file
		/// </summary>
		public FileReference VersionFile;

		/// <summary>
		/// Output location for the target file
		/// </summary>
		public FileReference ReceiptFile;

		/// <summary>
		/// The partially constructed receipt data
		/// </summary>
		public TargetReceipt Receipt;

		/// <summary>
		/// Map of module manifest filenames to their location on disk.
		/// </summary>
		public Dictionary<FileReference, ModuleManifest> FileToManifest = new Dictionary<FileReference, ModuleManifest>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="VersionFile"></param>
		/// <param name="ReceiptFile"></param>
		/// <param name="Receipt"></param>
		/// <param name="FileToManifest"></param>
		public WriteMetadataTargetInfo(FileReference ProjectFile, FileReference VersionFile, FileReference ReceiptFile, TargetReceipt Receipt, Dictionary<FileReference, ModuleManifest> FileToManifest)//string EngineManifestName, string ProjectManifestName, Dictionary<string, FileReference> ModuleNameToLocation)
		{
			this.ProjectFile = ProjectFile;
			this.VersionFile = VersionFile;
			this.ReceiptFile = ReceiptFile;
			this.Receipt = Receipt;
			this.FileToManifest = FileToManifest;
		}
	}

	/// <summary>
	/// Writes all metadata files at the end of a build (receipts, version files, etc...). This is implemented as a separate mode to allow it to be done as part of the action graph.
	/// </summary>
	[ToolMode("WriteMetadata")]
	class WriteMetadataMode : ToolMode
	{
		/// <summary>
		/// Execute the command
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			// Read the target info
			WriteMetadataTargetInfo TargetInfo = BinaryFormatterUtils.Load<WriteMetadataTargetInfo>(Arguments.GetFileReference("-Input="));
			bool bNoManifestChanges = Arguments.HasOption("-NoManifestChanges");
			Arguments.CheckAllArgumentsUsed();

			// Check if we need to set a build id
			TargetReceipt Receipt = TargetInfo.Receipt;
			if(Receipt.Version.BuildId == null)
			{
				// Check if there's an exist version file. If it exists, try to merge in any manifests that are valid (and reuse the existing build id)
				BuildVersion PreviousVersion;
				if(TargetInfo.VersionFile != null && BuildVersion.TryRead(TargetInfo.VersionFile, out PreviousVersion))
				{
					// Check if we can reuse the existing manifests. This prevents unnecessary builds when switching between projects.
					Dictionary<FileReference, ModuleManifest> PreviousFileToManifest = new Dictionary<FileReference, ModuleManifest>();
					if(TryRecyclingManifests(PreviousVersion.BuildId, TargetInfo.FileToManifest.Keys, PreviousFileToManifest))
					{
						// Merge files from the existing manifests with the new ones
						foreach(KeyValuePair<FileReference, ModuleManifest> Pair in PreviousFileToManifest)
						{
							ModuleManifest OldManifest = Pair.Value;
							ModuleManifest NewManifest = TargetInfo.FileToManifest[Pair.Key];
							foreach(KeyValuePair<string, string> ModulePair in OldManifest.ModuleNameToFileName)
							{
								if(!NewManifest.ModuleNameToFileName.ContainsKey(ModulePair.Key))
								{
									NewManifest.ModuleNameToFileName.Add(ModulePair.Key, ModulePair.Value);
								}
							}
						}

						// Update the build id to use the current one
						Receipt.Version.BuildId = PreviousVersion.BuildId;
					}
				}

				// If the build id is still not set, generate a new one from a GUID
				if(Receipt.Version.BuildId == null)
				{
					Receipt.Version.BuildId = Guid.NewGuid().ToString();
				}
			}

			// Update the build id in all the manifests, and write them out
			foreach (KeyValuePair<FileReference, ModuleManifest> Pair in TargetInfo.FileToManifest)
			{
				FileReference ManifestFile = Pair.Key;
				if(!UnrealBuildTool.IsFileInstalled(ManifestFile))
				{
					ModuleManifest Manifest = Pair.Value;
					Manifest.BuildId = Receipt.Version.BuildId;

					if(!FileReference.Exists(ManifestFile))
					{
						// If the file doesn't already exist, just write it out
						DirectoryReference.CreateDirectory(ManifestFile.Directory);
						Manifest.Write(ManifestFile);
					}
					else
					{
						// Otherwise write it to a buffer first
						string OutputText;
						using (StringWriter Writer = new StringWriter())
						{
							Manifest.Write(Writer);
							OutputText = Writer.ToString();
						}

						// And only write it to disk if it's been modified. Note that if a manifest is out of date, we should have generated a new build id causing the contents to differ.
						string CurrentText = FileReference.ReadAllText(ManifestFile);
						if(CurrentText != OutputText)
						{
							if(bNoManifestChanges)
							{
								Log.TraceError("Build modifies {0}. This is not permitted. Before:\n    {1}\nAfter:\n    {2}", ManifestFile, CurrentText.Replace("\n", "\n    "), OutputText.Replace("\n", "\n    "));
							}
							else
							{
								FileReference.WriteAllText(ManifestFile, OutputText);
							}
						}
					}
				}
			}

			// Write out the version file, if it's changed. Since this file is next to the executable, it may be used by multiple targets, and we should avoid modifying it unless necessary.
			if(TargetInfo.VersionFile != null && !UnrealBuildTool.IsFileInstalled(TargetInfo.VersionFile))
			{
				DirectoryReference.CreateDirectory(TargetInfo.VersionFile.Directory);

				StringWriter Writer = new StringWriter();
				Receipt.Version.Write(Writer);

				string Text = Writer.ToString();
				if(!FileReference.Exists(TargetInfo.VersionFile) || File.ReadAllText(TargetInfo.VersionFile.FullName) != Text)
				{
					File.WriteAllText(TargetInfo.VersionFile.FullName, Text);
				}
			}

			// Write out the receipt
			if(!UnrealBuildTool.IsFileInstalled(TargetInfo.ReceiptFile))
			{
				DirectoryReference.CreateDirectory(TargetInfo.ReceiptFile.Directory);
				Receipt.Write(TargetInfo.ReceiptFile);
			}

			return 0;
		}

		/// <summary>
		/// Checks whether existing manifests on disk can be merged with new manifests being created, by testing whether any build products they reference have a newer timestamp
		/// </summary>
		/// <param name="BuildId">The current build id read from the version file. Only manifests matching this ID will be considered.</param>
		/// <param name="ManifestFiles">List of new manifest files</param>
		/// <param name="RecycleFileToManifest">If successful, is populated with a map of filename to existing manifests that can be merged with the new manifests.</param>
		/// <returns>True if the manifests can be recycled (and fills RecycleFileToManifest)</returns>
		bool TryRecyclingManifests(string BuildId, IEnumerable<FileReference> ManifestFiles, Dictionary<FileReference, ModuleManifest> RecycleFileToManifest)
		{
			bool bCanRecycleManifests = true;
			foreach(FileReference ManifestFileName in ManifestFiles)
			{
				if(ManifestFileName.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && FileReference.Exists(ManifestFileName))
				{
					try
					{
						ModuleManifest Manifest = ModuleManifest.Read(ManifestFileName);
						if(Manifest.BuildId == BuildId)
						{
							if(IsOutOfDate(ManifestFileName, Manifest))
							{
								bCanRecycleManifests = false;
								break;
							}
							RecycleFileToManifest.Add(ManifestFileName, Manifest);
						}
					}
					catch(Exception Ex)
					{
						Log.TraceWarning("Unable to read '{0}'; ignoring.", ManifestFileName);
						Log.TraceLog(ExceptionUtils.FormatExceptionDetails(Ex));
					}
				}
			}
			return bCanRecycleManifests;
		}

		/// <summary>
		/// Checks whether a module manifest on disk is out of date (whether any of the binaries it references are newer than it is)
		/// </summary>
		/// <param name="ManifestFileName">Path to the manifest</param>
		/// <param name="Manifest">The manifest contents</param>
		/// <returns>True if the manifest is out of date</returns>
		bool IsOutOfDate(FileReference ManifestFileName, ModuleManifest Manifest)
		{
			DateTime ManifestTime = FileReference.GetLastWriteTimeUtc(ManifestFileName);
			foreach(string FileName in Manifest.ModuleNameToFileName.Values)
			{
				FileInfo ModuleInfo = new FileInfo(FileReference.Combine(ManifestFileName.Directory, FileName).FullName);
				if(!ModuleInfo.Exists || ModuleInfo.LastWriteTimeUtc > ManifestTime)
				{
					return true;
				}
			}
			return false;
		}
	}
}
