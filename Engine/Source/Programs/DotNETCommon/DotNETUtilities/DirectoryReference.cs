// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Representation of an absolute directory path. Allows fast hashing and comparisons.
	/// </summary>
	[Serializable]
	public class DirectoryReference : FileSystemReference, IEquatable<DirectoryReference>
	{
		/// <summary>
		/// Special value used to invoke the non-sanitizing constructor overload
		/// </summary>
		public enum Sanitize
		{
			None
		}

		/// <summary>
		/// Default constructor.
		/// </summary>
		/// <param name="InPath">Path to this directory.</param>
		public DirectoryReference(string InPath)
			: base(FixTrailingPathSeparator(Path.GetFullPath(InPath)))
		{
		}

		/// <summary>
		/// Construct a DirectoryReference from a DirectoryInfo object.
		/// </summary>
		/// <param name="InInfo">Path to this file</param>
		public DirectoryReference(DirectoryInfo InInfo)
			: base(FixTrailingPathSeparator(InInfo.FullName))
		{
		}

		/// <summary>
		/// Constructor for creating a directory object directly from two strings.
		/// </summary>
		/// <param name="InFullName">The full, sanitized path name</param>
		/// <param name="InSanitize">Dummy argument used to resolve this overload</param>
		public DirectoryReference(string InFullName, Sanitize InSanitize)
			: base(InFullName)
		{
		}

		/// <summary>
		/// Ensures that the correct trailing path separator is appended. On Windows, the root directory (eg. C:\) always has a trailing path separator, but no other
		/// path does.
		/// </summary>
		/// <param name="DirName">Absolute path to the directory</param>
		/// <returns>Path to the directory, with the correct trailing path separator</returns>
		private static string FixTrailingPathSeparator(string DirName)
		{
			if(DirName.Length == 2 && DirName[1] == ':')
			{
				return DirName + Path.DirectorySeparatorChar;
			}
			else if(DirName.Length == 3 && DirName[1] == ':' && DirName[2] == Path.DirectorySeparatorChar)
			{
				return DirName;
			}
			else if(DirName.Length > 1 && DirName[DirName.Length - 1] == Path.DirectorySeparatorChar)
			{
				return DirName.TrimEnd(Path.DirectorySeparatorChar);
			}
			else
			{
				return DirName;
			}
		}

		/// <summary>
		/// Gets the top level directory name
		/// </summary>
		/// <returns>The name of the directory</returns>
		public string GetDirectoryName()
		{
			return Path.GetFileName(FullName);
		}

		/// <summary>
		/// Gets the directory containing this object
		/// </summary>
		/// <returns>A new directory object representing the directory containing this object</returns>
		public DirectoryReference ParentDirectory
		{
			get
			{
				if (IsRootDirectory())
				{
					return null;
				}

				int ParentLength = FullName.LastIndexOf(Path.DirectorySeparatorChar);
				if (ParentLength == 2 && FullName[1] == ':')
				{
					ParentLength++;
				}

				return new DirectoryReference(FullName.Substring(0, ParentLength), Sanitize.None);
			}
		}

		/// <summary>
		/// Gets the parent directory for a file
		/// </summary>
		/// <param name="File">The file to get directory for</param>
		/// <returns>The full directory name containing the given file</returns>
		public static DirectoryReference GetParentDirectory(FileReference File)
		{
			int ParentLength = File.FullName.LastIndexOf(Path.DirectorySeparatorChar);
			if(ParentLength == 2 && File.FullName[1] == ':')
			{
				ParentLength++;
			}
			return new DirectoryReference(File.FullName.Substring(0, ParentLength), Sanitize.None);
		}

		/// <summary>
		/// Gets the path for a special folder
		/// </summary>
		/// <param name="Folder">The folder to receive the path for</param>
		/// <returns>Directory reference for the given folder, or null if it is not available</returns>
		public static DirectoryReference GetSpecialFolder(Environment.SpecialFolder Folder)
		{
			string FolderPath = Environment.GetFolderPath(Folder);
			return String.IsNullOrEmpty(FolderPath)? null : new DirectoryReference(FolderPath);
		}

		/// <summary>
		/// Determines whether this path represents a root directory in the filesystem
		/// </summary>
		/// <returns>True if this path is a root directory, false otherwise</returns>
		public bool IsRootDirectory()
		{
			return FullName[FullName.Length - 1] == Path.DirectorySeparatorChar;
		}

		/// <summary>
		/// Combine several fragments with a base directory, to form a new directory name
		/// </summary>
		/// <param name="BaseDirectory">The base directory</param>
		/// <param name="Fragments">Fragments to combine with the base directory</param>
		/// <returns>The new directory name</returns>
		public static DirectoryReference Combine(DirectoryReference BaseDirectory, params string[] Fragments)
		{
			string FullName = FileSystemReference.CombineStrings(BaseDirectory, Fragments);
			return new DirectoryReference(FullName, Sanitize.None);
		}

		/// <summary>
		/// Compares two filesystem object names for equality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public static bool operator ==(DirectoryReference A, DirectoryReference B)
		{
			if ((object)A == null)
			{
				return (object)B == null;
			}
			else
			{
				return (object)B != null && A.FullName.Equals(B.FullName, Comparison);
			}
		}

		/// <summary>
		/// Compares two filesystem object names for inequality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>False if the names represent the same object, true otherwise</returns>
		public static bool operator !=(DirectoryReference A, DirectoryReference B)
		{
			return !(A == B);
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public override bool Equals(object Obj)
		{
			return (Obj is DirectoryReference) && ((DirectoryReference)Obj) == this;
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public bool Equals(DirectoryReference Obj)
		{
			return Obj == this;
		}

		/// <summary>
		/// Returns a hash code for this object
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			return Comparer.GetHashCode(FullName);
		}

		/// <summary>
		/// Helper function to create a remote directory reference. Unlike normal DirectoryReference objects, these aren't converted to a full path in the local filesystem.
		/// </summary>
		/// <param name="AbsolutePath">The absolute path in the remote file system</param>
		/// <returns>New directory reference</returns>
		public static DirectoryReference MakeRemote(string AbsolutePath)
		{
			return new DirectoryReference(AbsolutePath, Sanitize.None);
		}

		/// <summary>
		/// Gets the parent directory for a file, or returns null if it's null.
		/// </summary>
		/// <param name="File">The file to create a directory reference for</param>
		/// <returns>The directory containing the file  </returns>
		public static DirectoryReference FromFile(FileReference File)
		{
			if(File == null)
			{
				return null;
			}
			else
			{
				return File.Directory;
			}
		}

		/// <summary>
		/// Create a DirectoryReference from a string. If the string is null, returns a null DirectoryReference.
		/// </summary>
		/// <param name="DirectoryName">Path for the new object</param>
		/// <returns>Returns a FileReference representing the given string, or null.</returns>
		public static DirectoryReference FromString(string DirectoryName)
		{
			if(String.IsNullOrEmpty(DirectoryName))
			{
				return null;
			}
			else
			{
				return new DirectoryReference(DirectoryName);
			}
		}

		/// <summary>
		/// Finds the correct case to match the location of this file on disk. Uses the given case for parts of the path that do not exist.
		/// </summary>
		/// <param name="Location">The path to find the correct case for</param>
		/// <returns>Location of the file with the correct case</returns>
		public static DirectoryReference FindCorrectCase(DirectoryReference Location)
		{
			return new DirectoryReference(DirectoryUtils.FindCorrectCase(Location.ToDirectoryInfo()));
		}

		/// <summary>
		/// Constructs a DirectoryInfo object from this reference
		/// </summary>
		/// <returns>New DirectoryInfo object</returns>
		public DirectoryInfo ToDirectoryInfo()
		{
			return new DirectoryInfo(FullName);
		}

		#region System.IO.Directory Wrapper Methods

		/// <summary>
		/// Finds the current directory
		/// </summary>
		/// <returns>The current directory</returns>
		public static DirectoryReference GetCurrentDirectory()
		{
			return new DirectoryReference(Directory.GetCurrentDirectory());
		}

		/// <summary>
		/// Creates a directory
		/// </summary>
		/// <param name="Location">Location of the directory</param>
		public static void CreateDirectory(DirectoryReference Location)
		{
			Directory.CreateDirectory(Location.FullName);
		}

        /// <summary>
        /// Deletes a directory
        /// </summary>
		/// <param name="Location">Location of the directory</param>
        public static void Delete(DirectoryReference Location)
        {
            Directory.Delete(Location.FullName);
        }

        /// <summary>
        /// Deletes a directory
        /// </summary>
		/// <param name="Location">Location of the directory</param>
		/// <param name="bRecursive">Whether to remove directories recursively</param>
        public static void Delete(DirectoryReference Location, bool bRecursive)
        {
            Directory.Delete(Location.FullName, bRecursive);
        }

        /// <summary>
        /// Checks whether the directory exists
        /// </summary>
		/// <param name="Location">Location of the directory</param>
        /// <returns>True if this directory exists</returns>
        public static bool Exists(DirectoryReference Location)
		{
			return Directory.Exists(Location.FullName);
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName))
			{
				yield return new FileReference(FileName, FileReference.Sanitize.None);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir, string Pattern)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName, Pattern))
			{
				yield return new FileReference(FileName, FileReference.Sanitize.None);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <param name="Option">Options for the search</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir, string Pattern, SearchOption Option)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName, Pattern, Option))
			{
				yield return new FileReference(FileName, FileReference.Sanitize.None);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName))
			{
				yield return new DirectoryReference(DirectoryName, Sanitize.None);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching directories</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir, string Pattern)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName, Pattern))
			{
				yield return new DirectoryReference(DirectoryName, Sanitize.None);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <param name="Option">Options for the search</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir, string Pattern, SearchOption Option)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName, Pattern, Option))
			{
				yield return new DirectoryReference(DirectoryName, Sanitize.None);
			}
		}

		/// <summary>
		/// Sets the current directory
		/// </summary>
		/// <param name="Location">Location of the new current directory</param>
		public static void SetCurrentDirectory(DirectoryReference Location)
		{
			Directory.SetCurrentDirectory(Location.FullName);
		}

		#endregion
	}

	/// <summary>
	/// Extension methods for passing DirectoryReference arguments
	/// </summary>
	public static class DirectoryReferenceExtensionMethods
	{
		/// <summary>
		/// Manually serialize a file reference to a binary stream.
		/// </summary>
		/// <param name="Writer">Binary writer to write to</param>
		/// <param name="Directory">The directory reference to write</param>
		public static void Write(this BinaryWriter Writer, DirectoryReference Directory)
		{
			Writer.Write((Directory == null) ? String.Empty : Directory.FullName);
		}

		/// <summary>
		/// Manually deserialize a directory reference from a binary stream.
		/// </summary>
		/// <param name="Reader">Binary reader to read from</param>
		/// <returns>New DirectoryReference object</returns>
		public static DirectoryReference ReadDirectoryReference(this BinaryReader Reader)
		{
			string FullName = Reader.ReadString();
			return (FullName.Length == 0) ? null : new DirectoryReference(FullName, DirectoryReference.Sanitize.None);
		}

		/// <summary>
		/// Writes a directory reference  to a binary archive
		/// </summary>
		/// <param name="Writer">The writer to output data to</param>
		/// <param name="Directory">The item to write</param>
		public static void WriteDirectoryReference(this BinaryArchiveWriter Writer, DirectoryReference Directory)
		{
			if(Directory == null)
			{
				Writer.WriteString(null);
			}
			else
			{
				Writer.WriteString(Directory.FullName);
			}
		}

		/// <summary>
		/// Reads a directory reference from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize data from</param>
		/// <returns>New directory reference instance</returns>
		public static DirectoryReference ReadDirectoryReference(this BinaryArchiveReader Reader)
		{
			string FullName = Reader.ReadString();
			if(FullName == null)
			{
				return null;
			}
			else
			{
				return new DirectoryReference(FullName, DirectoryReference.Sanitize.None);
			}
		}
	}
}
