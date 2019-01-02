// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Represents a list of responses from the Perforce server. Within the list, individual responses
	/// may indicate success or failure.
	/// </summary>
	/// <typeparam name="T">Successful response type</typeparam>
	public class PerforceResponseList<T> : List<PerforceResponse<T>> where T : class
	{
		/// <summary>
		/// Whether all responses in this list are successful
		/// </summary>
		public bool Succeeded
		{
			get { return this.All(x => x.Succeeded); }
		}

		/// <summary>
		/// Returns the first error, or null.
		/// </summary>
		public PerforceError FirstError
		{
			get { return Errors.FirstOrDefault(); }
		}

		/// <summary>
		/// Sequence of all the data objects from the responses.
		/// </summary>
		public IEnumerable<T> Data
		{
			get { return this.Select(x => x.Data).Where(x => x != null); }
		}

		/// <summary>
		/// Sequence of all the error responses.
		/// </summary>
		public IEnumerable<PerforceError> Errors
		{
			get { return this.Where(x => !x.Succeeded).Select(x => x.Error); }
		}

		/// <summary>
		/// Throws an exception if any response is an error
		/// </summary>
		public void RequireSuccess()
		{
			foreach (PerforceResponse<T> Response in this)
			{
				Response.RequireSuccess();
			}
		}
	}
}
