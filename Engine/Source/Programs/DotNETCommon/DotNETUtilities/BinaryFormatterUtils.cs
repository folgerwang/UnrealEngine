// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Utility functions for serializing using the BinaryFormatter
	/// </summary>
	public static class BinaryFormatterUtils
	{
		/// <summary>
		/// Load an object from a file on disk, using the binary formatter
		/// </summary>
		/// <param name="Location">File to read from</param>
		/// <returns>Instance of the object that was read from disk</returns>
		public static T Load<T>(FileReference Location)
		{
			using (FileStream Stream = new FileStream(Location.FullName, FileMode.Open, FileAccess.Read))
			{
				BinaryFormatter Formatter = new BinaryFormatter();
				return (T)Formatter.Deserialize(Stream);
			}
		}

		/// <summary>
		/// Saves a file to disk, using the binary formatter
		/// </summary>
		/// <param name="Location">File to write to</param>
		/// <param name="Object">Object to serialize</param>
		public static void Save(FileReference Location, object Object)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (FileStream Stream = new FileStream(Location.FullName, FileMode.Create, FileAccess.Write))
			{
				BinaryFormatter Formatter = new BinaryFormatter();
				Formatter.Serialize(Stream, Object);
			}
		}

		/// <summary>
		/// Saves a file to disk using the binary formatter, without updating the timestamp if it hasn't changed
		/// </summary>
		/// <param name="Location">File to write to</param>
		/// <param name="Object">Object to serialize</param>
		public static void SaveIfDifferent(FileReference Location, object Object)
		{
			byte[] Contents;
			using(MemoryStream Stream = new MemoryStream())
			{
				BinaryFormatter Formatter = new BinaryFormatter();
				Formatter.Serialize(Stream, Object);
				Contents = Stream.ToArray();
			}

			DirectoryReference.CreateDirectory(Location.Directory);
			FileReference.WriteAllBytesIfDifferent(Location, Contents);
		}
	}
}
