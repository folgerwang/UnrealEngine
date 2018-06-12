// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
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
				string ConnectionString = System.Configuration.ConfigurationManager.AppSettings["ConnectionString"];
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
					string CreateTables = @"CREATE TABLE IF NOT EXISTS
										[CIS] (
											[Id]           INTEGER       NOT NULL PRIMARY KEY AUTOINCREMENT,
											[ChangeNumber] INTEGER       NOT NULL,
											[BuildType]    NCHAR (32)    NOT NULL,
											[Result]       NCHAR (10)    NOT NULL,
											[Url]          VARCHAR (512) NOT NULL,
											[Project]      VARCHAR (256) NULL,
											[ArchivePath]  VARCHAR (512) NULL
										);
										
										CREATE TABLE IF NOT EXISTS
										[Comments] (
											[Id]           INTEGER    	  NOT NULL PRIMARY KEY AUTOINCREMENT,
											[ChangeNumber] INTEGER        NOT NULL,
											[UserName]     VARCHAR (128)  NOT NULL,
											[Text]         VARCHAR (140)  NOT NULL,
											[Project]      NVARCHAR (256) NOT NULL
										);

										CREATE TABLE IF NOT EXISTS
										[Errors] (
											[Id]        INTEGER		   NOT NULL PRIMARY KEY AUTOINCREMENT,
											[Type]      VARCHAR (50)   NOT NULL,
											[Text]      VARCHAR (1024) NOT NULL,
											[UserName]  NVARCHAR (128) NOT NULL,
											[Project]   VARCHAR (128)  NULL,
											[Timestamp] DATETIME       NOT NULL,
											[Version]   VARCHAR (64)   NOT NULL,
											[IpAddress] VARCHAR (64)   NOT NULL
										);
										
										CREATE TABLE IF NOT EXISTS
										[Telemetry.v2] (
											[Id]        INTEGER		   NOT NULL PRIMARY KEY AUTOINCREMENT,
											[Action]    VARCHAR (128)  NOT NULL,
											[Result]    VARCHAR (128)  NOT NULL,
											[UserName]  NVARCHAR (128) NOT NULL,
											[Project]   VARCHAR (128)  NOT NULL,
											[Timestamp] DATETIME       NOT NULL,
											[Duration]  REAL           NOT NULL,
											[Version]   VARCHAR (64)   NULL,
											[IpAddress] VARCHAR (64)   NULL
										);

										CREATE TABLE IF NOT EXISTS
										[UserVotes] (
											[Id]         INTEGER		NOT NULL PRIMARY KEY AUTOINCREMENT,
											[Changelist] INTEGER        NOT NULL,
											[UserName]   NVARCHAR (128) NOT NULL,
											[Verdict]    NCHAR (32)     NOT NULL,
											[Project]    NVARCHAR (256) NULL
										);";
					using (SQLiteCommand Command = new SQLiteCommand(CreateTables, Connection))
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
