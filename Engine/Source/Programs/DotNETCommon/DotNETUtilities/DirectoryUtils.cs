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
	/// Utility functions for manipulating directories
	/// </summary>
	public class DirectoryUtils
	{
		/// <summary>
		/// Finds the on-disk case of a a directory
		/// </summary>
		/// <param name="Info">DirectoryInfo instance describing the directory</param>
		/// <returns>New DirectoryInfo instance that represents the directory with the correct case</returns>
		public static DirectoryInfo FindCorrectCase(DirectoryInfo Info)
		{
			DirectoryInfo ParentInfo = Info.Parent;
			if (ParentInfo == null)
			{
				string FullName = Info.FullName;
				if (FullName.Length >= 2 && (FullName[0] >= 'a' && FullName[0] <= 'z') && FullName[1] == ':')
				{
					return new DirectoryInfo(Char.ToUpper(FullName[0]) + FullName.Substring(1));
				}
				else
				{
					return Info;
				}
			}
			else
			{
				ParentInfo = FindCorrectCase(ParentInfo);
				foreach (DirectoryInfo ChildInfo in ParentInfo.EnumerateDirectories())
				{
					if (String.Equals(ChildInfo.Name, Info.Name, DirectoryReference.Comparison))
					{
						return ChildInfo;
					}
				}
				return new DirectoryInfo(Path.Combine(ParentInfo.FullName, Info.Name));
			}
		}
	}
}
