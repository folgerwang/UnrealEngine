// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Interface that can be implemented by objects to support BinaryWriter container functionality. Also implies that
	/// derived objects will have a constructor taking a single BinaryReader argument (similar to how the system ISerializable
	/// interface assumes a specific constructor)
	/// </summary>
	public interface IBinarySerializable
	{
		void Write(BinaryWriter Writer);
	}

	/// <summary>
	/// Extension methods for BinaryWriter class
	/// </summary>
	public static class BinaryWriterExtensions
	{
		/// <summary>
		/// Writes an item implementing the IBinarySerializable interface to a binary writer. Included for symmetry with standard Writer.Write(X) calls.
		/// </summary>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Item">The item to write</param>
		public static void Write(this BinaryWriter Writer, IBinarySerializable Item)
		{
			Item.Write(Writer);
		}

		/// <summary>
		/// Writes an array of strings to a binary writer.
		/// </summary>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		public static void Write(this BinaryWriter Writer, string[] Items)
		{
			Write(Writer, Items, Item => Writer.Write(Item));
		}

		/// <summary>
		/// Writes an array to a binary writer.
		/// </summary>
		/// <typeparam name="T">The array element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		public static void Write<T>(this BinaryWriter Writer, T[] Items) where T : class, IBinarySerializable
		{
			Write(Writer, Items, Item => Writer.Write(Item));
		}

		/// <summary>
		/// Writes an array to a binary writer.
		/// </summary>
		/// <typeparam name="T">The array element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		/// <param name="WriteElement">Delegate to call to serialize each element</param>
		public static void Write<T>(this BinaryWriter Writer, T[] Items, Action<T> WriteElement)
		{
			if(Items == null)
			{
				Writer.Write(-1);
			}
			else
			{
				Writer.Write(Items.Length);
				for(int Idx = 0; Idx < Items.Length; Idx++)
				{
					WriteElement(Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Writes a list of strings to a binary writer.
		/// </summary>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		public static void Write(this BinaryWriter Writer, List<string> Items)
		{
			Write(Writer, Items, Item => Writer.Write(Item));
		}

		/// <summary>
		/// Writes a list to a binary writer.
		/// </summary>
		/// <typeparam name="T">The array element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		public static void Write<T>(this BinaryWriter Writer, List<T> Items) where T : class, IBinarySerializable
		{
			Write(Writer, Items, Item => Writer.Write(Item));
		}

		/// <summary>
		/// Writes a list to a binary writer.
		/// </summary>
		/// <typeparam name="T">The list element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">List of items</param>
		/// <param name="WriteElement">Delegate to call to serialize each element</param>
		public static void Write<T>(this BinaryWriter Writer, List<T> Items, Action<T> WriteElement)
		{
			if (Items == null)
			{
				Writer.Write(-1);
			}
			else
			{
				Writer.Write(Items.Count);
				for (int Idx = 0; Idx < Items.Count; Idx++)
				{
					WriteElement(Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Write a dictionary to a binary writer
		/// </summary>
		/// <typeparam name="K">The key type for the dictionary</typeparam>
		/// <typeparam name="V">The value type for the dictionary</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="Items">List of items to be written</param>
		/// <param name="WriteKey">Delegate to call to serialize each key</param>
		/// <param name="WriteKey">Delegate to call to serialize each value</param>
		/// <returns>Dictionary of objects, as serialized. May be null.</returns>
		public static void Write<K, V>(this BinaryWriter Writer, Dictionary<K, V> Items, Action<K> WriteKey, Action<V> WriteValue)
		{
			if(Items == null)
			{
				Writer.Write(-1);
			}
			else
			{
				Writer.Write(Items.Count);
				foreach(KeyValuePair<K, V> Item in Items)
				{
					WriteKey(Item.Key);
					WriteValue(Item.Value);
				}
			}
		}

		/// <summary>
		/// Read a nullable object from a binary reader
		/// </summary>
		/// <typeparam name="T">Type of the object</typeparam>
		/// <param name="Writer">Reader to read data from</param>
		/// <param name="WriteItem">Function to read the payload, if non-null</param>
		/// <returns>Object instance or null</returns>
		public static void WriteNullable<T>(this BinaryWriter Writer, T Item, Action WriteItem) where T : class
		{
			if(Item == null)
			{
				Writer.Write(false);
			}
			else
			{
				Writer.Write(true);
				WriteItem();
			}
		}

		/// <summary>
		/// Writes a value of a specific type to a binary writer
		/// </summary>
		/// <param name="Writer">Writer for output data</param>
		/// <param name="FieldType">Type of value to write</param>
		/// <param name="Value">The value to output</param>
		public static void Write(this BinaryWriter Writer, Type FieldType, object Value)
		{
			if(FieldType == typeof(string))
			{
				Writer.Write((string)Value);
			}
			else if(FieldType == typeof(bool))
			{
				Writer.Write((bool)Value);
			}
			else if(FieldType == typeof(int))
			{
				Writer.Write((int)Value);
			}
			else if(FieldType == typeof(float))
			{
				Writer.Write((float)Value);
			}
			else if(FieldType == typeof(double))
			{
				Writer.Write((double)Value);
			}
			else if(FieldType.IsEnum)
			{
				Writer.Write((int)Value);
			}
			else if(FieldType == typeof(string[]))
			{
				Writer.Write((string[])Value);
			}
			else if(FieldType == typeof(bool?))
			{
				bool? NullableValue = (bool?)Value;
				Writer.Write(NullableValue.HasValue? NullableValue.Value? 1 : 0 : -1);
			}
			else
			{
				throw new Exception(String.Format("Unsupported type '{0}' for binary serialization", FieldType.Name));
			}
		}
	}
}
