// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	enum PerforceRecordType
	{
		Invalid,
		Stat,
		Text,
		Binary,
	}

	enum PerforceFieldType
	{
		Record,
		EndRecord,
		String,
		Integer,
		EndStream
	}

	class PerforceChildProcess : IDisposable
	{
		ManagedProcessGroup ChildProcessGroup;
		ManagedProcess ChildProcess;
		byte[] Buffer;
		int BufferPos;
		int BufferEnd;
		bool bEndOfStream;

		public PerforceChildProcess(byte[] InputData, string Format, params object[] Args)
		{
			string PerforceFileName;
			if(Environment.OSVersion.Platform == PlatformID.Win32NT || Environment.OSVersion.Platform == PlatformID.Win32S || Environment.OSVersion.Platform == PlatformID.Win32Windows)
			{
				PerforceFileName = "p4.exe";
			}
			else
			{
				PerforceFileName = File.Exists("/usr/local/bin/p4")? "/usr/local/bin/p4" : "/usr/bin/p4";
			}

			string FullArgumentList = "-G " + String.Format(Format, Args);
			Log.TraceLog("Running {0} {1}", PerforceFileName, FullArgumentList);

			ChildProcessGroup = new ManagedProcessGroup();
			ChildProcess = new ManagedProcess(ChildProcessGroup, PerforceFileName, FullArgumentList, null, null, InputData, ProcessPriorityClass.Normal);

			Buffer = new byte[2048];
		}

		public void Dispose()
		{
			if(ChildProcess != null)
			{
				ChildProcess.Dispose();
				ChildProcess = null;
			}
			if(ChildProcessGroup != null)
			{
				ChildProcessGroup.Dispose();
				ChildProcessGroup = null;
			}
		}

		public bool IsEndOfStream()
		{
			if(!bEndOfStream)
			{
				bEndOfStream = (BufferPos == BufferEnd && !TryUpdateBuffer(1));
			}
			return bEndOfStream;
		}

		private string FormatUnexpectedData(byte NextByte)
		{
			// Read all the data to the buffer
			List<byte> Data = new List<byte>();
			for(int Idx = 0; Idx < 1024; Idx++)
			{
				Data.Add(NextByte);
				if(IsEndOfStream())
				{
					break;
				}
				NextByte = ReadByte();
			}

			// Check if it's printable. 
			bool bIsPrintable = true;
			for(int Idx = 0; Idx < Data.Count; Idx++)
			{
				if(Data[Idx] < 0x09 || (Data[Idx] >= 0x0e && Data[Idx] <= 0x1f) || Data[Idx] == 0x1f)
				{
					bIsPrintable = false;
				}
			}

			// Format the result
			StringBuilder Result = new StringBuilder();
			if(Data.Count > 0)
			{
				if(bIsPrintable)
				{
					Result.Append("\n    ");
					for(int Idx = 0; Idx < Data.Count; Idx++)
					{
						if(Data[Idx] == '\n')
						{
							Result.Append("\n    ");
						}
						else if(Data[Idx] != '\r')
						{
							Result.Append((char)Data[Idx]);
						}
					}
				}
				else
				{
					for(int Idx = 0; Idx < Data.Count; Idx++)
					{
						Result.AppendFormat("{0}{1:x2}", ((Idx & 31) == 0)? "\n    " : " ", Data[Idx]);
					}
				}
			}
			return Result.ToString();
		}

		public bool TryReadRecord(List<KeyValuePair<string, object>> Record)
		{
			// Check if we've reached the end of the stream. This is the only condition where we return false.
			if(IsEndOfStream())
			{
				return false;
			}

			// Check that a dictionary follows
			byte Temp = ReadByte();
			if(Temp != '{')
			{
				throw new PerforceException("Unexpected data while parsing marshaled output - expected '{{', got: {0}", FormatUnexpectedData(Temp));
			}

			// Read all the fields in the record
			Record.Clear();
			for(;;)
			{
				// Read the next field type. Perforce only outputs string records. A '0' character indicates the end of the dictionary.
				byte KeyFieldType = ReadByte();
				if(KeyFieldType == '0')
				{
					break;
				}
				else if(KeyFieldType != 's')
				{
					throw new PerforceException("Unexpected key field type while parsing marshalled output ({0}) - expected 's', got: {1}", (int)KeyFieldType, FormatUnexpectedData(KeyFieldType));
				}

				// Read the key
				string Key = ReadString();

				// Read the value type.
				byte ValueFieldType = ReadByte();
				if(ValueFieldType == 'i')
				{
					// An integer
					Record.Add(new KeyValuePair<string, object>(Key, ReadInt32()));
				}
				else if(ValueFieldType == 's')
				{
					// A string (or variable length data)
					byte[] Value = ReadBytes();
					if(Record.Count > 0 && Record[0].Key == "code" && ((string)Record[0].Value == "binary" || (string)Record[0].Value == "text"))
					{
						Record.Add(new KeyValuePair<string, object>(Key, Value));
					}
					else
					{
						Record.Add(new KeyValuePair<string, object>(Key, Encoding.UTF8.GetString(Value)));
					}
				}
				else
				{
					throw new PerforceException("Unexpected value field type while parsing marshalled output ({0}) - expected 's', got: {1}", (int)ValueFieldType, FormatUnexpectedData(ValueFieldType));
				}
			}
			return true;
		}

		public byte ReadByte()
		{
			UpdateBuffer(1);

			byte NextByte = Buffer[BufferPos++];
			return NextByte;
		}

		public int ReadInt32()
		{
			UpdateBuffer(4);

			int Value = Buffer[BufferPos + 0] | (Buffer[BufferPos + 1] << 8) | (Buffer[BufferPos + 2] << 16) | (Buffer[BufferPos + 3] << 24);
			BufferPos += 4;
			return Value;
		}

		public string ReadString()
		{
			byte[] Bytes = ReadBytes();
			return Encoding.UTF8.GetString(Bytes);
		}

		public byte[] ReadBytes()
		{
			int MaxNumBytes = ReadInt32();

			byte[] Buffer = new byte[MaxNumBytes];
			CopyBytes(MaxNumBytes, new MemoryStream(Buffer, true));
			return Buffer;
		}

		public void CopyBytes(int NumBytes, Stream OutputStream)
		{
			while(NumBytes > 0)
			{
				UpdateBuffer(1);
				int NumBytesThisLoop = Math.Min(NumBytes, BufferEnd - BufferPos);
				OutputStream.Write(Buffer, BufferPos, NumBytesThisLoop);
				BufferPos += NumBytesThisLoop;
				NumBytes -= NumBytesThisLoop;
			}
		}

		private void UpdateBuffer(int MinSize)
		{
			if(!TryUpdateBuffer(MinSize))
			{
				throw new PerforceException("Unexpected end of stream");
			}
		}

		private bool TryUpdateBuffer(int MinSize)
		{
			while(BufferPos + MinSize > BufferEnd)
			{
				if(BufferPos > 0)
				{
					Array.Copy(Buffer, BufferPos, Buffer, 0, BufferEnd - BufferPos);
					BufferEnd -= BufferPos;
					BufferPos = 0;
				}

				int ReadSize = ChildProcess.Read(Buffer, BufferEnd, Buffer.Length - BufferEnd);
				if(ReadSize == 0)
				{
					if(BufferPos == BufferEnd)
					{
						return false;
					}
					else
					{
						throw new PerforceException("Partially read record");
					}
				}

				BufferEnd += ReadSize;
			}
			return true;
		}
	}
}
