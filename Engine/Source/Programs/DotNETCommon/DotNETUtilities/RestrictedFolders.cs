// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Names of restricted folders. Note: The name of each entry is used to search for/create folders
	/// </summary>
	public enum RestrictedFolder
	{
		/// <summary>
		/// Legacy. Should not be used any more.
		/// </summary>
		EpicInternal,

		/// <summary>
		/// Can be used by UE4 but not required
		/// </summary>
		CarefullyRedist,

		/// <summary>
		/// Epic Employees and Contractors
		/// </summary>
		NotForLicensees,

		/// <summary>
		/// Epic Employees only
		/// </summary>
		NoRedist,

		/// <summary>
		/// Playstation 4 source files
		/// </summary>
		PS4,

		/// <summary>
		/// XboxOne source files
		/// </summary>
		XboxOne,

		/// <summary>
		/// Switch source files
		/// </summary>
		Switch,

		/// <summary>
		/// Quail source files
		/// </summary>
		Quail,
	}

	/// <summary>
	/// Utility functions for getting restricted folder
	/// </summary>
	public static class RestrictedFolders
	{
		/// <summary>
		/// Public array of restricted folder names
		/// </summary>
		public static readonly string[] Names;

		/// <summary>
		/// Public array of restricted folder names
		/// </summary>
		public static readonly RestrictedFolder[] Values;

		/// <summary>
		/// Keeps a cache of folder substrings and flags
		/// </summary>
		static readonly Tuple<string, RestrictedFolder>[] NamesAndValues;
		
		/// <summary>
		/// Static constructor
		/// </summary>
		static RestrictedFolders()
		{
			Names = Enum.GetNames(typeof(RestrictedFolder));
			Values = (RestrictedFolder[])Enum.GetValues(typeof(RestrictedFolder));

			NamesAndValues = new Tuple<string, RestrictedFolder>[Names.Length];
			for(int Idx = 0; Idx < Names.Length; Idx++)
			{
				NamesAndValues[Idx] = Tuple.Create(Names[Idx], Values[Idx]);
			}
		}

		/// <summary>
		/// Finds all the restricted folder names relative to a base directory
		/// </summary>
		/// <param name="BaseDir">The base directory to check against</param>
		/// <param name="OtherDir">The file or directory to check</param>
		/// <returns>Array of restricted folder names</returns>
		public static List<RestrictedFolder> FindRestrictedFolders(DirectoryReference BaseDir, DirectoryReference OtherDir)
		{
			List<RestrictedFolder> Folders = new List<RestrictedFolder>();
			if(OtherDir.IsUnderDirectory(BaseDir))
			{
				foreach(Tuple<string, RestrictedFolder> Pair in NamesAndValues)
				{
					if(OtherDir.ContainsName(Pair.Item1, BaseDir.FullName.Length))
					{
						Folders.Add(Pair.Item2);
					}
				}
			}
			return Folders;
		}
	}
}
