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
	/// Reads data from a binary output stream. Similar to the NET Framework BinaryReader class, but supports fast serialization of object graphs and container types, and supports nullable objects.
	/// Significantly faster than BinaryReader due to the expectation that the whole stream is in memory before deserialization.
	/// </summary>
	public class BinaryArchiveReader : IDisposable
	{
		/// <summary>
		/// The input stream.
		/// </summary>
		Stream Stream;

		/// <summary>
		/// The input buffer
		/// </summary>
		byte[] Buffer;

		/// <summary>
		/// Current position within the buffer
		/// </summary>
		int BufferPos;
		
		/// <summary>
		/// List of previously serialized objects
		/// </summary>
		List<object> Objects = new List<object>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Buffer">The buffer to read from</param>
		public BinaryArchiveReader(byte[] Buffer)
		{
			this.Buffer = Buffer;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="FileName">File to read from</param>
		public BinaryArchiveReader(FileReference FileName)
		{
			this.Buffer = FileReference.ReadAllBytes(FileName);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Stream">Stream to read from</param>
		public BinaryArchiveReader(Stream Stream)
		{
			Buffer = new byte[Stream.Length];
			Stream.Read(Buffer, 0, Buffer.Length);
		}

		/// <summary>
		/// Dispose of the stream owned by this reader
		/// </summary>
		public void Dispose()
		{
			if(Stream != null)
			{
				Stream.Dispose();
				Stream = null;
			}

			Buffer = null;
		}

		/// <summary>
		/// Reads a bool from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public bool ReadBool()
		{
			return ReadByte() != 0;
		}

		/// <summary>
		/// Reads a single byte from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public byte ReadByte()
		{
			byte Value = Buffer[BufferPos];
			BufferPos++;
			return Value;
		}

		/// <summary>
		/// Reads a single signed byte from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public sbyte ReadSignedByte()
		{
			return (sbyte)ReadByte();
		}

		/// <summary>
		/// Reads a single short from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public short ReadShort()
		{
			return (short)ReadUnsignedShort();
		}

		/// <summary>
		/// Reads a single unsigned short from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public ushort ReadUnsignedShort()
		{
			ushort Value = (ushort)(Buffer[BufferPos + 0] | (Buffer[BufferPos + 1] << 8));
			BufferPos += 2;
			return Value;
		}

		/// <summary>
		/// Reads a single int from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public int ReadInt()
		{
			return (int)ReadUnsignedInt();
		}

		/// <summary>
		/// Reads a single unsigned int from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public uint ReadUnsignedInt()
		{
			uint Value = (uint)(Buffer[BufferPos + 0] | (Buffer[BufferPos + 1] << 8) | (Buffer[BufferPos + 2] << 16) | (Buffer[BufferPos + 3] << 24));
			BufferPos += 4;
			return Value;
		}

		/// <summary>
		/// Reads a single long from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public long ReadLong()
		{
			return (long)ReadUnsignedLong();
		}

		/// <summary>
		/// Reads a single unsigned long from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public ulong ReadUnsignedLong()
		{
			ulong Value = (ulong)ReadUnsignedInt();
			Value |= (ulong)ReadUnsignedInt() << 32;
			return Value;
		}

		/// <summary>
		/// Reads a string from the stream
		/// </summary>
		/// <returns>The value that was read</returns>
		public string ReadString()
		{
			byte[] Bytes = ReadByteArray();
			if(Bytes == null)
			{
				return null;
			}
			else
			{
				return Encoding.UTF8.GetString(Bytes);
			}
		}

		/// <summary>
		/// Reads a byte array from the stream
		/// </summary>
		/// <returns>The data that was read</returns>
		public byte[] ReadByteArray()
		{
			return ReadPrimitiveArray<byte>(sizeof(byte));
		}

		/// <summary>
		/// Reads a short array from the stream
		/// </summary>
		/// <returns>The data that was read</returns>
		public short[] ReadShortArray()
		{
			return ReadPrimitiveArray<short>(sizeof(short));
		}

		/// <summary>
		/// Reads an int array from the stream
		/// </summary>
		/// <returns>The data that was read</returns>
		public int[] ReadIntArray()
		{
			return ReadPrimitiveArray<int>(sizeof(int));
		}

		/// <summary>
		/// Reads an array of primitive types from the stream
		/// </summary>
		/// <param name="ElementSize">Size of a single element</param>
		/// <returns>The data that was read</returns>
		private T[] ReadPrimitiveArray<T>(int ElementSize) where T : struct
		{
			int Length = ReadInt();
			if(Length < 0)
			{
				return null;
			}
			else
			{
				T[] Result = new T[Length];
				ReadBulkData(Result, Length * ElementSize);
				return Result;
			}
		}

		/// <summary>
		/// Reads a byte array from the stream
		/// </summary>
		/// <param name="Length">Length of the array to read</param>
		/// <returns>The data that was read</returns>
		public byte[] ReadFixedSizeByteArray(int Length)
		{
			return ReadFixedSizePrimitiveArray<byte>(sizeof(byte), Length);
		}

		/// <summary>
		/// Reads a short array from the stream
		/// </summary>
		/// <param name="Length">Length of the array to read</param>
		/// <returns>The data that was read</returns>
		public short[] ReadFixedSizeShortArray(int Length)
		{
			return ReadFixedSizePrimitiveArray<short>(sizeof(short), Length);
		}

		/// <summary>
		/// Reads an int array from the stream
		/// </summary>
		/// <param name="Length">Length of the array to read</param>
		/// <returns>The data that was read</returns>
		public int[] ReadFixedSizeIntArray(int Length)
		{
			return ReadFixedSizePrimitiveArray<int>(sizeof(int), Length);
		}

		/// <summary>
		/// Reads an array of primitive types from the stream
		/// </summary>
		/// <param name="ElementSize">Size of a single element</param>
		/// <param name="ElementCount">Number of elements to read</param>
		/// <returns>The data that was read</returns>
		private T[] ReadFixedSizePrimitiveArray<T>(int ElementSize, int ElementCount) where T : struct
		{
			T[] Result = new T[ElementCount];
			ReadBulkData(Result, ElementSize * ElementCount);
			return Result;
		}

		/// <summary>
		/// Reads bulk data from the stream into the given buffer
		/// </summary>
		/// <param name="Data">Array which receives the data that was read</param>
		/// <param name="Size">Size of data to read</param>
		private void ReadBulkData(Array Data, int Size)
		{
			System.Buffer.BlockCopy(Buffer, BufferPos, Data, 0, Size);
			BufferPos += Size;
		}


		/// <summary>
		/// Reads an array of items
		/// </summary>
		/// <returns>New array</returns>
		public T[] ReadArray<T>(Func<T> ReadElement)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				T[] Result = new T[Count];
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result[Idx] = ReadElement();
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a list of items
		/// </summary>
		/// <typeparam name="T">The element type for the list</typeparam>
		/// <param name="ReadElement">Delegate used to read a single element</param>
		/// <returns>List of items</returns>
		public List<T> ReadList<T>(Func<T> ReadElement)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				List<T> Result = new List<T>(Count);
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result.Add(ReadElement());
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a hashset of items
		/// </summary>
		/// <typeparam name="T">The element type for the set</typeparam>
		/// <param name="ReadElement">Delegate used to read a single element</param>
		/// <returns>Set of items</returns>
		public HashSet<T> ReadHashSet<T>(Func<T> ReadElement)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				HashSet<T> Result = new HashSet<T>();
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result.Add(ReadElement());
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a hashset of items
		/// </summary>
		/// <typeparam name="T">The element type for the set</typeparam>
		/// <param name="ReadElement">Delegate used to read a single element</param>
		/// <param name="Comparer">Comparison function for the set</param>
		/// <returns>Set of items</returns>
		public HashSet<T> ReadHashSet<T>(Func<T> ReadElement, IEqualityComparer<T> Comparer)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				HashSet<T> Result = new HashSet<T>(Comparer);
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result.Add(ReadElement());
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a dictionary of items
		/// </summary>
		/// <typeparam name="K">Type of the dictionary key</typeparam>
		/// <typeparam name="V">Type of the dictionary value</typeparam>
		/// <param name="ReadKey">Delegate used to read a single key</param>
		/// <param name="ReadValue">Delegate used to read a single value</param>
		/// <returns>New dictionary instance</returns>
		public Dictionary<K, V> ReadDictionary<K, V>(Func<K> ReadKey, Func<V> ReadValue)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				Dictionary<K, V> Result = new Dictionary<K, V>(Count);
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result.Add(ReadKey(), ReadValue());
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a dictionary of items
		/// </summary>
		/// <typeparam name="K">Type of the dictionary key</typeparam>
		/// <typeparam name="V">Type of the dictionary value</typeparam>
		/// <param name="ReadKey">Delegate used to read a single key</param>
		/// <param name="ReadValue">Delegate used to read a single value</param>
		/// <param name="Comparer">Comparison function for keys in the dictionary</param>
		/// <returns>New dictionary instance</returns>
		public Dictionary<K, V> ReadDictionary<K, V>(Func<K> ReadKey, Func<V> ReadValue, IEqualityComparer<K> Comparer)
		{
			int Count = ReadInt();
			if(Count < 0)
			{
				return null;
			}
			else
			{
				Dictionary<K, V> Result = new Dictionary<K, V>(Count, Comparer);
				for(int Idx = 0; Idx < Count; Idx++)
				{
					Result.Add(ReadKey(), ReadValue());
				}
				return Result;
			}
		}

		/// <summary>
		/// Reads a nullable object from the archive
		/// </summary>
		/// <typeparam name="T">The nullable type</typeparam>
		/// <param name="ReadValue">Delegate used to read a value</param>
		public Nullable<T> ReadNullable<T>(Func<T> ReadValue) where T : struct
		{
			if(ReadBool())
			{
				return new Nullable<T>(ReadValue());
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Reads an object, which may be null, from the archive. Does not handle de-duplicating object references. 
		/// </summary>
		/// <typeparam name="T">Type of the object to read</typeparam>
		/// <param name="Read">Delegate used to read the object</param>
		/// <returns>The object instance</returns>
		public T ReadOptionalObject<T>(Func<T> Read) where T : class
		{
			if(ReadBool())
			{
				return Read();
			}
			else
			{
				return null;
			}
		}

		/// <summary>
		/// Reads an object reference from the stream. Each referenced object will only be serialized once using the supplied delegates. Reading an object instance is
		/// done in two phases; first the object is created and its reference stored in the unique object list, then the object contents are read. This allows the object to 
		/// serialize a reference to itself.
		/// </summary>
		/// <typeparam name="T">Type of the object to read.</typeparam>
		/// <param name="CreateObject">Delegate used to create an object instance</param>
		/// <param name="ReadObject">Delegate used to read an object instance</param>
		/// <returns>Object instance</returns>
		public T ReadObjectReference<T>(Func<T> CreateObject, Action<T> ReadObject) where T : class
		{
			int Index = ReadInt();
			if(Index < 0)
			{
				return null;
			}
			else
			{
				if(Index == Objects.Count)
				{
					T Object = CreateObject();
					Objects.Add(Object);
					ReadObject(Object);
				}
				return (T)Objects[Index];
			}
		}

		/// <summary>
		/// Reads an object reference from the stream. Each object will only be serialized once using the supplied delegate; subsequent reads reference the original.
		/// Since the reader only receives the object reference when the CreateObject delegate returns, it is not possible for the object to serialize a reference to itself.
		/// </summary>
		/// <typeparam name="T">Type of the object to read.</typeparam>
		/// <param name="ReadObject">Delegate used to create an object instance. The object may not reference itself recursively.</param>
		/// <returns>Object instance</returns>
		public T ReadObjectReference<T>(Func<T> ReadObject) where T : class
		{
			int Index = ReadInt();
			if(Index < 0)
			{
				return null;
			}
			else
			{
				// Temporarily add the reader to the object list, so we can detect invalid recursive references. 
				if(Index == Objects.Count)
				{
					Objects.Add(null);
					Objects[Index] = ReadObject();
				}
				if(Objects[Index] == null)
				{
					throw new InvalidOperationException("Attempt to serialize reference to object recursively.");
				}
				return (T)Objects[Index];
			}
		}
	}
}
