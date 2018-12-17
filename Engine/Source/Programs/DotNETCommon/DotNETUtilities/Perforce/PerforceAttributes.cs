// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Attributes for fields that should be deserialized from P4 tags
	/// </summary>
	[AttributeUsage(AttributeTargets.Field)]
	public class PerforceTagAttribute : Attribute
	{
		/// <summary>
		/// The tag name
		/// </summary>
		public string Name;

		/// <summary>
		/// Whether this tag is required for a valid record
		/// </summary>
		public bool Optional = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the tag</param>
		public PerforceTagAttribute(string Name)
		{
			this.Name = Name;
		}
	}

	/// <summary>
	/// Specifies the name of an enum when converted into a P4 string
	/// </summary>
	[AttributeUsage(AttributeTargets.Field)]
	public class PerforceEnumAttribute : Attribute
	{
		/// <summary>
		/// Name of the enum value
		/// </summary>
		public string Name;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the serialized value</param>
		public PerforceEnumAttribute(string Name)
		{
			this.Name = Name;
		}
	}

	/// <summary>
	/// When attached to a list field, indicates that a list of structures can be included in the record
	/// </summary>
	[AttributeUsage(AttributeTargets.Field)]
	public class PerforceRecordListAttribute : Attribute
	{
	}
}
