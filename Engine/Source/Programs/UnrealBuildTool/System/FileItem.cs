// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a file on disk that is used as an input or output of a build action.
	/// FileItems are created by calling FileItem.GetItemByFileReference, which creates a single FileItem for each unique file path.
	/// </summary>
	[Serializable]
	class FileItem : ISerializable
	{
		///
		/// Preparation and Assembly (serialized)
		/// 

		/// <summary>
		/// The action that produces the file.
		/// </summary>
		public Action ProducingAction = null;

		/// <summary>
		/// The file reference
		/// </summary>
		public FileReference Location;

		/// <summary>
		/// True if any DLLs produced by this
		/// </summary>
		public bool bNeedsHotReloadNumbersDLLCleanUp = false;

		/// <summary>
		/// Accessor for the absolute path to the file
		/// </summary>
		public string AbsolutePath
		{
			get { return Location.FullName; }
		}

		/// <summary>
		/// For C++ file items, this stores cached information about the include paths needed in order to include header files from these C++ files.  This is part of UBT's dependency caching optimizations.
		/// </summary>
		public CppIncludePaths CachedIncludePaths
		{
			get
			{
				return CachedIncludePathsValue;
			}
			set
			{
				if (value != null && CachedIncludePathsValue != null && CachedIncludePathsValue != value)
				{
					// Uh oh.  We're clobbering our cached CompileEnvironment for this file with a different CompileEnvironment.  This means
					// that the same source file is being compiled into more than one module.
					throw new BuildException("File '{0}' was included by multiple modules, but with different include paths", this.Info.FullName);
				}
				CachedIncludePathsValue = value;
			}
		}
		private CppIncludePaths CachedIncludePathsValue;


		///
		/// Transients (not serialized)
		///

		/// <summary>
		/// The information about the file.
		/// </summary>
		public FileInfo Info;

		/// <summary>
		/// This is true if this item is actually a directory. Consideration for Mac application bundles. Note that Info will be null if true!
		/// </summary>
		public bool IsDirectory;

		/// <summary>
		/// Relative cost of action associated with producing this file.
		/// </summary>
		public long RelativeCost = 0;

		/// <summary>
		/// The last write time of the file.
		/// </summary>
		public DateTimeOffset _LastWriteTime;
		public DateTimeOffset LastWriteTime
		{
			get { return _LastWriteTime; }
			set { _LastWriteTime = value; }
		}

		/// <summary>
		/// Whether the file exists.
		/// </summary>
		public bool _bExists = false;
		public bool bExists
		{
			get { return _bExists; }
			set { _bExists = value; }
		}

		/// <summary>
		/// Size of the file if it exists, otherwise -1
		/// </summary>
		public long _Length = -1;
		public long Length
		{
			get { return _Length; }
			set { _Length = value; }
		}


		///
		/// Statics
		///

		/// <summary>
		/// Used for performance debugging
		/// </summary>
		public static long TotalFileItemCount = 0;
		public static long MissingFileItemCount = 0;

		/// <summary>
		/// A case-insensitive dictionary that's used to map each unique file name to a single FileItem object.
		/// </summary>
		static Dictionary<FileReference, FileItem> UniqueSourceFileMap = new Dictionary<FileReference, FileItem>();

		/// <summary>
		/// Clears the FileItem caches.
		/// </summary>
		public static void ClearCaches()
		{
			UniqueSourceFileMap.Clear();
		}

		/// <summary>
		/// Clears the cached include paths on every file item
		/// </summary>
		public static void ClearCachedIncludePaths()
		{
			foreach(FileItem Item in UniqueSourceFileMap.Values)
			{
				Item.CachedIncludePaths = null;
			}
		}

		/// <returns>The FileItem that represents the given file path.</returns>
		public static FileItem GetItemByPath(string FilePath)
		{
			return GetItemByFileReference(new FileReference(FilePath));
		}

		/// <returns>The FileItem that represents the given a full file path.</returns>
		public static FileItem GetItemByFileReference(FileReference Reference)
		{
			FileItem Result = null;
			if (UniqueSourceFileMap.TryGetValue(Reference, out Result))
			{
				return Result;
			}
			else
			{
				return new FileItem(Reference);
			}
		}

		/// <summary>
		/// If the given file path identifies a file that already exists, returns the FileItem that represents it.
		/// </summary>
		public static FileItem GetExistingItemByPath(string FileName)
		{
			return GetExistingItemByFileReference(new FileReference(FileName));
		}

		/// <summary>
		/// If the given file path identifies a file that already exists, returns the FileItem that represents it.
		/// </summary>
		public static FileItem GetExistingItemByFileReference(FileReference FileRef)
		{
			FileItem Result = GetItemByFileReference(FileRef);
			if (Result.bExists)
			{
				return Result;
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Determines the appropriate encoding for a string: either ASCII or UTF-8.
		/// </summary>
		/// <param name="Str">The string to test.</param>
		/// <returns>Either System.Text.Encoding.ASCII or System.Text.Encoding.UTF8, depending on whether or not the string contains non-ASCII characters.</returns>
		private static Encoding GetEncodingForString(string Str)
		{
			// If the string length is equivalent to the encoded length, then no non-ASCII characters were present in the string.
			// Don't write BOM as it messes with clang when loading response files.
			return (Encoding.UTF8.GetByteCount(Str) == Str.Length) ? Encoding.ASCII : new UTF8Encoding(false);
		}

		/// <summary>
		/// Creates a text file with the given contents.  If the contents of the text file aren't changed, it won't write the new contents to
		/// the file to avoid causing an action to be considered outdated.
		/// </summary>
		/// <param name="AbsolutePath">Path to the intermediate file to create</param>
		/// <param name="Contents">Contents of the new file</param>
		/// <returns>File item for the newly created file</returns>
		public static FileItem CreateIntermediateTextFile(FileReference AbsolutePath, string Contents)
		{
			// Create the directory if it doesn't exist.
			Directory.CreateDirectory(Path.GetDirectoryName(AbsolutePath.FullName));

			// Only write the file if its contents have changed.
			if (!FileReference.Exists(AbsolutePath))
			{
				File.WriteAllText(AbsolutePath.FullName, Contents, GetEncodingForString(Contents));
			}
			else
			{
				string CurrentContents = Utils.ReadAllText(AbsolutePath.FullName);
				if(!String.Equals(CurrentContents, Contents, StringComparison.InvariantCultureIgnoreCase))
				{
					Log.TraceLog("Updating {0} - contents have changed. Previous:\n  {1}\nNew:\n  {2}", AbsolutePath.FullName, CurrentContents.Replace("\n", "\n  "), Contents.Replace("\n", "\n  "));
					File.WriteAllText(AbsolutePath.FullName, Contents, GetEncodingForString(Contents));
				}
			}
			return GetItemByFileReference(AbsolutePath);
		}

		/// <summary>
		/// Creates a text file with the given contents.  If the contents of the text file aren't changed, it won't write the new contents to
		/// the file to avoid causing an action to be considered outdated.
		/// </summary>
		/// <param name="AbsolutePath">Path to the intermediate file to create</param>
		/// <param name="Contents">Contents of the new file</param>
		/// <returns>File item for the newly created file</returns>
		public static FileItem CreateIntermediateTextFile(FileReference AbsolutePath, IEnumerable<string> Contents)
		{
			return CreateIntermediateTextFile(AbsolutePath, string.Join(Environment.NewLine, Contents));
		}

		/// <summary>
		/// Deletes the file.
		/// </summary>
		public void Delete()
		{
			Debug.Assert(_bExists);

			int MaxRetryCount = 3;
			int DeleteTryCount = 0;
			bool bFileDeletedSuccessfully = false;
			do
			{
				// If this isn't the first time through, sleep a little before trying again
				if (DeleteTryCount > 0)
				{
					Thread.Sleep(1000);
				}
				DeleteTryCount++;
				try
				{
					// Delete the destination file if it exists
					FileInfo DeletedFileInfo = new FileInfo(AbsolutePath);
					if (DeletedFileInfo.Exists)
					{
						DeletedFileInfo.IsReadOnly = false;
						DeletedFileInfo.Delete();
					}
					// Success!
					bFileDeletedSuccessfully = true;
				}
				catch (Exception Ex)
				{
					Log.TraceInformation("Failed to delete file '" + AbsolutePath + "'");
					Log.TraceInformation("    Exception: " + Ex.Message);
					if (DeleteTryCount < MaxRetryCount)
					{
						Log.TraceInformation("Attempting to retry...");
					}
					else
					{
						Log.TraceInformation("ERROR: Exhausted all retries!");
					}
				}
			}
			while (!bFileDeletedSuccessfully && (DeleteTryCount < MaxRetryCount));
		}

		/// <summary>
		/// Initialization constructor.
		/// </summary>
		protected FileItem(FileReference InFile)
		{
			Location = InFile;

			ResetFileInfo();

			++TotalFileItemCount;
			if (!_bExists)
			{
				++MissingFileItemCount;
				// Log.TraceInformation( "Missing: " + FileAbsolutePath );
			}

			UniqueSourceFileMap[Location] = this;
		}


		/// <summary>
		/// ISerializable: Constructor called when this object is deserialized
		/// </summary>
		protected FileItem(SerializationInfo SerializationInfo, StreamingContext StreamingContext)
		{
			ProducingAction = (Action)SerializationInfo.GetValue("pa", typeof(Action));
			Location = (FileReference)SerializationInfo.GetValue("fi", typeof(FileReference));
			bNeedsHotReloadNumbersDLLCleanUp = SerializationInfo.GetBoolean("hr");
			CachedIncludePaths = (CppIncludePaths)SerializationInfo.GetValue("ci", typeof(CppIncludePaths));

			// Go ahead and init normally now
			{
				ResetFileInfo();

				++TotalFileItemCount;
				if (!_bExists)
				{
					++MissingFileItemCount;
					// Log.TraceInformation( "Missing: " + FileAbsolutePath );
				}

				UniqueSourceFileMap[Location] = this;
			}
		}


		/// <summary>
		/// ISerializable: Called when serialized to report additional properties that should be saved
		/// </summary>
		public void GetObjectData(SerializationInfo SerializationInfo, StreamingContext StreamingContext)
		{
			SerializationInfo.AddValue("pa", ProducingAction);
			SerializationInfo.AddValue("fi", Location);
			SerializationInfo.AddValue("hr", bNeedsHotReloadNumbersDLLCleanUp);
			SerializationInfo.AddValue("ci", CachedIncludePaths);
		}


		/// <summary>
		/// (Re-)set file information for this FileItem
		/// </summary>
		public void ResetFileInfo()
		{
			if (Directory.Exists(AbsolutePath))
			{
				// path is actually a directory (such as a Mac app bundle)
				_bExists = true;
				LastWriteTime = Directory.GetLastWriteTimeUtc(AbsolutePath);
				IsDirectory = true;
				_Length = 0;
				Info = null;
			}
			else
			{
				Info = new FileInfo(AbsolutePath);

				_bExists = Info.Exists;
				if (_bExists)
				{
					_LastWriteTime = Info.LastWriteTimeUtc;
					_Length = Info.Length;
				}
			}
		}

		/// <summary>
		/// Reset file information on all cached FileItems.
		/// </summary>
		public static void ResetInfos()
		{
			foreach (KeyValuePair<FileReference, FileItem> Item in UniqueSourceFileMap)
			{
				Item.Value.ResetFileInfo();
			}
		}

		/// <summary>
		/// Initialization constructor for optionally remote files.
		/// </summary>
		protected FileItem(FileReference InReference, bool InIsRemoteFile, UnrealTargetPlatform Platform)
		{
			Location = InReference;

			FileInfo Info = new FileInfo(AbsolutePath);

			_bExists = Info.Exists;
			if (_bExists)
			{
				_LastWriteTime = Info.LastWriteTimeUtc;
				_Length = Info.Length;
			}

			++TotalFileItemCount;
			if (!_bExists)
			{
				++MissingFileItemCount;
				// Log.TraceInformation( "Missing: " + FileAbsolutePath );
			}

			// @todo iosmerge: This was in UE3, why commented out now?
			//UniqueSourceFileMap[AbsolutePathUpperInvariant] = this;
		}

		public override string ToString()
		{
			return AbsolutePath;
		}
	}

}
