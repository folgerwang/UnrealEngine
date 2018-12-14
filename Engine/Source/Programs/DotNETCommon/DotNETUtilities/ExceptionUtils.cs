// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Methods for adding context information to exceptions
	/// </summary>
	public static class ExceptionUtils
	{
		/// <summary>
		/// Unique key name for adding context to exceptions thrown inside Epic apps
		/// </summary>
		const string ContextEntryName = "EpicGames.Context";

		/// <summary>
		/// Adds a context message to a list stored on the given exception. Intended to be used in situations where supplying additional context
		/// for an exception is valuable, but wrapping it would remove information.
		/// </summary>
		/// <param name="Ex">The exception that was thrown</param>
		/// <param name="Message">Message to append to the context list</param>
		public static void AddContext(Exception Ex, string Message)
		{
			List<string> Messages = Ex.Data[ContextEntryName] as List<string>;
			if (Messages == null)
			{
				Messages = new List<string>();
				Ex.Data[ContextEntryName] = Messages;
			}
			Messages.Add(Message);
		}

		/// <summary>
		/// Adds a context message to a list stored on the given exception. Intended to be used in situations where supplying additional context
		/// for an exception is valuable, but wrapping it would remove information.
		/// </summary>
		/// <param name="Ex">The exception that was thrown</param>
		/// <param name="Format">Formatting string for </param>
		/// <param name="Args">Message to append to the context list</param>
		public static void AddContext(Exception Ex, string Format, params object[] Args)
		{
			AddContext(Ex, String.Format(Format, Args));
		}

		/// <summary>
		/// Enumerates the context lines from the given exception
		/// </summary>
		/// <param name="Ex">The exception to retrieve context from</param>
		/// <returns>Sequence of context lines</returns>
		public static IEnumerable<string> GetContext(Exception Ex)
		{
			List<string> Messages = Ex.Data[ContextEntryName] as List<string>;
			if (Messages != null)
			{
				foreach (string Message in Messages)
				{
					yield return Message;
				}
			}
		}

		/// <summary>
		/// Formats an exception for display in the log, including additional lines of context that were attached to it.
		/// </summary>
		/// <param name="Ex">The exception to format</param>
		/// <returns>String containing the exception information. May be multiple lines.</returns>
		public static string FormatException(Exception Ex)
		{
			StringBuilder ErrorMessage = new StringBuilder();
			if (Ex is AggregateException)
			{
				Exception InnerException = ((AggregateException)Ex).InnerException;
				ErrorMessage.Append(InnerException.ToString());
				foreach (string Line in GetContext(InnerException))
				{
					ErrorMessage.AppendFormat("\n  {0}", Line);
				}
			}
			else
			{
				ErrorMessage.Append(Ex.ToString());
			}
			foreach (string Line in GetContext(Ex))
			{
				ErrorMessage.AppendFormat("\n{0}", Line);
			}
			return ErrorMessage.ToString();
		}

		/// <summary>
		/// Formats a detailed information about where an exception occurs, including any inner exceptions
		/// </summary>
		/// <param name="Ex">The exception to format</param>
		/// <returns>String containing the exception information. May be multiple lines.</returns>
		public static string FormatExceptionDetails(Exception Ex)
		{
			List<Exception> ExceptionStack = new List<Exception>();
			for (Exception CurrentEx = Ex; CurrentEx != null; CurrentEx = CurrentEx.InnerException)
			{
				ExceptionStack.Add(CurrentEx);
			}

			StringBuilder Message = new StringBuilder();
			for (int Idx = ExceptionStack.Count - 1; Idx >= 0; Idx--)
			{
				Exception CurrentEx = ExceptionStack[Idx];
				Message.AppendFormat("{0}{1}: {2}\n{3}", (Idx == ExceptionStack.Count - 1) ? "" : "Wrapped by ", CurrentEx.GetType().Name, CurrentEx.Message, CurrentEx.StackTrace);

				if (CurrentEx.Data.Count > 0)
				{
					foreach (object Key in CurrentEx.Data.Keys)
					{
						object Value = CurrentEx.Data[Key];

						string ValueString;
						if(Value is List<string>)
						{
							ValueString = String.Format("({0})", String.Join(", ", ((List<string>)Value).Select(x => String.Format("\"{0}\"", x))));
						}
						else
						{
							ValueString = Value.ToString();
						}

						Message.AppendFormat("   data: {0} = {1}", Key, ValueString);
					}
				}
			}
			return Message.ToString();
		}
	}
}
