// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DotNETUtilities
{
	public static class StringUtils
	{
		/// <summary>
		/// Case-sensitive replacement for String.EndsWith(), which seems to be pathalogically slow on Mono.
		/// </summary>
		/// <param name="Source">String to test</param>
		/// <param name="Suffix">Suffix to check for</param>
		/// <returns>True if the source string ends with the given suffix</returns>
		public static bool FastEndsWith(string Source, string Suffix)
		{
			if(Source.Length < Suffix.Length)
			{
				return false;
			}

			int SourceBase = Source.Length - Suffix.Length;
			for(int Idx = 0; Idx < Suffix.Length; Idx++)
			{
				if(Source[SourceBase + Idx] != Suffix[Idx])
				{
					return false;
				}
			}

			return true;
		}
	}
}
