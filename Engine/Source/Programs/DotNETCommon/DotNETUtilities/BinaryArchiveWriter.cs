// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Writes data to a binary output stream. Similar to the NET Framework BinaryWriter class, but supports fast serialization of object graphs and container types, and supports nullable objects.
	/// </summary>
	public class BinaryArchiveWriter : IDisposable
	{
		/// <summary>
		/// Comparer which tests for reference equality between two objects
		/// </summary>
		class ReferenceComparer : IEqualityComparer<object>
		{
			bool IEqualityComparer<object>.Equals(object A, object B)
			{
				return A == B;
			}

			int IEqualityComparer<object>.GetHashCode(object X)
			{
				return RuntimeHelpers.GetHashCode(X);
			}
		}

		/// <summary>
		/// Instance of the ReferenceComparer class which can be shared by all archive writers
		/// </summary>
		static ReferenceComparer ReferenceComparerInstance = new ReferenceComparer();

		/// <summary>
		/// The output stream being written to
		/// </summary>
		Stream Stream;

		/// <summary>
		/// Buffer for data to be written to the stream
		/// </summary>
		byte[] Buffer;

		/// <summary>
		/// Current position within the output buffer
		/// </summary>
		int BufferPos;

		/// <summary>
		/// Map of object instance to unique id
		/// </summary>
		Dictionary<object, int> ObjectToUniqueId = new Dictionary<object, int>(ReferenceComparerInstance);

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Stream">The output stream</param>
		public BinaryArchiveWriter(Stream Stream)
		{
			this.Stream = Stream;
			this.Buffer = new byte[4096];
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="FileName">File to write to</param>
		public BinaryArchiveWriter(FileReference FileName)
			: this(File.Open(FileName.FullName, FileMode.Create, FileAccess.Write, FileShare.Read))
		{
		}

		/// <summary>
		/// Flushes this stream, and disposes the stream
		/// </summary>
		public void Dispose()
		{
			Flush();

			if(Stream != null)
			{
				Stream.Dispose();
				Stream = null;
			}
		}

		/// <summary>
		/// Writes all buffered data to disk
		/// </summary>
		public void Flush()
		{
			if(BufferPos > 0)
			{
				Stream.Write(Buffer, 0, BufferPos);
				BufferPos = 0;
			}
		}

		/// <summary>
		/// Ensures there is a minimum amount of space in the output buffer
		/// </summary>
		/// <param name="NumBytes">Minimum amount of space required in the output buffer</param>
		private void EnsureSpace(int NumBytes)
		{
			if(BufferPos + NumBytes > Buffer.Length)
			{
				Flush();
				if(NumBytes > Buffer.Length)
				{
					Buffer = new byte[NumBytes];
				}
			}
		}

		/// <summary>
		/// Writes a bool to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteBool(bool Value)
		{
			WriteByte(Value? (byte)1 : (byte)0);
		}

		/// <summary>
		/// Writes a single byte to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteByte(byte Value)
		{
			EnsureSpace(1);

			Buffer[BufferPos] = Value;

			BufferPos++;
		}

		/// <summary>
		/// Writes a single signed byte to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteSignedByte(sbyte Value)
		{
			WriteByte((byte)Value);
		}

		/// <summary>
		/// Writes a single short to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteShort(short Value)
		{
			WriteUnsignedShort((ushort)Value);
		}

		/// <summary>
		/// Writes a single unsigned short to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteUnsignedShort(ushort Value)
		{
			EnsureSpace(2);

			Buffer[BufferPos + 0] = (byte)Value;
			Buffer[BufferPos + 1] = (byte)(Value >> 8);

			BufferPos += 2;
		}

		/// <summary>
		/// Writes a single int to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteInt(int Value)
		{
			WriteUnsignedInt((uint)Value);
		}

		/// <summary>
		/// Writes a single unsigned int to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteUnsignedInt(uint Value)
		{
			EnsureSpace(4);

			Buffer[BufferPos + 0] = (byte)Value;
			Buffer[BufferPos + 1] = (byte)(Value >> 8);
			Buffer[BufferPos + 2] = (byte)(Value >> 16);
			Buffer[BufferPos + 3] = (byte)(Value >> 24);

			BufferPos += 4;
		}

		/// <summary>
		/// Writes a single long to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteLong(long Value)
		{
			WriteUnsignedLong((ulong)Value);
		}

		/// <summary>
		/// Writes a single unsigned long to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteUnsignedLong(ulong Value)
		{
			EnsureSpace(8);

			Buffer[BufferPos + 0] = (byte)Value;
			Buffer[BufferPos + 1] = (byte)(Value >> 8);
			Buffer[BufferPos + 2] = (byte)(Value >> 16);
			Buffer[BufferPos + 3] = (byte)(Value >> 24);
			Buffer[BufferPos + 4] = (byte)(Value >> 32);
			Buffer[BufferPos + 5] = (byte)(Value >> 40);
			Buffer[BufferPos + 6] = (byte)(Value >> 48);
			Buffer[BufferPos + 7] = (byte)(Value >> 56);

			BufferPos += 8;
		}

		/// <summary>
		/// Writes a string to the output
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteString(string Value)
		{
			byte[] Bytes;
			if(Value == null)
			{
				Bytes = null;
			}
			else
			{
				Bytes = Encoding.UTF8.GetBytes(Value);
			}
			WriteByteArray(Bytes);
		}

		/// <summary>
		/// Writes an array of bytes to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteByteArray(byte[] Data)
		{
			WritePrimitiveArray(Data, sizeof(byte));
		}

		/// <summary>
		/// Writes an array of shorts to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteShortArray(short[] Data)
		{
			WritePrimitiveArray(Data, sizeof(short));
		}

		/// <summary>
		/// Writes an array of ints to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteIntArray(int[] Data)
		{
			WritePrimitiveArray(Data, sizeof(int));
		}

		/// <summary>
		/// Writes an array of primitive types to the output.
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		private void WritePrimitiveArray<T>(T[] Data, int ElementSize) where T : struct
		{
			if(Data == null)
			{
				WriteInt(-1);
			}
			else
			{
				WriteInt(Data.Length);
				WriteBulkData(Data, Data.Length * ElementSize);
			}
		}

		/// <summary>
		/// Writes an array of bytes to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteFixedSizeByteArray(byte[] Data)
		{
			WriteFixedSizePrimitiveArray(Data, sizeof(byte));
		}

		/// <summary>
		/// Writes an array of shorts to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteFixedSizeShortArray(short[] Data)
		{
			WriteFixedSizePrimitiveArray(Data, sizeof(short));
		}

		/// <summary>
		/// Writes an array of ints to the output
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		public void WriteFixedSizeIntArray(int[] Data)
		{
			WriteFixedSizePrimitiveArray(Data, sizeof(int));
		}

		/// <summary>
		/// Writes an array of primitive types to the output.
		/// </summary>
		/// <param name="Data">Data to write. May be null.</param>
		private void WriteFixedSizePrimitiveArray<T>(T[] Data, int ElementSize) where T : struct
		{
			WriteBulkData(Data, Data.Length * ElementSize);
		}

		/// <summary>
		/// Writes primitive data from the given array to the output buffer.
		/// </summary>
		/// <param name="Data">Data to write.</param>
		/// <param name="Size">Size of the data, in bytes</param>
		private void WriteBulkData(Array Data, int Size)
		{
			if(Size > 0)
			{
				for(int Pos = 0;; )
				{
					int CopySize = Math.Min(Size - Pos, Buffer.Length - BufferPos);

					System.Buffer.BlockCopy(Data, Pos, Buffer, BufferPos, CopySize);
					BufferPos += CopySize;
					Pos += CopySize;

					if(Pos == Size)
					{
						break;
					}

					Flush();
				}
			}
		}

		/// <summary>
		/// Write an array of items to the archive
		/// </summary>
		/// <typeparam name="T">Type of the element</typeparam>
		/// <param name="Items">Array of items</param>
		/// <param name="WriteElement">Writes an individual element to the archive</param>
		public void WriteArray<T>(T[] Items, Action<T> WriteElement)
		{
			if(Items == null)
			{
				WriteInt(-1);
			}
			else
			{
				WriteInt(Items.Length);
				for(int Idx = 0; Idx < Items.Length; Idx++)
				{
					WriteElement(Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Write a list of items to the archive
		/// </summary>
		/// <typeparam name="T">Type of the element</typeparam>
		/// <param name="Items">List of items</param>
		/// <param name="WriteElement">Writes an individual element to the archive</param>
		public void WriteList<T>(List<T> Items, Action<T> WriteElement)
		{
			if(Items == null)
			{
				WriteInt(-1);
			}
			else
			{
				WriteInt(Items.Count);
				for(int Idx = 0; Idx < Items.Count; Idx++)
				{
					WriteElement(Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Writes a hashset of items
		/// </summary>
		/// <typeparam name="T">The element type for the set</typeparam>
		/// <param name="Set">The set to write</param>
		/// <param name="WriteElement">Delegate used to read a single element</param>
		public void WriteHashSet<T>(HashSet<T> Set, Action<T> WriteElement)
		{
			if(Set == null)
			{
				WriteInt(-1);
			}
			else
			{
				WriteInt(Set.Count);
				foreach(T Element in Set)
				{
					WriteElement(Element);
				}
			}
		}

		/// <summary>
		/// Writes a dictionary of items
		/// </summary>
		/// <typeparam name="K">Type of the dictionary key</typeparam>
		/// <typeparam name="V">Type of the dictionary value</typeparam>
		/// <param name="Dictionary">The dictionary to write</param>
		/// <param name="WriteKey">Delegate used to read a single key</param>
		/// <param name="WriteValue">Delegate used to read a single value</param>
		public void WriteDictionary<K, V>(Dictionary<K, V> Dictionary, Action<K> WriteKey, Action<V> WriteValue)
		{
			if(Dictionary == null)
			{
				WriteInt(-1);
			}
			else
			{
				WriteInt(Dictionary.Count);
				foreach(KeyValuePair<K, V> Pair in Dictionary)
				{
					WriteKey(Pair.Key);
					WriteValue(Pair.Value);
				}
			}
		}

		/// <summary>
		/// Writes a nullable object to the archive
		/// </summary>
		/// <typeparam name="T">The nullable type</typeparam>
		/// <param name="Item">Item to write</param>
		/// <param name="WriteValue">Delegate used to write a value</param>
		public void WriteNullable<T>(Nullable<T> Item, Action<T> WriteValue) where T : struct
		{
			if(Item.HasValue)
			{
				WriteBool(true);
				WriteValue(Item.Value);
			}
			else
			{
				WriteBool(false);
			}
		}

		/// <summary>
		/// Writes an object to the output, checking whether it is null or not. Does not preserve object references; each object written is duplicated.
		/// </summary>
		/// <typeparam name="T">Type of the object to serialize</typeparam>
		/// <param name="Object">Reference to check for null before serializing</param>
		/// <param name="WriteObject">Delegate used to write the object</param>
		public void WriteOptionalObject<T>(T Object, Action WriteObject) where T : class
		{
			if(Object == null)
			{
				WriteBool(false);
			}
			else
			{
				WriteBool(true);
				WriteObject();
			}
		}

		/// <summary>
		/// Writes an object to the output. If the specific instance has already been written, preserves the reference to that.
		/// </summary>
		/// <typeparam name="T">Type of the object to serialize</typeparam>
		/// <param name="Object">The object to serialize</param>
		/// <param name="WriteObject">Delegate used to write the object</param>
		public void WriteObjectReference<T>(T Object, Action WriteObject) where T : class
		{
			if(Object == null)
			{
				WriteInt(-1);
			}
			else
			{
				int Index;
				if(ObjectToUniqueId.TryGetValue(Object, out Index))
				{
					WriteInt(Index);
				}
				else
				{
					WriteInt(ObjectToUniqueId.Count);
					ObjectToUniqueId.Add(Object, ObjectToUniqueId.Count);
					WriteObject();
				}
			}
		}
	}
}
