// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Runtime.Serialization;
using Tools.DotNETCommon;
using System.Collections.Concurrent;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a file on disk that is used as an input or output of a build action. FileItem instances are unique for a given path. Use FileItem.GetItemByFileReference 
	/// to get the FileItem for a specific path.
	/// </summary>
	class FileItem
	{
		/// <summary>
		/// The directory containing this file
		/// </summary>
		DirectoryItem CachedDirectory;

		/// <summary>
		/// Location of this file
		/// </summary>
		public readonly FileReference Location;

		/// <summary>
		/// The information about the file.
		/// </summary>
		FileInfo Info;

		/// <summary>
		/// A case-insensitive dictionary that's used to map each unique file name to a single FileItem object.
		/// </summary>
		static ConcurrentDictionary<FileReference, FileItem> UniqueSourceFileMap = new ConcurrentDictionary<FileReference, FileItem>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Info">File info</param>
		private FileItem(FileReference Location, FileInfo Info)
		{
			this.Location = Location;
			this.Info = Info;
		}

		/// <summary>
		/// Name of this file
		/// </summary>
		public string Name
		{
			get { return Info.Name; }
		}

		/// <summary>
		/// Accessor for the absolute path to the file
		/// </summary>
		public string AbsolutePath
		{
			get { return Location.FullName; }
		}

		/// <summary>
		/// Gets the directory that this file is in
		/// </summary>
		public DirectoryItem Directory
		{
			get
			{
				if(CachedDirectory == null)
				{
					CachedDirectory = DirectoryItem.GetItemByDirectoryReference(Location.Directory);
				}
				return CachedDirectory;
			}
		}

		/// <summary>
		/// Whether the file exists.
		/// </summary>
		public bool Exists
		{
			get { return Info.Exists; }
		}

		/// <summary>
		/// Size of the file if it exists, otherwise -1
		/// </summary>
		public long Length
		{
			get { return Info.Length; }
		}

		/// <summary>
		/// The attributes for this file
		/// </summary>
		public FileAttributes Attributes
		{
			get { return Info.Attributes; }
		}

		/// <summary>
		/// The last write time of the file.
		/// </summary>
		public DateTime LastWriteTimeUtc
		{
			get { return Info.LastWriteTimeUtc; }
		}

		/// <summary>
		/// Determines if the file has the given extension
		/// </summary>
		/// <param name="Extension">The extension to check for</param>
		/// <returns>True if the file has the given extension, false otherwise</returns>
		public bool HasExtension(string Extension)
		{
			return Location.HasExtension(Extension);
		}

		/// <summary>
		/// Gets the directory containing this file
		/// </summary>
		/// <returns>DirectoryItem for the directory containing this file</returns>
		public DirectoryItem GetDirectoryItem()
		{
			return Directory;
		}

		/// <summary>
		/// Updates the cached directory for this file. Used by DirectoryItem when enumerating files, to avoid having to look this up later.
		/// </summary>
		/// <param name="Directory">The directory that this file is in</param>
		public void UpdateCachedDirectory(DirectoryItem Directory)
		{
			Debug.Assert(Directory.Location == Location.Directory);
			CachedDirectory = Directory;
		}

		/// <summary>
		/// Gets a FileItem corresponding to the given path
		/// </summary>
		/// <param name="FilePath">Path for the FileItem</param>
		/// <returns>The FileItem that represents the given file path.</returns>
		public static FileItem GetItemByPath(string FilePath)
		{
			return GetItemByFileReference(new FileReference(FilePath));
		}

		/// <summary>
		/// Gets a FileItem for a given path
		/// </summary>
		/// <param name="Info">Information about the file</param>
		/// <returns>The FileItem that represents the given a full file path.</returns>
		public static FileItem GetItemByFileInfo(FileInfo Info)
		{
			FileReference Location = new FileReference(Info);

			FileItem Result;
			if (!UniqueSourceFileMap.TryGetValue(Location, out Result))
			{
				FileItem NewFileItem = new FileItem(Location, Info);
				if(UniqueSourceFileMap.TryAdd(Location, NewFileItem))
				{
					Result = NewFileItem;
				}
				else
				{
					Result = UniqueSourceFileMap[Location];
				}
			}
			return Result;
		}

		/// <summary>
		/// Gets a FileItem for a given path
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>The FileItem that represents the given a full file path.</returns>
		public static FileItem GetItemByFileReference(FileReference Location)
		{
			FileItem Result;
			if (!UniqueSourceFileMap.TryGetValue(Location, out Result))
			{
				FileItem NewFileItem = new FileItem(Location, Location.ToFileInfo());
				if(UniqueSourceFileMap.TryAdd(Location, NewFileItem))
				{
					Result = NewFileItem;
				}
				else
				{
					Result = UniqueSourceFileMap[Location];
				}
			}
			return Result;
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
		/// <param name="Location">Path to the intermediate file to create</param>
		/// <param name="Contents">Contents of the new file</param>
		/// <returns>File item for the newly created file</returns>
		public static FileItem CreateIntermediateTextFile(FileReference Location, string Contents)
		{
			// Only write the file if its contents have changed.
			if (!FileReference.Exists(Location))
			{
				DirectoryReference.CreateDirectory(Location.Directory);
				FileReference.WriteAllText(Location, Contents, GetEncodingForString(Contents));
			}
			else
			{
				string CurrentContents = Utils.ReadAllText(Location.FullName);
				if(!String.Equals(CurrentContents, Contents, StringComparison.InvariantCultureIgnoreCase))
				{
					FileReference BackupFile = new FileReference(Location.FullName + ".old");
					try
					{
						Log.TraceLog("Updating {0}: contents have changed. Saving previous version to {1}.", Location, BackupFile);
						FileReference.Delete(BackupFile);
						FileReference.Move(Location, BackupFile);
					}
					catch(Exception Ex)
					{
						Log.TraceWarning("Unable to rename {0} to {1}", Location, BackupFile);
						Log.TraceLog("{0}", ExceptionUtils.FormatExceptionDetails(Ex));
					}
					FileReference.WriteAllText(Location, Contents, GetEncodingForString(Contents));
				}
			}

			// Reset the file info, in case it already knows about the old file
			FileItem Item = GetItemByFileReference(Location);
			Item.ResetCachedInfo();
			return Item;
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
			Debug.Assert(Exists);

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
		/// Resets the cached file info
		/// </summary>
		public void ResetCachedInfo()
		{
			Info = Location.ToFileInfo();
		}

		/// <summary>
		/// Resets all cached file info. Significantly reduces performance; do not use unless strictly necessary.
		/// </summary>
		public static void ResetAllCachedInfo_SLOW()
		{
			foreach(FileItem Item in UniqueSourceFileMap.Values)
			{
				Item.ResetCachedInfo();
			}
		}

		/// <summary>
		/// Return the path to this FileItem to debugging
		/// </summary>
		/// <returns>Absolute path to this file item</returns>
		public override string ToString()
		{
			return AbsolutePath;
		}
	}

	/// <summary>
	/// Helper functions for serialization
	/// </summary>
	static class FileItemExtensionMethods
	{
		/// <summary>
		/// Read a file item from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>Instance of the serialized file item</returns>
		public static FileItem ReadFileItem(this BinaryArchiveReader Reader)
		{
			return Reader.ReadObjectReference<FileItem>(() => FileItem.GetItemByFileReference(Reader.ReadFileReference()));
		}

		/// <summary>
		/// Write a file item to a binary archive
		/// </summary>
		/// <param name="Writer">Writer to serialize data to</param>
		/// <param name="FileItem">File item to write</param>
		public static void WriteFileItem(this BinaryArchiveWriter Writer, FileItem FileItem)
		{
			Writer.WriteObjectReference<FileItem>(FileItem, () => Writer.WriteFileReference(FileItem.Location));
		}

		/// <summary>
		/// Read a file item as a DirectoryItem and name. This is slower than reading it directly, but results in a significantly smaller archive
		/// where most files are in the same directories.
		/// </summary>
		/// <param name="Reader">Archive to read from</param>
		/// <returns>FileItem read from the archive</returns>
		static FileItem ReadCompactFileItemData(this BinaryArchiveReader Reader)
		{
			DirectoryItem Directory = Reader.ReadDirectoryItem();
			string Name = Reader.ReadString();

			FileItem FileItem = FileItem.GetItemByFileReference(FileReference.Combine(Directory.Location, Name));
            FileItem.UpdateCachedDirectory(Directory);
            return FileItem;
		}

		/// <summary>
		/// Read a file item in a format which de-duplicates directory names.
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>Instance of the serialized file item</returns>
		public static FileItem ReadCompactFileItem(this BinaryArchiveReader Reader)
		{
			return Reader.ReadObjectReference<FileItem>(() => ReadCompactFileItemData(Reader));
		}

		/// <summary>
		/// Writes a file item in a format which de-duplicates directory names.
		/// </summary>
		/// <param name="Writer">Writer to serialize data to</param>
		/// <param name="FileItem">File item to write</param>
		public static void WriteCompactFileItem(this BinaryArchiveWriter Writer, FileItem FileItem)
		{
			Writer.WriteObjectReference<FileItem>(FileItem, () => { Writer.WriteDirectoryItem(FileItem.GetDirectoryItem()); Writer.WriteString(FileItem.Name); });
		}
	}
}
