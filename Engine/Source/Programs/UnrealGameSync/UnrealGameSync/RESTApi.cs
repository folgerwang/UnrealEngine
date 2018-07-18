// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace UnrealGameSync
{
	public static class RESTApi
	{
		private static string SendRequestInternal(string URI, string Resource, string Method, string RequestBody = null, params string[] QueryParams)
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

		public static string POST(string URI, string Resource, string RequestBody = null, params string[] QueryParams)
		{
			return SendRequestInternal(URI, Resource, "POST", RequestBody, QueryParams);
		}
		public static T GET<T>(string URI, string Resource, params string[] QueryParams)
		{
			return new JavaScriptSerializer().Deserialize<T>(SendRequestInternal(URI, Resource, "GET", null, QueryParams));
		}
	}
}
