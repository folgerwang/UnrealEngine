// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;


namespace nDisplayLauncher.Cluster.Events
{
	[DataContract]
	public class ClusterEvent
	{
		[DataMember]
		public string Category { get; set; } = string.Empty;
		[DataMember]
		public string Type { get; set; } = string.Empty;
		[DataMember]
		public string Name { get; set; } = string.Empty;
		[DataMember]
		public Dictionary<string, string> Parameters = new Dictionary<string, string>();

		public ClusterEvent()
		{
		}

		public ClusterEvent(string category, string type, string name, Dictionary<string, string> parameters)
		{
			Category = category;
			Type = type;
			Name = name;
			Parameters = parameters;

			RebuildJsonStringForGui();
		}

		public void RebuildJsonStringForGui()
		{
			MemoryStream stream = new MemoryStream();
			DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(ClusterEvent), new DataContractJsonSerializerSettings()
			{
				UseSimpleDictionaryFormat = true
			});
			serializer.WriteObject(stream, this);
			_JsonData = Encoding.ASCII.GetString(stream.ToArray());
		}

		private string _JsonData;
		public string JsonData
		{
			get
			{
				return _JsonData;
			}

			private set
			{ }
		}

		public string SerializeToString()
		{
			string Serialized = string.Format("{0};{1};{2}", Category, Type, Name);
			foreach (KeyValuePair<string, string> Parameter in Parameters)
			{
				Serialized += string.Format(";{0}={1}", Parameter.Key, Parameter.Value);
			}

			return Serialized;
		}

		public void DeserializeFromString(string Serialized)
		{
			string[] parts = Serialized.Split(';');
			Category = parts[0];
			Type = parts[1];
			Name = parts[2];

			Parameters.Clear();
			for (int i = 3; i < parts.Length; ++i)
			{
				string[] Parameter = parts[i].Split('=');
				Parameters.Add(Parameter[0], Parameter[1]);
			}

			RebuildJsonStringForGui();
		}
	}
}
