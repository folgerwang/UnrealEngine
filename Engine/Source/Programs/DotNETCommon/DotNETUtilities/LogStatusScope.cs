// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Writes a status message to the log, which can be updated with a progress indicator as a slow task is being performed.
	/// </summary>
	public class LogStatusScope : IDisposable
	{
		/// <summary>
		/// The base status message
		/// </summary>
		string Message;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Message">The status message</param>
		public LogStatusScope(string Message)
		{
			this.Message = Message;
			Log.PushStatus(Message);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Format">The format specifier for the message</param>
		/// <param name="Args">Arguments for the status message</param>
		public LogStatusScope(string Format, params object[] Args)
			: this(String.Format(Format, Args))
		{
		}

		/// <summary>
		/// Updates the base status message passed into the constructor.
		/// </summary>
		/// <param name="Message">The status message</param>
		public void SetMessage(string Message)
		{
			this.Message = Message;
			Log.UpdateStatus(Message);
		}

		/// <summary>
		/// Updates the base status message passed into the constructor.
		/// </summary>
		/// <param name="Format">The format specifier for the message</param>
		/// <param name="Args">Arguments for the status message</param>
		public void SetMessage(string Format, params object[] Args)
		{
			SetMessage(String.Format(Format, Args));
		}

		/// <summary>
		/// Appends a progress string to the status message. Overwrites any previous progress message.
		/// </summary>
		/// <param name="Progress">The progress message</param>
		public void SetProgress(string Progress)
		{
			StringBuilder FullMessage = new StringBuilder(Message);
			FullMessage.Append(' ');
			FullMessage.Append(Progress);
			Log.UpdateStatus(FullMessage.ToString());
		}

		/// <summary>
		/// Appends a progress string to the status message. Overwrites any previous progress message.
		/// </summary>
		/// <param name="Format">The format specifier for the message</param>
		/// <param name="Args">Arguments for the status message</param>
		public void SetProgress(string Format, params object[] Args)
		{
			StringBuilder FullMessage = new StringBuilder(Message);
			FullMessage.Append(' ');
			FullMessage.AppendFormat(Format, Args);
			Log.UpdateStatus(FullMessage.ToString());
		}

		/// <summary>
		/// Pops the status message from the log.
		/// </summary>
		public void Dispose()
		{
			if(Message != null)
			{
				Log.PopStatus();
				Message = null;
			}
		}
	}
}
