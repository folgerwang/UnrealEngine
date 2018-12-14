// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Represents an exception specific to perforce
	/// </summary>
	public class PerforceException : Exception
	{
		/// <summary>
		/// For errors returned by the server, contains the error record
		/// </summary>
		public PerforceError Error;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Message">Message for the exception</param>
		public PerforceException(string Message)
			: base(Message)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Arguments for the formatted string</param>
		public PerforceException(string Format, params object[] Args)
			: base(String.Format(Format, Args))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Error">The error from the server</param>
		public PerforceException(PerforceError Error)
			: base(Error.ToString())
		{
			this.Error = Error;
		}
	}
}
