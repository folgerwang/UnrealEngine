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
		/// Indents a string by a given indent
		/// </summary>
		/// <param name="Text">The text to indent</param>
		/// <param name="Indent">The indent to add to each line</param>
		/// <returns>The indented string</returns>
		public static string Indent(string Text, string Indent)
		{
			string Result = "";
			if(Text.Length > 0)
			{
				Result = Indent + Text.Replace("\n", "\n" + Indent);
			}
			return Result;
		}

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
		/// Formats a list of strings in the style "1, 2, 3 and 4"
		/// </summary>
		/// <param name="Arguments">List of strings to format</param>
		/// <returns>Formatted list of strings</returns>
		public static string FormatList(string[] Arguments)
		{
			StringBuilder Result = new StringBuilder();
			if(Arguments.Length > 0)
			{
				Result.Append(Arguments[0]);
				for(int Idx = 1; Idx < Arguments.Length; Idx++)
				{
					if(Idx == Arguments.Length - 1)
					{
						Result.Append(" and ");
					}
					else
					{
						Result.Append(", ");
					}
					Result.Append(Arguments[Idx]);
				}
			}
			return Result.ToString();
		}

		/// <summary>
		/// Formats a list of strings in the style "1, 2, 3 and 4"
		/// </summary>
		/// <param name="Arguments">List of strings to format</param>
		/// <returns>Formatted list of strings</returns>
		public static string FormatList(IEnumerable<string> Arguments)
		{
			return FormatList(Arguments.ToArray());
		}
	}
}
