// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	public static class BinaryReaderExtensions
	{
		/// <summary>
		/// Read an objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the array</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <returns>Object instance.</returns>
		public static T ReadObject<T>(this BinaryReader Reader) where T : class, IBinarySerializable
		{
			return (T)Activator.CreateInstance(typeof(T), Reader);
		}

		/// <summary>
		/// Read an array of strings from a binary reader
		/// </summary>
		/// <param name="Reader">Reader to read data from</param>
		/// <returns>Array of strings, as serialized. May be null.</returns>
		public static string[] ReadStringArray(this BinaryReader Reader)
		{
			return Reader.ReadArray(() => Reader.ReadString());
		}

		/// <summary>
		/// Read an array of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the array</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <returns>Array of objects, as serialized. May be null.</returns>
		public static T[] ReadArray<T>(this BinaryReader Reader) where T : class, IBinarySerializable
		{
			return ReadArray<T>(Reader, () => Reader.ReadObject<T>());
		}

		/// <summary>
		/// Read an array of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the array</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadElement">Delegate to call to serialize each element</param>
		/// <returns>Array of objects, as serialized. May be null.</returns>
		public static T[] ReadArray<T>(this BinaryReader Reader, Func<T> ReadElement)
		{
			int NumItems = Reader.ReadInt32();
			if(NumItems < 0)
			{
				return null;
			}

			T[] Items = new T[NumItems];
			for(int Idx = 0; Idx < NumItems; Idx++)
			{
				Items[Idx] = ReadElement();
			}
			return Items;
		}

		/// <summary>
		/// Read a list of strings from a binary reader
		/// </summary>
		/// <param name="Reader">Reader to read data from</param>
		/// <returns>Array of strings, as serialized. May be null.</returns>
		public static List<string> ReadStringList(this BinaryReader Reader)
		{
			return Reader.ReadList(() => Reader.ReadString());
		}

		/// <summary>
		/// Read a list of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the list</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadElement">Delegate to call to serialize each element</param>
		/// <returns>List of objects, as serialized. May be null.</returns>
		public static List<T> ReadList<T>(this BinaryReader Reader) where T : class, IBinarySerializable
		{
			return ReadList<T>(Reader, () => Reader.ReadObject<T>());
		}

		/// <summary>
		/// Read a list of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the list</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadElement">Delegate to call to serialize each element</param>
		/// <returns>List of objects, as serialized. May be null.</returns>
		public static List<T> ReadList<T>(this BinaryReader Reader, Func<T> ReadElement)
		{
			int NumItems = Reader.ReadInt32();
			if(NumItems < 0)
			{
				return null;
			}

			List<T> Items = new List<T>(NumItems);
			for(int Idx = 0; Idx < NumItems; Idx++)
			{
				Items.Add(ReadElement());
			}
			return Items;
		}

		/// <summary>
		/// Read a list of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the list</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadKey">Delegate to call to serialize each key</param>
		/// <param name="ReadValue">Delegate to call to serialize each value</param>
		/// <returns>List of objects, as serialized. May be null.</returns>
		public static Dictionary<K, V> ReadDictionary<K, V>(this BinaryReader Reader, Func<K> ReadKey, Func<V> ReadValue)
		{
			int NumItems = Reader.ReadInt32();
			if(NumItems < 0)
			{
				return null;
			}

			Dictionary<K, V> Items = new Dictionary<K, V>(NumItems);
			for(int Idx = 0; Idx < NumItems; Idx++)
			{
				K Key = ReadKey();
				V Value = ReadValue();
				Items.Add(Key, Value);
			}
			return Items;
		}

		/// <summary>
		/// Read a nullable object from a binary reader
		/// </summary>
		/// <typeparam name="T">Type of the object</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadItem">Function to read the payload, if non-null</param>
		/// <returns>Object instance or null</returns>
		public static T ReadNullable<T>(this BinaryReader Reader, Func<T> ReadItem) where T : class
		{
			if(Reader.ReadBoolean())
			{
				return ReadItem();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Reads a value of a specific type from a binary reader
		/// </summary>
		/// <param name="Reader">Reader for input data</param>
		/// <param name="ObjectType">Type of value to read</param>
		/// <returns>The value read from the stream</returns>
		public static object ReadObject(this BinaryReader Reader, Type ObjectType)
		{
			if(ObjectType == typeof(string))
			{
				return Reader.ReadString();
			}
			else if(ObjectType == typeof(bool))
			{
				return Reader.ReadBoolean();
			}
			else if(ObjectType == typeof(int))
			{
				return Reader.ReadInt32();
			}
			else if(ObjectType == typeof(float))
			{
				return Reader.ReadSingle();
			}
			else if(ObjectType == typeof(double))
			{
				return Reader.ReadDouble();
			}
			else if(ObjectType == typeof(string[]))
			{
				return Reader.ReadStringArray();
			}
			else if(ObjectType == typeof(bool?))
			{
				int Value = Reader.ReadInt32();
				return (Value == -1)? (bool?)null : (Value == 0)? (bool?)false : (bool?)true;
			}
			else if(ObjectType.IsEnum)
			{
				return Enum.ToObject(ObjectType, Reader.ReadInt32());
			}
			else
			{
				throw new Exception(String.Format("Reading binary objects of type '{0}' is not currently supported.", ObjectType.Name));
			}
		}
	}
}
