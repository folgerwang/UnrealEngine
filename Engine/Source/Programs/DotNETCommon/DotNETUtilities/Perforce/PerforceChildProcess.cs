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
		Process ChildProcess;
		byte[] Buffer;
		int BufferPos;
		int BufferEnd;
		bool bEndOfStream;

		public PerforceChildProcess(byte[] InputData, string Format, params object[] Args)
		{
			ChildProcess = new Process();
			ChildProcess.StartInfo.FileName = "p4.exe";
			ChildProcess.StartInfo.Arguments = "-G " + String.Format(Format, Args);

			ChildProcess.StartInfo.RedirectStandardError = true;
			ChildProcess.StartInfo.RedirectStandardOutput = true;
			ChildProcess.StartInfo.RedirectStandardInput = InputData != null;
			ChildProcess.StartInfo.UseShellExecute = false;
			ChildProcess.StartInfo.CreateNoWindow = true;

			ChildProcess.Start();

			if(InputData != null)
			{
				ChildProcess.StandardInput.BaseStream.Write(InputData, 0, InputData.Length);
				ChildProcess.StandardInput.BaseStream.Close();
			}

			Buffer = new byte[2048];
		}

		public void Dispose()
		{
			if(ChildProcess != null)
			{
				try
				{
					if(!ChildProcess.HasExited)
					{
						ChildProcess.Kill();
						ChildProcess.WaitForExit();
					}
				}
				catch
				{
				}

				ChildProcess.Dispose();
				ChildProcess = null;
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
				throw new PerforceException("Unexpected data while parsing marshaled output - expected '{'");
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
					throw new PerforceException("Unexpected key field type while parsing marshalled output ({0}) - expected 's'", (int)KeyFieldType);
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
					throw new PerforceException("Unexpected value field type while parsing marshalled output ({0}) - expected 's'", (int)ValueFieldType);
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

				int ReadSize = ChildProcess.StandardOutput.BaseStream.Read(Buffer, BufferEnd, Buffer.Length - BufferEnd);
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
