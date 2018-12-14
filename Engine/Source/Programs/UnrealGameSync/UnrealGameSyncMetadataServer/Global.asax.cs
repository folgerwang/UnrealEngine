// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Data.SQLite;
using System.IO;
using System.Text.RegularExpressions;
using System.Web;
using System.Web.Http;

namespace UnrealGameSyncMetadataServer
{
	public class WebApiApplication : System.Web.HttpApplication
	{
		protected void Application_Start()
		{
			try
			{
				string ConnectionString = System.Configuration.ConfigurationManager.ConnectionStrings["ConnectionString"].ConnectionString;
				string FileName = Regex.Match(ConnectionString, "Data Source=(.+);.+").Groups[1].Value;
				Directory.CreateDirectory(new FileInfo(FileName).Directory.FullName);
				if (!File.Exists(FileName))
				{
					SQLiteConnection.CreateFile(FileName);
				}
				// initialize the db if it doesn't exist
				using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString, true))
				{
					Connection.Open();
					using (SQLiteCommand Command = new SQLiteCommand(Properties.Resources.Setup, Connection))
					{
						Command.ExecuteNonQuery();
					}
				}

			}
			catch(Exception)
			{
				HttpRuntime.UnloadAppDomain();
				return;
			}
			GlobalConfiguration.Configure(WebApiConfig.Register);
		}
	}
}
