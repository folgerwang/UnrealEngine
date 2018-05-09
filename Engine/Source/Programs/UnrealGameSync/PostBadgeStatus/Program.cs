// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace WriteBadgeStatus
{
	class Program
	{
		static int Main(string[] Args)
		{
			// Parse all the parameters
			List<string> Arguments = new List<string>(Args);
			string Name = ParseParam(Arguments, "Name");
			string Change = ParseParam(Arguments, "Change");
			string Project = ParseParam(Arguments, "Project");
			string RestUrl = ParseParam(Arguments, "RestUrl");
			string Status = ParseParam(Arguments, "Status");
			string Url = ParseParam(Arguments, "Url");

			// Check we've got all the arguments we need (and no more)
			if (Arguments.Count > 0 || Name == null || Change == null || Project == null || RestUrl == null || Status == null || Url == null)
			{
				Console.WriteLine("Syntax:");
				Console.WriteLine("  PostBadgeStatus -Name=<Name> -Change=<CL> -Project=<DepotPath> -RestUrl=<Url> -Status=<Status> -Url=<Url>");
				return 1;
			}
			if (RestUrl != null)
			{
				BuildData Build = new BuildData
				{
					BuildType = Name,
					Url = Url,
					Project = Project,
					ArchivePath = ""
				};
				if(!int.TryParse(Change, out Build.ChangeNumber))
				{
					Console.WriteLine("Change must be an integer!");
					return 1;
				}
				if (!Enum.TryParse<BuildData.BuildDataResult>(Status, true, out Build.Result))
				{
					Console.WriteLine("Change must be Starting, Failure, Warning, Success, or Skipped!");
					return 1;
				}
				try
				{
					SendRequest(RestUrl, "CIS", "POST", new JavaScriptSerializer().Serialize(Build));
				}
				catch(Exception ex)
				{
					Console.WriteLine(string.Format("An exception was thrown attempting to send the request: {0}", ex.Message));
					return 1;
				}
			}
			return 0;
		}

		static string ParseParam(List<string> Arguments, string ParamName)
		{
			string ParamPrefix = String.Format("-{0}=", ParamName);
			for (int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				if (Arguments[Idx].StartsWith(ParamPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					string ParamValue = Arguments[Idx].Substring(ParamPrefix.Length);
					Arguments.RemoveAt(Idx);
					return ParamValue;
				}
			}
			return null;
		}

		class BuildData
		{
			public enum BuildDataResult
			{
				Starting,
				Failure,
				Warning,
				Success,
				Skipped,
			}

			public int ChangeNumber;
			public string BuildType;
			public BuildDataResult Result;
			public string Url;
			public string Project;
			public string ArchivePath;

			public bool IsSuccess
			{
				get { return Result == BuildDataResult.Success || Result == BuildDataResult.Warning; }
			}

			public bool IsFailure
			{
				get { return Result == BuildDataResult.Failure; }
			}
		}

		static string SendRequest(string URI, string Resource, string Method, string RequestBody = null, params string[] QueryParams)
		{
			// set up the query string
			StringBuilder TargetURI = new StringBuilder(string.Format("{0}/api/{1}", URI, Resource));
			if (QueryParams.Length != 0)
			{
				TargetURI.Append("?");
				for (int Idx = 0; Idx < QueryParams.Length; Idx++)
				{
					TargetURI.Append(QueryParams[Idx]);
					if (Idx != QueryParams.Length - 1)
					{
						TargetURI.Append("&");
					}
				}
			}
			HttpWebRequest Request = (HttpWebRequest)WebRequest.Create(TargetURI.ToString());
			Request.ContentType = "application/json";
			Request.Method = Method;


			// Add json to request body
			if (!string.IsNullOrEmpty(RequestBody))
			{
				if (Method == "POST")
				{
					byte[] bytes = Encoding.ASCII.GetBytes(RequestBody);
					using (Stream RequestStream = Request.GetRequestStream())
					{
						RequestStream.Write(bytes, 0, bytes.Length);
					}
				}
			}
			try
			{
				WebResponse Repsonse = Request.GetResponse();
				string ResponseContent = null;
				using (StreamReader ResponseReader = new System.IO.StreamReader(Repsonse.GetResponseStream(), Encoding.Default))
				{
					ResponseContent = ResponseReader.ReadToEnd();
				}
				return ResponseContent;
			}
			catch (WebException ex)
			{
				if (ex.Response != null)
				{
					throw new Exception(string.Format("Request returned status: {0}, message: {1}", ((HttpWebResponse)ex.Response).StatusCode, ex.Message));
				}
				else
				{
					throw new Exception(string.Format("Request returned message: {0}", ex.InnerException.Message));
				}
			}
			catch (Exception ex)
			{
				throw new Exception(string.Format("Couldn't complete the request, error: {0}", ex.Message));
			}
		}
	}
}
