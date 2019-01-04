// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Class to apply a log indent for the lifetime of an object 
	/// </summary>
	public class LogIndentScope : IDisposable
	{
		/// <summary>
		/// The previous indent
		/// </summary>
		string PrevIndent;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Indent">Indent to append to the existing indent</param>
		public LogIndentScope(string Indent)
		{
			PrevIndent = Log.Indent;
			Log.Indent += Indent;
		}

		/// <summary>
		/// Restore the log indent to normal
		/// </summary>
		public void Dispose()
		{
			if (PrevIndent != null)
			{
				Log.Indent = PrevIndent;
				PrevIndent = null;
			}
		}
	}
}
