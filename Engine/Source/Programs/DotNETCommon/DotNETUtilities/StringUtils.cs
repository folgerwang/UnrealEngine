// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	public static class StringUtils
	{
		/// <summary>
		/// Extension method to allow formatting a string to a stringbuilder and appending a newline
		/// </summary>
		/// <param name="Builder">The string builder</param>
		/// <param name="Format">Format string, as used for StringBuilder.AppendFormat</param>
		/// <param name="Args">Arguments for the format string</param>
		public static void AppendLine(this StringBuilder Builder, string Format, params object[] Args)
		{
			Builder.AppendFormat(Format, Args);
			Builder.AppendLine();
		}

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
