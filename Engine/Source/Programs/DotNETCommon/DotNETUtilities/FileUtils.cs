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
	/// Utility functions for manipulating files
	/// </summary>
	public static class FileUtils
	{
		/// <summary>
		/// Finds the on-disk case of a a file
		/// </summary>
		/// <param name="Info">FileInfo instance describing the file</param>
		/// <returns>New FileInfo instance that represents the file with the correct case</returns>
		public static FileInfo FindCorrectCase(FileInfo Info)
		{
			DirectoryInfo ParentInfo = DirectoryUtils.FindCorrectCase(Info.Directory);
			foreach (FileInfo ChildInfo in ParentInfo.EnumerateFiles())
			{
				if (String.Equals(ChildInfo.Name, Info.Name, FileReference.Comparison))
				{
					return ChildInfo;
				}
			}
			return new FileInfo(Path.Combine(ParentInfo.FullName, Info.Name));
		}
	}
}
