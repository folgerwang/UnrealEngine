// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.CodeDom.Compiler;
using Microsoft.CSharp;
using Microsoft.Win32;
using System.Reflection;
using System.Diagnostics;
using UnrealBuildTool;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Captures all log output during startup until a log file writer has been created
	/// </summary>
	public class StartupTraceListener : TraceListener
	{
		StringBuilder Buffer = new StringBuilder();

		/// <summary>
		/// Constructor
		/// </summary>
		public StartupTraceListener()
		{
		}

		/// <summary>
		/// Copy the contents of the buffered output to another trace listener
		/// </summary>
		/// <param name="Other">The trace listener to receive the buffered output</param>
		public void CopyTo(TraceListener Other)
		{
			int StartIdx = 0;
			while(StartIdx < Buffer.Length)
			{
				int EndIdx = StartIdx;
				while(EndIdx < Buffer.Length && Buffer[EndIdx] != '\n')
				{
					EndIdx++;
				}
				Other.WriteLine(Buffer.ToString(StartIdx, EndIdx - StartIdx));
				StartIdx = EndIdx + 1;
			}
		}

		/// <summary>
		/// Write a message to the buffer
		/// </summary>
		/// <param name="Message">The message to write</param>
		public override void Write(string Message)
		{
			if(NeedIndent)
			{
				WriteIndent();
			}
			Buffer.Append(Message);
		}

		/// <summary>
		/// Write a message to the buffer, followed by a newline
		/// </summary>
		/// <param name="Message">The message to write</param>
		public override void WriteLine(string Message)
		{
			Write(Message);
			Buffer.Append("\n");
		}
	}
}
