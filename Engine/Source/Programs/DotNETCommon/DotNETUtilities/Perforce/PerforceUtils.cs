// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	public static class PerforceUtils
	{
		static public string EscapePath(string Path)
		{
			string NewPath = Path;
			NewPath = NewPath.Replace("%", "%25");
			NewPath = NewPath.Replace("*", "%2A");
			NewPath = NewPath.Replace("#", "%23");
			NewPath = NewPath.Replace("@", "%40");
			return NewPath;
		}

		static public string UnescapePath(string Path)
		{
			string NewPath = Path;
			NewPath = NewPath.Replace("%40", "@");
			NewPath = NewPath.Replace("%23", "#");
			NewPath = NewPath.Replace("%2A", "*");
			NewPath = NewPath.Replace("%2a", "*");
			NewPath = NewPath.Replace("%25", "%");
			return NewPath;
		}
	}
}
