// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Net;
using System.Net.Sockets;
using System.Diagnostics;
using System.Management;
using System.IO;
using System.Collections.Generic;


namespace nDisplay
{
	class nDisplayListener
	{
		const int MessageMaxLength = 2048;

		const string CmdStart   = "start";
		const string CmdKill    = "kill";
		const string CmdStatus  = "status";
		const string CmdRestart = "restart";

		const string ArgPort = "port=";

		static int Port = 41000;

		static HashSet<int> ProcIDs      = new HashSet<int>();
		static System.Object LockPIDList = new System.Object();


		static void Main(string[] args)
		{
			Console.SetWindowSize(120, 24);
			Console.SetBufferSize(120, 240);

			ParseCommandLine(args);

			TcpListener server = null;
			try
			{
				server = new TcpListener(IPAddress.Any, Port);
				server.Start();

				//@note: I'm note sure is it allowed to use copyright strings in the code
				//PrintColorText("Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.", ConsoleColor.Cyan);
				PrintColorText(string.Format("Listening to port {0}", Port.ToString()), ConsoleColor.Cyan);
				Console.WriteLine("---------------------------------------------------------");

				// Processing commands
				while (true)
				{
					TcpClient client = server.AcceptTcpClient();
					ProcessingRequest(client);
					client.Close();

					Console.WriteLine("---------------------------------------------------------\n\n");
				}
			}
			catch (SocketException e)
			{
				Console.WriteLine("SocketException: {0}", e);
			}
			finally
			{
				if(server != null)
					server.Stop();
			}
		}

		private static void PrintColorText(string message, ConsoleColor foregroundColor)
		{
			PrintColorText(message, foregroundColor, Console.BackgroundColor);
		}

		private static void PrintColorText(string message, ConsoleColor foregroundColor, ConsoleColor backgroundColor)
		{
			ConsoleColor oldClrF = Console.ForegroundColor;
			ConsoleColor oldClrB = Console.BackgroundColor;

			Console.ForegroundColor = foregroundColor;
			Console.BackgroundColor = backgroundColor;

			Console.WriteLine(message);

			Console.ForegroundColor = oldClrF;
			Console.BackgroundColor = oldClrB;
		}

		private static void ProcessingRequest(TcpClient client)
		{
			NetworkStream stream = client.GetStream();

			string clientIP = ((IPEndPoint)client.Client.RemoteEndPoint).Address.ToString();
			Console.WriteLine("Client [{0}] connected.", clientIP);
			PrintColorText(string.Format("New message from [{0}]", clientIP), ConsoleColor.Green);

			// Loop to receive all the data sent by the client.
			Byte[] data = new Byte[1024];
			int bytes = stream.Read(data, 0, data.Length);
			// Translate data bytes to a ASCII string.
			string cmd = System.Text.Encoding.ASCII.GetString(data, 0, bytes);

			int Result = ParseData(cmd);

			byte[] msg = BitConverter.GetBytes(Result);

			// Send back a response.
			stream.Write(msg, 0, msg.Length);
		}

		private static int ParseData(string data)
		{
			data = data.Trim();
			if (string.IsNullOrEmpty(data))
			{
				PrintColorText("Empty message received.", ConsoleColor.Yellow);
				return 1;
			}

			Console.WriteLine("Received command: [{0}]", data);

			if (data.StartsWith(CmdStart, StringComparison.OrdinalIgnoreCase))
			{
				data = data.Substring(CmdStart.Length);
				return StartApplication(data);
			}
			else if (data.StartsWith(CmdKill, StringComparison.OrdinalIgnoreCase))
			{
				return KillAll();
			}
			else if (data.StartsWith(CmdRestart, StringComparison.OrdinalIgnoreCase))
			{
				return RestartComputer();
			}
			else if (data.StartsWith(CmdStatus, StringComparison.OrdinalIgnoreCase))
			{
				return PrintStatus();
			}
			else
			{
				PrintColorText("Unknown command", ConsoleColor.Yellow);
				return 1;
			}
		}

		private static string ExtractApplicationPath(string data)
		{
			if (string.IsNullOrWhiteSpace(data))
			{
				return string.Empty;
			}

			data = data.Trim();

			int idxStart = 0;
			int idxEnd = 0;

			if (data.StartsWith("\"") && data.Length > 2)
			{
				idxEnd = data.IndexOf("\"", 1);
			}
			else
			{
				idxEnd = data.IndexOf(" ", 1);
			}

			if (idxEnd > 0)
			{
				return data.Substring(idxStart, idxEnd - idxStart + 1);
			}
			else
			{
				return string.Empty;
			}
		}

		private static int StartApplication(string data)
		{
			data = data.Trim();
			if (data == String.Empty)
			{
				PrintColorText("No data", ConsoleColor.Red);
				return 1;
			}

			try
			{
				string appPath = ExtractApplicationPath(data).Trim();
				string argList = data.Substring(appPath.Length).Trim();

				// For now we just forward arguments list as is.
				Process proc = new Process();
				proc.StartInfo.FileName = appPath;
				proc.StartInfo.Arguments = argList;
				proc.Start();

				lock (LockPIDList)
				{
					ProcIDs.Add(proc.Id);
				}

				PrintColorText(string.Format("Process started: {0} | {1}", proc.Id, proc.ProcessName), ConsoleColor.White);

				return 0;
			}
			catch (Exception e)
			{
				PrintColorText(e.Message, ConsoleColor.Red);
				return 1;
			}
		}

		private static int KillAll()
		{
			try
			{
				lock (LockPIDList)
				{
					foreach (int pid in ProcIDs)
					{
						try
						{
							KillProcessAndChildren(pid);
						}
						catch (Exception e)
						{
							PrintColorText(e.Message, ConsoleColor.Red);
						}
					}

					ProcIDs.Clear();
				}

				return 0;
			}
			catch (Exception e)
			{
				PrintColorText(e.Message, ConsoleColor.Red);
				return 1;
			}
		}

		private static void KillProcessAndChildren(int pid)
		{
			ManagementObjectSearcher searcher = new ManagementObjectSearcher("Select * From Win32_Process Where ParentProcessID=" + pid);
			ManagementObjectCollection moc = searcher.Get();

			foreach (ManagementObject mo in moc)
			{
				KillProcessAndChildren(Convert.ToInt32(mo["ProcessID"]));
			}

			KillProcessByPID(pid);
		}

		private static void KillProcessByPID(int PID)
		{
			try
			{
				Process proc = Process.GetProcessById(PID);

				proc.Kill();
				PrintColorText("Killed " + proc.ProcessName, ConsoleColor.White);
			}
			catch(Exception)
			{
				//PrintColorText(e.Message, ConsoleColor.Red);
			}
		}

		private static int RestartComputer()
		{
			try
			{
				Process proc = new Process();
				proc.StartInfo.FileName = "shutdown";
				proc.StartInfo.Arguments = "-r -f -t 00";
				proc.Start();
				return 0;
			}
			catch (Exception e)
			{
				PrintColorText(e.Message, ConsoleColor.Red);
				return 1;
			}
		}

		private static int PrintStatus()
		{
			int num = 0;
			foreach (int id in ProcIDs)
			{
				PrintColorText(string.Format("APP[{0}]: pid {1}", num++, id), ConsoleColor.Magenta);
			}

			return 0;
		}

		private static void ParseCommandLine(string[] args)
		{
			foreach (string arg in args)
			{
				try
				{
					if (arg.StartsWith(ArgPort, StringComparison.OrdinalIgnoreCase))
					{
						Port = int.Parse(arg.Substring(ArgPort.Length));
						return;
					}
				}
				catch (Exception e)
				{
					PrintColorText(e.Message, ConsoleColor.Red);
				}
			}
		}
	}
}
