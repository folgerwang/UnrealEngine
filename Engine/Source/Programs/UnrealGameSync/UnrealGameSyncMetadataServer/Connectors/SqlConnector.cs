// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Data.SQLite;
using System.IO;
using System.Text.RegularExpressions;
using UnrealGameSyncMetadataServer.Models;

namespace UnrealGameSyncMetadataServer.Connectors
{
	public static class SqlConnector
	{
		private static string ConnectionString = System.Configuration.ConfigurationManager.AppSettings["ConnectionString"];
		public static LatestData GetLastIds(string Project = null)
		{
			// Get ids going back 25 builds for the project being asked for
			// Do this by grouping by ChangeNumber to get unique entries, then take the 25th id
			long LastEventId = 0;
			long LastCommentId = 0;
			long LastBuildId = 0;
			string ProjectLikeString = "%" + (Project == null ? String.Empty : Project) + "%";
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id FROM [UserVotes] WHERE Project LIKE @param1 GROUP BY Changelist ORDER BY Changelist DESC LIMIT 1 OFFSET 432;", Connection))
				{
					Command.Parameters.AddWithValue("@param1", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							LastEventId = Reader.GetInt64(0);
							break;
						}
					}
				}
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id FROM [Comments] WHERE Project LIKE @param1 GROUP BY ChangeNumber ORDER BY ChangeNumber DESC LIMIT 1 OFFSET 432;", Connection))
				{
					Command.Parameters.AddWithValue("@param1", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							LastCommentId = Reader.GetInt32(0);
							break;
						}
					}
				}
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id FROM [CIS] WHERE Project LIKE @param1 GROUP BY ChangeNumber ORDER BY ChangeNumber DESC LIMIT 1 OFFSET 432", Connection))
				{
					Command.Parameters.AddWithValue("@param1", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							LastBuildId = Reader.GetInt32(0);
							break;
						}
					}
				}
			}
			return new LatestData { LastBuildId = LastBuildId, LastCommmentId = LastCommentId, LastEventId = LastEventId };
		}
		public static List<EventData> GetUserVotes(string Project, long LastEventId)
		{
			List<EventData> ReturnedEvents = new List<EventData>();
			string ProjectLikeString = "%" + (Project == null ? String.Empty : Project) + "%";
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id, Changelist, UserName, Verdict, Project FROM [UserVotes] WHERE Id > @param1 AND Project LIKE @param2 ORDER BY Id", Connection))
				{
					Command.Parameters.AddWithValue("@param1", LastEventId);
					Command.Parameters.AddWithValue("@param2", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							EventData Review = new EventData();
							Review.Id = Reader.GetInt64(0);
							Review.Change = Reader.GetInt32(1);
							Review.UserName = Reader.GetString(2);
							Review.Project = Reader.IsDBNull(4) ? null : Reader.GetString(4);
							if (Enum.TryParse(Reader.GetString(3), out Review.Type))
							{
								if (Review.Project == null || String.Compare(Review.Project, Project, true) == 0)
								{
									ReturnedEvents.Add(Review);
								}
							}
						}
					}
				}
			}
			return ReturnedEvents;
		}
		public static List<CommentData> GetComments(string Project, long LastCommentId)
		{
			List<CommentData> ReturnedComments = new List<CommentData>();
			string ProjectLikeString = "%" + (Project == null ? String.Empty : Project) + "%";
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id, ChangeNumber, UserName, Text, Project FROM [Comments] WHERE Id > @param1 AND Project LIKE @param2 ORDER BY Id", Connection))
				{
					Command.Parameters.AddWithValue("@param1", LastCommentId);
					Command.Parameters.AddWithValue("@param2", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							CommentData Comment = new CommentData();
							Comment.Id = Reader.GetInt32(0);
							Comment.ChangeNumber = Reader.GetInt32(1);
							Comment.UserName = Reader.GetString(2);
							Comment.Text = Reader.GetString(3);
							Comment.Project = Reader.GetString(4);
							if (Comment.Project == null || String.Compare(Comment.Project, Project, true) == 0)
							{
								ReturnedComments.Add(Comment);
							}
						}
					}
				}
			}
			return ReturnedComments;
		}

		public static List<BuildData> GetBuilds(string Project, long LastBuildId)
		{
			List<BuildData> ReturnedBuilds = new List<BuildData>();
			string ProjectLikeString = "%" + (Project == null ? String.Empty : Project) + "%";
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id, ChangeNumber, BuildType, Result, Url, Project FROM [CIS] WHERE Id > @param1 AND Project LIKE @param2 ORDER BY Id", Connection))
				{
					Command.Parameters.AddWithValue("@param1", LastBuildId);
					Command.Parameters.AddWithValue("@param2", ProjectLikeString);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							BuildData Build = new BuildData();
							Build.Id = Reader.GetInt32(0);
							Build.ChangeNumber = Reader.GetInt32(1);
							Build.BuildType = Reader.GetString(2).TrimEnd();
							if (Enum.TryParse(Reader.GetString(3).TrimEnd(), true, out Build.Result))
							{
								Build.Url = Reader.GetString(4);
								Build.Project = Reader.IsDBNull(5) ? null : Reader.GetString(5);
								if (Build.Project == null || String.Compare(Build.Project, Project, true) == 0 || MatchesWildcard(Build.Project, Project))
								{
									ReturnedBuilds.Add(Build);
								}
							}
							LastBuildId = Math.Max(LastBuildId, Build.Id);
						}
					}
				}
			}
			return ReturnedBuilds;
		}

		public static List<TelemetryErrorData> GetErrorData(int Records)
		{
			List<TelemetryErrorData> ReturnedErrors = new List<TelemetryErrorData>();
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("SELECT Id, Type, Text, UserName, Project, Timestamp, Version, IpAddress FROM [Errors] ORDER BY Id DESC LIMIT @param1", Connection))
				{
					Command.Parameters.AddWithValue("@param1", Records);
					using (SQLiteDataReader Reader = Command.ExecuteReader())
					{
						while (Reader.Read())
						{
							TelemetryErrorData Error = new TelemetryErrorData();
							Error.Id = Reader.GetInt32(0);
							Enum.TryParse(Reader.GetString(1), true, out Error.Type);
							Error.Text = Reader.GetString(2);
							Error.UserName = Reader.GetString(3);
							Error.Project = Reader.IsDBNull(4) ? null : Reader.GetString(4);
							Error.Timestamp = Reader.GetDateTime(5);
							Error.Version = Reader.GetString(6);
							Error.IpAddress = Reader.GetString(7);
							ReturnedErrors.Add(Error);
						}
					}
				}
			}
			return ReturnedErrors;
		}


		public static void PostBuild(BuildData Build)
		{
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("INSERT INTO [CIS] (ChangeNumber, BuildType, Result, URL, Project, ArchivePath) VALUES (@ChangeNumber, @BuildType, @Result, @URL, @Project, @ArchivePath)", Connection))
				{
					Command.Parameters.AddWithValue("@ChangeNumber", Build.ChangeNumber);
					Command.Parameters.AddWithValue("@BuildType", Build.BuildType);
					Command.Parameters.AddWithValue("@Result", Build.Result);
					Command.Parameters.AddWithValue("@URL", Build.Url);
					Command.Parameters.AddWithValue("@Project", Build.Project);
					Command.Parameters.AddWithValue("@ArchivePath", Build.ArchivePath);
					Command.ExecuteNonQuery();
				}
			}
		}

		public static void PostEvent(EventData Event)
		{
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("INSERT INTO [UserVotes] (Changelist, UserName, Verdict, Project) VALUES (@Changelist, @UserName, @Verdict, @Project)", Connection))
				{
					Command.Parameters.AddWithValue("@Changelist", Event.Change);
					Command.Parameters.AddWithValue("@UserName", Event.UserName.ToString());
					Command.Parameters.AddWithValue("@Verdict", Event.Type.ToString());
					Command.Parameters.AddWithValue("@Project", Event.Project);
					Command.ExecuteNonQuery();
				}
			}
		}

		public static void PostComment(CommentData Comment)
		{
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("INSERT INTO [Comments] (ChangeNumber, UserName, Text, Project) VALUES (@ChangeNumber, @UserName, @Text, @Project)", Connection))
				{
					Command.Parameters.AddWithValue("@ChangeNumber", Comment.ChangeNumber);
					Command.Parameters.AddWithValue("@UserName", Comment.UserName);
					Command.Parameters.AddWithValue("@Text", Comment.Text);
					Command.Parameters.AddWithValue("@Project", Comment.Project);
					Command.ExecuteNonQuery();
				}
			}
		}

		public static void PostTelemetryData(TelemetryTimingData Data, string Version, string IpAddress)
		{
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("INSERT INTO [Telemetry.v2] (Action, Result, UserName, Project, Timestamp, Duration, Version, IpAddress) VALUES (@Action, @Result, @UserName, @Project, @Timestamp, @Duration, @Version, @IpAddress)", Connection))
				{
					Command.Parameters.AddWithValue("@Action", Data.Action);
					Command.Parameters.AddWithValue("@Result", Data.Result);
					Command.Parameters.AddWithValue("@UserName", Data.UserName);
					Command.Parameters.AddWithValue("@Project", Data.Project);
					Command.Parameters.AddWithValue("@Timestamp", Data.Timestamp);
					Command.Parameters.AddWithValue("@Duration", Data.Duration);
					Command.Parameters.AddWithValue("@Version", Version);
					Command.Parameters.AddWithValue("@IPAddress", IpAddress);
					Command.ExecuteNonQuery();
				}
			}
		}

		public static void PostErrorData(TelemetryErrorData Data, string Version, string IpAddress)
		{
			using (SQLiteConnection Connection = new SQLiteConnection(ConnectionString))
			{
				Connection.Open();
				using (SQLiteCommand Command = new SQLiteCommand("INSERT INTO [Errors] (Type, Text, UserName, Project, Timestamp, Version, IpAddress) VALUES (@Type, @Text, @UserName, @Project, @Timestamp, @Version, @IpAddress)", Connection))
				{
					Command.Parameters.AddWithValue("@Type", Data.Type.ToString());
					Command.Parameters.AddWithValue("@Text", Data.Text);
					Command.Parameters.AddWithValue("@UserName", Data.UserName);
					if (Data.Project == null)
					{
						Command.Parameters.AddWithValue("@Project", DBNull.Value);
					}
					else
					{
						Command.Parameters.AddWithValue("@Project", Data.Project);
					}
					Command.Parameters.AddWithValue("@Timestamp", Data.Timestamp);
					Command.Parameters.AddWithValue("@Version", Version);
					Command.Parameters.AddWithValue("@IPAddress", IpAddress);
					Command.ExecuteNonQuery();
				}
			}
		}
		private static bool MatchesWildcard(string Wildcard, string Project)
		{
			return Wildcard.EndsWith("...") && Project.StartsWith(Wildcard.Substring(0, Wildcard.Length - 4), StringComparison.InvariantCultureIgnoreCase);
		}
	}
}