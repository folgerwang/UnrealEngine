// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	enum AutomationRequestType
	{
		SyncProject,
		FindProject,
		OpenProject,
	}

	class AutomationRequestInput
	{
		public AutomationRequestType Type;
		public byte[] Data;

		public AutomationRequestInput(AutomationRequestType Type, byte[] Data)
		{
			this.Type = Type;
			this.Data = Data;
		}

		public static AutomationRequestInput Read(Stream InputStream)
		{
			BinaryReader Reader = new BinaryReader(InputStream);
			
			int Type = Reader.ReadInt32();
			int InputSize = Reader.ReadInt32();
			byte[] Input = Reader.ReadBytes(InputSize);

			return new AutomationRequestInput((AutomationRequestType)Type, Input);
		}

		public void Write(Stream OutputStream)
		{
			BinaryWriter Writer = new BinaryWriter(OutputStream);

			Writer.Write((int)Type);
			Writer.Write(Data.Length);
			Writer.Write(Data);
		}
	}

	enum AutomationRequestResult
	{
		Ok,
		Invalid,
		Busy,
		Canceled,
		Error,
		NotFound
	}

	class AutomationRequestOutput
	{
		public AutomationRequestResult Result;
		public byte[] Data;

		public AutomationRequestOutput(AutomationRequestResult Result)
		{
			this.Result = Result;
			this.Data = new byte[0];
		}

		public AutomationRequestOutput(AutomationRequestResult Result, byte[] Data)
		{
			this.Result = Result;
			this.Data = Data;
		}

		public static AutomationRequestOutput Read(Stream InputStream)
		{
			using(BinaryReader Reader = new BinaryReader(InputStream))
			{
				AutomationRequestResult Result = (AutomationRequestResult)Reader.ReadInt32();
				int DataSize = Reader.ReadInt32();
				byte[] Data = Reader.ReadBytes(DataSize);
				return new AutomationRequestOutput(Result, Data);
			}
		}

		public void Write(Stream OutputStream)
		{
			using(BinaryWriter Writer = new BinaryWriter(OutputStream))
			{
				Writer.Write((int)Result);
				Writer.Write(Data.Length);
				Writer.Write(Data);
			}
		}
	}

	class AutomationRequest : IDisposable
	{
		public AutomationRequestInput Input;
		public AutomationRequestOutput Output;
		public ManualResetEventSlim Complete;

		public AutomationRequest(AutomationRequestInput Input)
		{
			this.Input = Input;
			this.Complete = new ManualResetEventSlim(false);
		}

		public void SetOutput(AutomationRequestOutput Output)
		{
			this.Output = Output;
			Complete.Set();
		}

		public void Dispose()
		{
			if(Complete != null)
			{
				Complete.Dispose();
				Complete = null;
			}
		}
	}

	class AutomationServer : IDisposable
	{
		TcpListener Listener;
		TcpClient CurrentClient;
		Thread BackgroundThread;
		Action<AutomationRequest> PostRequest;
		bool bDisposing;
		TextWriter Log;

		public AutomationServer(Action<AutomationRequest> PostRequest, TextWriter Log)
		{
			this.PostRequest = PostRequest;
			this.Log = Log;

			object PortValue = Registry.GetValue("HKEY_CURRENT_USER\\Software\\Epic Games\\UnrealGameSync", "AutomationPort", null);
			if(PortValue != null)
			{
				try
				{
					int PortNumber = (int)PortValue;

					Listener = new TcpListener(IPAddress.Loopback, PortNumber);
					Listener.Start();

					BackgroundThread = new Thread(() => Run());
					BackgroundThread.Start();
				}
				catch(Exception Ex)
				{
					Log.WriteLine("Unable to start automation server: {0}", Ex.ToString());
				}
			}
		}

		public void Run()
		{
			try
			{
				for(;;)
				{
					TcpClient Client = CurrentClient = Listener.AcceptTcpClient();
					try
					{
						Log.WriteLine("Accepted connection from {0}", Client.Client.RemoteEndPoint);

						NetworkStream Stream = Client.GetStream();

						AutomationRequestInput Input = AutomationRequestInput.Read(Stream);
						Log.WriteLine("Received input: {0} (+{1} bytes)", Input.Type, Input.Data.Length);

						AutomationRequestOutput Output;
						using(AutomationRequest Request = new AutomationRequest(Input))
						{
							PostRequest(Request);
							Request.Complete.Wait();
							Output = Request.Output;
						}

						Output.Write(Stream);
						Log.WriteLine("Sent output: {0} (+{1} bytes)", Output.Result, Output.Data.Length);
					}
					catch(Exception Ex)
					{
						Log.WriteLine("Exception: {0}", Ex.ToString());
					}
					finally
					{
						Client.Close();
						Log.WriteLine("Closed connection.");
					}
					CurrentClient = null;
				}
			}
			catch(Exception Ex)
			{
				if(!bDisposing)
				{
					Log.WriteLine("Exception: {0}", Ex.ToString());
				}
			}
			Log.WriteLine("Closing socket.");
		}
		
		public void Dispose()
		{
			bDisposing = true;

			TcpClient Client = CurrentClient;
			if(Client != null)
			{
				try { Client.Close(); } catch { }
				Client = null;
			}

			if(Listener != null)
			{
				Listener.Stop();
				Listener = null;
			}

			if(BackgroundThread != null)
			{
				BackgroundThread.Join();
				BackgroundThread = null;
			}
		}
	}
}
