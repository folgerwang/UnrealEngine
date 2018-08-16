using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon.Perforce
{
	/// <summary>
	/// Stores settings for communicating with a Perforce server.
	/// </summary>
	public class PerforceConnection
	{
		/// <summary>
		/// Constant for the default changelist, where valid.
		/// </summary>
		public const int DefaultChange = -2;

		#region Plumbing

		/// <summary>
		/// Stores cached information about a field with a P4Tag attribute
		/// </summary>
		class CachedTagInfo
		{
			/// <summary>
			/// Name of the tag. Specified in the attribute or inferred from the field name.
			/// </summary>
			public string Name;

			/// <summary>
			/// Whether this tag is optional or not.
			/// </summary>
			public bool Optional;

			/// <summary>
			/// The field containing the value of this data.
			/// </summary>
			public FieldInfo Field;

			/// <summary>
			/// Index into the bitmask of required types
			/// </summary>
			public ulong RequiredTagBitMask;
		}

		/// <summary>
		/// Stores cached information about a record
		/// </summary>
		class CachedRecordInfo
		{
			/// <summary>
			/// Type of the record
			/// </summary>
			public Type Type;

			/// <summary>
			/// Map of tag names to their cached reflection information
			/// </summary>
			public Dictionary<string, CachedTagInfo> TagNameToInfo = new Dictionary<string, CachedTagInfo>();

			/// <summary>
			/// Bitmask of all the required tags. Formed by bitwise-or'ing the RequiredTagBitMask fields for each required CachedTagInfo.
			/// </summary>
			public ulong RequiredTagsBitMask;

			/// <summary>
			/// The type of records to create for subelements
			/// </summary>
			public Type SubElementType;

			/// <summary>
			/// The cached record info for the subelement type
			/// </summary>
			public CachedRecordInfo SubElementRecordInfo;

			/// <summary>
			/// Field containing subelements
			/// </summary>
			public FieldInfo SubElementField;
		}

		/// <summary>
		/// Unix epoch; used for converting times back into C# datetime objects
		/// </summary>
		static readonly DateTime UnixEpoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

		/// <summary>
		/// Cached map of enum types to a lookup mapping from p4 strings to enum values.
		/// </summary>
		static ConcurrentDictionary<Type, Dictionary<string, int>> EnumTypeToFlags = new ConcurrentDictionary<Type, Dictionary<string, int>>();

		/// <summary>
		/// Cached set of record 
		/// </summary>
		static ConcurrentDictionary<Type, CachedRecordInfo> RecordTypeToInfo = new ConcurrentDictionary<Type, CachedRecordInfo>();

		/// <summary>
		/// Default type for info
		/// </summary>
		static CachedRecordInfo InfoRecordInfo = GetCachedRecordInfo(typeof(PerforceInfo));

		/// <summary>
		/// Default type for errors
		/// </summary>
		static CachedRecordInfo ErrorRecordInfo = GetCachedRecordInfo(typeof(PerforceError));

		/// <summary>
		/// Global options for each command
		/// </summary>
		public readonly string GlobalOptions;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="GlobalOptions">Global options to pass to every Perforce command</param>
		public PerforceConnection(string GlobalOptions)
		{
			this.GlobalOptions = GlobalOptions;
		}

		/// <summary>
		/// Execute a Perforce command and parse the output as marshalled Python objects. This is more robustly defined than the text-based tagged output
		/// format, because it avoids ambiguity when returned fields can have newlines.
		/// </summary>
		/// <param name="CommandLine">Command line to execute Perforce with</param>
		/// <param name="InputData">Input data to pass to the Perforce server. May be null.</param>
		/// <param name="RecordHandler">Handler for each received record.</param>
		public void Command(string CommandLine, byte[] InputData, Action<List<KeyValuePair<string, object>>> RecordHandler)
		{
			using(PerforceChildProcess Process = new PerforceChildProcess(InputData, "{0} {1}", GlobalOptions, CommandLine))
			{
				List<KeyValuePair<string, object>> Record = new List<KeyValuePair<string, object>>();
				while(Process.TryReadRecord(Record))
				{
					RecordHandler(Record);
				}
			}
		}

		/// <summary>
		/// Serializes a list of key/value pairs into binary format.
		/// </summary>
		/// <param name="KeyValuePairs">List of key value pairs</param>
		/// <returns>Serialized record data</returns>
		static byte[] SerializeRecord(List<KeyValuePair<string, object>> KeyValuePairs)
		{
			MemoryStream Stream = new MemoryStream();
			using (BinaryWriter Writer = new BinaryWriter(Stream))
			{
				Writer.Write((byte)'{');
				foreach(KeyValuePair<string, object> KeyValuePair in KeyValuePairs)
				{
					Writer.Write('s');
					byte[] KeyBytes = Encoding.UTF8.GetBytes(KeyValuePair.Key);
					Writer.Write((int)KeyBytes.Length);
					Writer.Write(KeyBytes);

					if (KeyValuePair.Value is string)
					{
						Writer.Write('s');
						byte[] ValueBytes = Encoding.UTF8.GetBytes((string)KeyValuePair.Value);
						Writer.Write((int)ValueBytes.Length);
						Writer.Write(ValueBytes);
					}
					else
					{
						throw new PerforceException("Unsupported formatting type for {0}", KeyValuePair.Key);
					}
				}
				Writer.Write((byte)'0');
			}
			return Stream.ToArray();
		}

		/// <summary>
		/// Execute a command and parse the response
		/// </summary>
		/// <param name="Arguments">Arguments for the command</param>
		/// <param name="InputData">Input data to pass to Perforce</param>
		/// <param name="StatRecordType">The type of records to return for "stat" responses</param>
		/// <returns>List of objects returned by the server</returns>
		public List<PerforceResponse> Command(string Arguments, byte[] InputData, Type StatRecordType)
		{
			CachedRecordInfo StatRecordInfo = (StatRecordType == null)? null : GetCachedRecordInfo(StatRecordType);

			List<PerforceResponse> Responses = new List<PerforceResponse>();
			Action<List<KeyValuePair<string, object>>> Handler = (KeyValuePairs) =>
			{
				if(KeyValuePairs.Count == 0)
				{
					throw new PerforceException("Unexpected empty record returned by Perforce.");
				}
				if(KeyValuePairs[0].Key != "code")
				{
					throw new PerforceException("Expected first returned field to be 'code'");
				}

				string Code = KeyValuePairs[0].Value as string;

				int Idx = 1;
				if (Code == "stat" && StatRecordType != null)
				{
					Responses.Add(ParseResponse(KeyValuePairs, ref Idx, "", StatRecordInfo));
				}
				else if (Code == "info")
				{
					Responses.Add(ParseResponse(KeyValuePairs, ref Idx, "", InfoRecordInfo));
				}
				else if(Code == "error")
				{
					Responses.Add(ParseResponse(KeyValuePairs, ref Idx, "", ErrorRecordInfo));
				}
				else
				{
					throw new PerforceException("Unknown return code for record: {0}", KeyValuePairs[0].Value);
				}
			};
			Command(Arguments, InputData, Handler);

			return Responses;
		}

		/// <summary>
		/// Execute a command and parse the response
		/// </summary>
		/// <param name="Arguments">Arguments for the command</param>
		/// <param name="InputData">Input data to pass to Perforce</param>
		/// <returns>List of objects returned by the server</returns>
		public PerforceResponseList<T> Command<T>(string Arguments, byte[] InputData) where T : class
		{
			List<PerforceResponse> Responses = Command(Arguments, InputData, typeof(T));

			PerforceResponseList<T> TypedResponses = new PerforceResponseList<T>();
			foreach (PerforceResponse Response in Responses)
			{
				TypedResponses.Add(new PerforceResponse<T>(Response));
			}
			return TypedResponses;
		}

		/// <summary>
		/// Attempts to execute the given command, returning the results from the server or the first PerforceResponse object.
		/// </summary>
		/// <param name="Arguments">Arguments for the command.</param>
		/// <param name="InputData">Input data for the command.</param>
		/// <param name="StatRecordType">Type of element to return in the response</param>
		/// <returns>Response from the server; either an object of type T or error.</returns>
		public PerforceResponse SingleResponseCommand(string Arguments, byte[] InputData, Type StatRecordType)
		{
			List<PerforceResponse> Responses = Command(Arguments, InputData, StatRecordType);
			if(Responses.Count != 1)
			{
				throw new PerforceException("Expected one result from 'p4 {0}', got {1}", Arguments, Responses.Count);
			}
			return Responses[0];
		}

		/// <summary>
		/// Attempts to execute the given command, returning the results from the server or the first PerforceResponse object.
		/// </summary>
		/// <typeparam name="T">Type of record to parse</typeparam>
		/// <param name="Arguments">Arguments for the command.</param>
		/// <param name="InputData">Input data for the command.</param>
		/// <returns>Response from the server; either an object of type T or error.</returns>
		public PerforceResponse<T> SingleResponseCommand<T>(string Arguments, byte[] InputData) where T : class
		{
			return new PerforceResponse<T>(SingleResponseCommand(Arguments, InputData, typeof(T)));
		}

		/// <summary>
		/// Parse an individual record from the server.
		/// </summary>
		/// <param name="KeyValuePairs">List of tagged values returned by the server.</param>
		/// <param name="Idx">Index of the first tagged value to parse.</param>
		/// <param name="RequiredSuffix">The required suffix for any subobject arrays.</param>
		/// <param name="RecordInfo">Reflection information for the type being serialized into.</param>
		/// <returns>The parsed object.</returns>
		PerforceResponse ParseResponse(List<KeyValuePair<string, object>> KeyValuePairs, ref int Idx, string RequiredSuffix, CachedRecordInfo RecordInfo)
		{
			// Create a bitmask for all the required tags
			ulong RequiredTagsBitMask = 0;

			// Get the record info, and parse it into the object
			object NewRecord = Activator.CreateInstance(RecordInfo.Type);
			while(Idx < KeyValuePairs.Count)
			{
				// Split out the tag and value
				string Tag = KeyValuePairs[Idx].Key;
				string Value = KeyValuePairs[Idx].Value.ToString();

				// Parse the suffix from the current key
				int SuffixIdx = Tag.Length;
				while(SuffixIdx > 0 && (Tag[SuffixIdx - 1] == ',' || (Tag[SuffixIdx - 1] >= '0' && Tag[SuffixIdx - 1] <= '9')))
				{
					SuffixIdx--;
				}

				// Split out the suffix
				string Suffix = Tag.Substring(SuffixIdx);
				Tag = Tag.Substring(0, SuffixIdx);

				// Check whether it's a subobject or part of the current object.
				if (Suffix == RequiredSuffix)
				{
					// Part of the current object
					CachedTagInfo TagInfo;
					if (RecordInfo.TagNameToInfo.TryGetValue(Tag, out TagInfo))
					{
						FieldInfo FieldInfo = TagInfo.Field;
						if (FieldInfo.FieldType == typeof(DateTime))
						{
							DateTime Time;
							if(!DateTime.TryParse(Value, out Time))
							{
								Time = UnixEpoch + TimeSpan.FromSeconds(long.Parse(Value));
							}
							FieldInfo.SetValue(NewRecord, Time);
						}
						else if (FieldInfo.FieldType == typeof(bool))
						{
							FieldInfo.SetValue(NewRecord, Value.Length == 0 || Value == "true");
						}
						else if(FieldInfo.FieldType == typeof(Nullable<bool>))
						{
							FieldInfo.SetValue(NewRecord, Value == "true");
						}
						else if (FieldInfo.FieldType == typeof(int))
						{
							if(Value == "new" || Value == "none")
							{
								FieldInfo.SetValue(NewRecord, -1);
							}
							else if(Value.StartsWith("#"))
							{
								FieldInfo.SetValue(NewRecord, (Value == "#none") ? 0 : int.Parse(Value.Substring(1)));
							}
							else if(Value == "default")
							{
								FieldInfo.SetValue(NewRecord, DefaultChange);
							}
							else
							{
								FieldInfo.SetValue(NewRecord, int.Parse(Value));
							}
						}
						else if (FieldInfo.FieldType == typeof(long))
						{
							FieldInfo.SetValue(NewRecord, long.Parse(Value));
						}
						else if (FieldInfo.FieldType == typeof(string))
						{
							FieldInfo.SetValue(NewRecord, Value);
						}
						else if(FieldInfo.FieldType.IsEnum)
						{
							FieldInfo.SetValue(NewRecord, ParseEnum(FieldInfo.FieldType, Value));
						}
						else
						{
							throw new PerforceException("Unsupported type of {0}.{1} for tag '{0}'", RecordInfo.Type.Name, FieldInfo.FieldType.Name, Tag);
						}
						RequiredTagsBitMask |= TagInfo.RequiredTagBitMask;
					}
					Idx++;
				}
				else if (Suffix.StartsWith(RequiredSuffix) && (RequiredSuffix.Length == 0 || Suffix[RequiredSuffix.Length] == ','))
				{
					// Part of a subobject. If this record doesn't have any listed subobject type, skip the field and continue.
					if (RecordInfo.SubElementField == null)
					{
						CachedTagInfo TagInfo;
						if (RecordInfo.TagNameToInfo.TryGetValue(Tag, out TagInfo))
						{
							FieldInfo FieldInfo = TagInfo.Field;
							if (FieldInfo.FieldType == typeof(List<string>))
							{
								((List<string>)FieldInfo.GetValue(NewRecord)).Add(Value);
							}
							else
							{
								throw new PerforceException("Unsupported type of {0}.{1} for tag '{0}'", RecordInfo.Type.Name, FieldInfo.FieldType.Name, Tag);
							}
							RequiredTagsBitMask |= TagInfo.RequiredTagBitMask;
						}
						Idx++;
					}
					else
					{
						// Get the expected suffix for the next item based on the number of elements already in the list
						System.Collections.IList List = (System.Collections.IList)RecordInfo.SubElementField.GetValue(NewRecord);
						string RequiredChildSuffix = (RequiredSuffix.Length == 0) ? String.Format("{0}", List.Count) : String.Format("{0},{1}", RequiredSuffix, List.Count);
						if (Suffix != RequiredChildSuffix)
						{
							throw new PerforceException("Subobject element received out of order; expected {0}{1}, got {0}{2}", Tag, RequiredChildSuffix, Suffix);
						}

						// Parse the subobject and add it to the list
						PerforceResponse Response = ParseResponse(KeyValuePairs, ref Idx, RequiredChildSuffix, RecordInfo.SubElementRecordInfo);
						List.Add(Response.Data);
					}
				}
				else
				{
					break;
				}
			}

			// Make sure we've got all the required tags we need
			if (RequiredTagsBitMask != RecordInfo.RequiredTagsBitMask)
			{
				string MissingTagNames = String.Join(", ", RecordInfo.TagNameToInfo.Where(x => (RequiredTagsBitMask | x.Value.RequiredTagBitMask) != RequiredTagsBitMask).Select(x => x.Key));
				throw new PerforceException("Missing '{0}' tag when parsing '{1}'", MissingTagNames, RecordInfo.Type.Name);
			}

			return new PerforceResponse(NewRecord);
		}

		/// <summary>
		/// Gets a mapping of flags to enum values for the given type
		/// </summary>
		/// <param name="EnumType">The enum type to retrieve flags for</param>
		/// <returns>Map of name to enum value</returns>
		static Dictionary<string, int> GetCachedEnumFlags(Type EnumType)
		{
			Dictionary<string, int> NameToValue;
			if (!EnumTypeToFlags.TryGetValue(EnumType, out NameToValue))
			{
				NameToValue = new Dictionary<string, int>();

				FieldInfo[] Fields = EnumType.GetFields(BindingFlags.Public | BindingFlags.Static);
				foreach (FieldInfo Field in Fields)
				{
					PerforceEnumAttribute Attribute = Field.GetCustomAttribute<PerforceEnumAttribute>();
					if (Attribute != null)
					{
						NameToValue.Add(Attribute.Name, (int)Field.GetValue(null));
					}
				}

				if (!EnumTypeToFlags.TryAdd(EnumType, NameToValue))
				{
					NameToValue = EnumTypeToFlags[EnumType];
				}
			}
			return NameToValue;
		}

		/// <summary>
		/// Parses an enum value, using PerforceEnumAttribute markup for names.
		/// </summary>
		/// <param name="EnumType">Type of the enum to parse.</param>
		/// <param name="Value">Value of the enum.</param>
		/// <returns>Text for the enum.</returns>
		string GetEnumText(Type EnumType, object Value)
		{
			int IntegerValue = (int)Value;

			Dictionary<string, int> NameToValue = GetCachedEnumFlags(EnumType);
			if(EnumType.GetCustomAttribute<FlagsAttribute>() != null)
			{
				List<string> Names = new List<string>();
				foreach (KeyValuePair<string, int> Pair in NameToValue)
				{
					if ((IntegerValue & Pair.Value) != 0)
					{
						Names.Add(Pair.Key);
					}
				}
				return String.Join(" ", Names);
			}
			else
			{
				string Name = null;
				foreach (KeyValuePair<string, int> Pair in NameToValue)
				{
					if (IntegerValue == Pair.Value)
					{
						Name = Pair.Key;
						break;
					}
				}
				return Name;
			}
		}

		/// <summary>
		/// Parses an enum value, using PerforceEnumAttribute markup for names.
		/// </summary>
		/// <param name="EnumType">Type of the enum to parse.</param>
		/// <param name="Text">Text to parse.</param>
		/// <returns>The parsed enum value. Unknown values will be ignored.</returns>
		object ParseEnum(Type EnumType, string Text)
		{
			Dictionary<string, int> NameToValue = GetCachedEnumFlags(EnumType);
			if(EnumType.GetCustomAttribute<FlagsAttribute>() != null)
			{
				int Result = 0;
				foreach (string Item in Text.Split(' '))
				{
					int ItemValue;
					if (NameToValue.TryGetValue(Item, out ItemValue))
					{
						Result |= ItemValue;
					}
				}
				return Enum.ToObject(EnumType, Result);
			}
			else
			{
				int Result;
				NameToValue.TryGetValue(Text, out Result);
				return Enum.ToObject(EnumType, Result);
			}
		}

		/// <summary>
		/// Gets reflection data for the given record type
		/// </summary>
		/// <param name="RecordType">The type to retrieve record info for</param>
		/// <returns>The cached reflection information for the given type</returns>
		static CachedRecordInfo GetCachedRecordInfo(Type RecordType)
		{
			CachedRecordInfo Record;
			if (!RecordTypeToInfo.TryGetValue(RecordType, out Record))
			{
				Record = new CachedRecordInfo();
				Record.Type = RecordType;

				// Get all the fields for this type
				FieldInfo[] Fields = RecordType.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);

				// Build the map of all tags for this record
				foreach (FieldInfo Field in Fields)
				{
					PerforceTagAttribute TagAttribute = Field.GetCustomAttribute<PerforceTagAttribute>();
					if (TagAttribute != null)
					{
						CachedTagInfo Tag = new CachedTagInfo();
						Tag.Name = TagAttribute.Name ?? Field.Name;
						Tag.Optional = TagAttribute.Optional;
						Tag.Field = Field;
						if(!Tag.Optional)
						{
							Tag.RequiredTagBitMask = Record.RequiredTagsBitMask + 1;
							if(Tag.RequiredTagBitMask == 0)
							{
								throw new PerforceException("Too many required tags in {0}; max is {1}", RecordType.Name, sizeof(ulong) * 8);
							}
							Record.RequiredTagsBitMask |= Tag.RequiredTagBitMask;
						}
						Record.TagNameToInfo.Add(Tag.Name, Tag);
					}

					PerforceRecordListAttribute SubElementAttribute = Field.GetCustomAttribute<PerforceRecordListAttribute>();
					if(SubElementAttribute != null)
					{
						Record.SubElementField = Field;
						Record.SubElementType = Field.FieldType.GenericTypeArguments[0];
						Record.SubElementRecordInfo = GetCachedRecordInfo(Record.SubElementType);
					}
				}

				// Try to save the record info, or get the version that's already in the cache
				if(!RecordTypeToInfo.TryAdd(RecordType, Record))
				{
					Record = RecordTypeToInfo[RecordType];
				}
			}
			return Record;
		}

		#endregion

		#region p4 add

		/// <summary>
		/// Adds files to a pending changelist.
		/// </summary>
		/// <param name="ChangeNumber">Changelist to add files to</param>
		/// <param name="FileNames">Files to be added</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<AddRecord> Add(int ChangeNumber, params string[] FileNames)
		{
			return Add(ChangeNumber, null, AddOptions.None, FileNames);
		}

		/// <summary>
		/// Adds files to a pending changelist.
		/// </summary>
		/// <param name="ChangeNumber">Changelist to add files to</param>
		/// <param name="FileType">Type for new files</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileNames">Files to be added</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<AddRecord> Add(int ChangeNumber, string FileType, AddOptions Options, params string[] FileNames)
		{
			StringBuilder Arguments = new StringBuilder("add");
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if((Options & AddOptions.DowngradeToAdd) != 0)
			{
				Arguments.Append(" -d");
			}
			if((Options & AddOptions.IncludeWildcards) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & AddOptions.NoIgnore) != 0)
			{
				Arguments.Append(" -I");
			}
			if((Options & AddOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if(FileType != null)
			{
				Arguments.AppendFormat(" -t \"{0}\"", FileType);
			}
			foreach(string FileName in FileNames)
			{
				Arguments.AppendFormat(" \"{0}\"", FileName);
			}

			return Command<AddRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 change

		/// <summary>
		/// Creates a changelist with the p4 change command. 
		/// </summary>
		/// <param name="Record">Information for the change to create. The number field should be left set to -1.</param>
		/// <returns>The changelist number, or an error.</returns>
		public PerforceResponse<int> CreateChange(ChangeRecord Record)
		{
			if(Record.Number != -1)
			{
				throw new PerforceException("'Number' field should be set to -1 to create a new changelist.");
			}

			PerforceResponse Response = SingleResponseCommand("change -i", SerializeRecord(Record), null);
			if (Response.Failed)
			{
				return new PerforceResponse<int>(Response.Error);
			}

			string[] Tokens = Response.Info.Data.Split(' ');
			if (Tokens.Length != 3)
			{
				throw new PerforceException("Unexpected info response from change command: {0}", Response);
			}

			return new PerforceResponse<int>(int.Parse(Tokens[1]));
		}

		/// <summary>
		/// Updates an existing changelist.
		/// </summary>
		/// <param name="Options">Options for this command</param>
		/// <param name="Record">Information for the change to create. The number field should be left set to zero.</param>
		/// <returns>The changelist number, or an error.</returns>
		public PerforceResponse UpdateChange(UpdateChangeOptions Options, ChangeRecord Record)
		{
			if(Record.Number == -1)
			{
				throw new PerforceException("'Number' field must be set to update a changelist.");
			}

			StringBuilder Arguments = new StringBuilder("change -i");
			if((Options & UpdateChangeOptions.Force) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & UpdateChangeOptions.Submitted) != 0)
			{
				Arguments.Append(" -u");
			}

			return SingleResponseCommand(Arguments.ToString(), SerializeRecord(Record), null);
		}

		/// <summary>
		/// Deletes a changelist (p4 change -d)
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="ChangeNumber">Changelist number to delete</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse DeleteChange(DeleteChangeOptions Options, int ChangeNumber)
		{
			StringBuilder Arguments = new StringBuilder("change -d");
			if((Options & DeleteChangeOptions.Submitted) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & DeleteChangeOptions.BeforeRenumber) != 0)
			{
				Arguments.Append(" -O");
			}
			Arguments.AppendFormat(" {0}", ChangeNumber);

			return SingleResponseCommand(Arguments.ToString(), null, null);
		}

		/// <summary>
		/// Gets a changelist
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="ChangeNumber">Changelist number to retrieve. -1 is the default changelist for this workspace.</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse<ChangeRecord> GetChange(GetChangeOptions Options, int ChangeNumber)
		{
			StringBuilder Arguments = new StringBuilder("change -o");
			if((Options & GetChangeOptions.BeforeRenumber) != 0)
			{
				Arguments.Append(" -O");
			}
			Arguments.AppendFormat(" {0}", ChangeNumber);

			return SingleResponseCommand<ChangeRecord>(Arguments.ToString(), null);
		}

		byte[] SerializeRecord(ChangeRecord Input)
		{
			List<KeyValuePair<string, object>> NameToValue = new List<KeyValuePair<string, object>>();
			if (Input.Number == -1)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Change", "new"));
			}
			else
			{
				NameToValue.Add(new KeyValuePair<string, object>("Change", Input.Number.ToString()));
			}
			if (Input.Type != ChangeType.Unspecified)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Type", Input.Type.ToString()));
			}
			if (Input.User != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("User", Input.User));
			}
			if (Input.Client != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Client", Input.Client));
			}
			if (Input.Description != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Description", Input.Description));
			}
			return SerializeRecord(NameToValue);
		}

		#endregion

		#region p4 changes

		/// <summary>
		/// Enumerates changes on the server
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxChanges">List only the highest numbered changes</param>
		/// <param name="Status">Limit the list to the changelists with the given status (pending, submitted or shelved)</param>
		/// <param name="FileSpecs">Paths to query changes for</param>
		/// <returns>List of responses from the server.</returns>
		public PerforceResponseList<ChangesRecord> Changes(ChangesOptions Options, int MaxChanges, ChangeStatus Status, params string[] FileSpecs)
		{
			return Changes(Options, null, MaxChanges, Status, null, FileSpecs);
		}

		/// <summary>
		/// Enumerates changes on the server
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="ClientName">List only changes made from the named client workspace.</param>
		/// <param name="MaxChanges">List only the highest numbered changes</param>
		/// <param name="Status">Limit the list to the changelists with the given status (pending, submitted or shelved)</param>
		/// <param name="UserName">List only changes made by the named user</param>
		/// <param name="FileSpecs">Paths to query changes for</param>
		/// <returns>List of responses from the server.</returns>
		public PerforceResponseList<ChangesRecord> Changes(ChangesOptions Options, string ClientName, int MaxChanges, ChangeStatus Status, string UserName, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("changes");
			if ((Options & ChangesOptions.IncludeIntegrations) != 0)
			{
				Arguments.Append(" -i");
			}
			if ((Options & ChangesOptions.IncludeTimes) != 0)
			{
				Arguments.Append(" -t");
			}
			if ((Options & ChangesOptions.LongOutput) != 0)
			{
				Arguments.Append(" -l");
			}
			if ((Options & ChangesOptions.TruncatedLongOutput) != 0)
			{
				Arguments.Append(" -L");
			}
			if ((Options & ChangesOptions.IncludeRestricted) != 0)
			{
				Arguments.Append(" -f");
			}
			if(ClientName != null)
			{
				Arguments.AppendFormat(" -c \"{0}\"", ClientName);
			}
			if(MaxChanges != -1)
			{
				Arguments.AppendFormat(" -m {0}", MaxChanges);
			}
			if(Status != ChangeStatus.All)
			{
				Arguments.AppendFormat(" -s {0}", GetEnumText(typeof(ChangeStatus), Status));
			}
			if(UserName != null)
			{
				Arguments.AppendFormat(" -u {0}", UserName);
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}
			return Command<ChangesRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 client

		/// <summary>
		/// Creates a client
		/// </summary>
		/// <param name="Record">The client record</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse CreateClient(ClientRecord Record)
		{
			return UpdateClient(Record);
		}

		/// <summary>
		/// Creates a client
		/// </summary>
		/// <param name="Record">The client record</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse UpdateClient(ClientRecord Record)
		{
			return SingleResponseCommand("client -i", SerializeRecord(Record), null);
		}

		/// <summary>
		/// Deletes a client
		/// </summary>
		/// <param name="Options">Options for this command</param>
		/// <param name="ClientName">Name of the client</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse DeleteClient(DeleteClientOptions Options, string ClientName)
		{
			StringBuilder Arguments = new StringBuilder("client -d");
			if((Options & DeleteClientOptions.Force) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & DeleteClientOptions.DeleteShelved) != 0)
			{
				Arguments.Append(" -Fs");
			}
			Arguments.AppendFormat(" -d \"{0}\"", ClientName);
			return SingleResponseCommand<ClientRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Changes the stream associated with a client
		/// </summary>
		/// <param name="ClientName">The client name</param>
		/// <param name="StreamName">The new stream to be associated with the client</param>
		/// <param name="Options">Options for this command</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse SwitchClientToStream(string ClientName, string StreamName, SwitchClientOptions Options)
		{
			StringBuilder Arguments = new StringBuilder("client -s");
			if((Options & SwitchClientOptions.IgnoreOpenFiles) != 0)
			{
				Arguments.Append(" -f");
			}
			Arguments.AppendFormat(" -S \"{0}\"", StreamName);

			return SingleResponseCommand(Arguments.ToString(), null, null);
		}

		/// <summary>
		/// Changes a client to mirror a template
		/// </summary>
		/// <param name="ClientName">The client name</param>
		/// <param name="TemplateName">The new stream to be associated with the client</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse SwitchClientToTemplate(string ClientName, string TemplateName)
		{
			string Arguments = String.Format("client -s -t \"{0}\"", TemplateName);
			return SingleResponseCommand(Arguments, null, null);
		}

		/// <summary>
		/// Queries the current client definition
		/// </summary>
		/// <param name="ClientName">Name of the client. Specify null for the current client.</param>
		/// <returns>Response from the server; either a client record or error code</returns>
		public PerforceResponse<ClientRecord> GetClient(string ClientName)
		{
			StringBuilder Arguments = new StringBuilder("client -o");
			if(ClientName != null)
			{
				Arguments.AppendFormat(" \"{0}\"", ClientName);
			}
			return SingleResponseCommand<ClientRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Queries the view for a stream
		/// </summary>
		/// <param name="StreamName">Name of the stream.</param>
		/// <param name="ChangeNumber">Changelist at which to query the stream view</param>
		/// <returns>Response from the server; either a client record or error code</returns>
		public PerforceResponse<ClientRecord> GetStreamView(string StreamName, int ChangeNumber)
		{
			StringBuilder Arguments = new StringBuilder("client -o");
			Arguments.AppendFormat(" -S \"{0}\"", StreamName);
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			return SingleResponseCommand<ClientRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Serializes a client record to a byte array
		/// </summary>
		/// <param name="Input">The input record</param>
		/// <returns>Serialized record data</returns>
		byte[] SerializeRecord(ClientRecord Input)
		{
			List<KeyValuePair<string, object>> NameToValue = new List<KeyValuePair<string, object>>();
			if (Input.Name != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Client", Input.Name));
			}
			if (Input.Owner != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Owner", Input.Owner));
			}
			if (Input.Host != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Host", Input.Host));
			}
			if (Input.Description != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Description", Input.Description));
			}
			if (Input.Root != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Root", Input.Root));
			}
			if(Input.Options != ClientOptions.None)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Options", GetEnumText(typeof(ClientOptions), Input.Options)));
			}
			if(Input.SubmitOptions != ClientSubmitOptions.Unspecified)
			{
				NameToValue.Add(new KeyValuePair<string, object>("SubmitOptions", GetEnumText(typeof(ClientSubmitOptions), Input.SubmitOptions)));
			}
			if(Input.LineEnd != ClientLineEndings.Unspecified)
			{
				NameToValue.Add(new KeyValuePair<string, object>("LineEnd", GetEnumText(typeof(ClientLineEndings), Input.LineEnd)));
			}
			if(Input.Type != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Type", Input.Type));
			}
			if(Input.Stream != null)
			{
				NameToValue.Add(new KeyValuePair<string, object>("Stream", Input.Stream));
			}
			for (int Idx = 0; Idx < Input.View.Count; Idx++)
			{
				NameToValue.Add(new KeyValuePair<string, object>(String.Format("View{0}", Idx), Input.View[Idx]));
			}
			return SerializeRecord(NameToValue);
		}

		#endregion

		#region p4 clients

		/// <summary>
		/// Queries the current client definition
		/// </summary>
		/// <param name="Options">Options for this command</param>
		/// <param name="UserName">List only client workspaces owned by this user.</param>
		/// <returns>Response from the server; either a client record or error code</returns>
		public PerforceResponseList<ClientsRecord> Clients(ClientsOptions Options, string UserName)
		{
			return Clients(Options, null, -1, null, UserName);
		}

		/// <summary>
		/// Queries the current client definition
		/// </summary>
		/// <param name="Options">Options for this command</param>
		/// <param name="Filter">List only client workspaces matching filter. Treated as case sensitive if <ref>ClientsOptions.CaseSensitiveFilter</ref> is set.</param>
		/// <param name="MaxResults">Limit the number of results to return. -1 for all.</param>
		/// <param name="Stream">List client workspaces associated with the specified stream.</param>
		/// <param name="UserName">List only client workspaces owned by this user.</param>
		/// <returns>Response from the server; either a client record or error code</returns>
		public PerforceResponseList<ClientsRecord> Clients(ClientsOptions Options, string Filter, int MaxResults, string Stream, string UserName)
		{
			StringBuilder Arguments = new StringBuilder("clients");
			if((Options & ClientsOptions.All) != 0)
			{
				Arguments.Append(" -a");
			}
			if (Filter != null)
			{
				if ((Options & ClientsOptions.CaseSensitiveFilter) != 0)
				{
					Arguments.AppendFormat(" -e \"{0}\"", Filter);
				}
				else
				{
					Arguments.AppendFormat(" -E \"{0}\"", Filter);
				}
			}
			if(MaxResults != -1)
			{
				Arguments.AppendFormat(" -m {0}", MaxResults);
			}
			if(Stream != null)
			{
				Arguments.AppendFormat(" -S \"{0}\"", Stream);
			}
			if((Options & ClientsOptions.WithTimes) != 0)
			{
				Arguments.Append(" -t");
			}
			if(UserName != null)
			{
				Arguments.AppendFormat(" -u \"{0}\"", UserName);
			}
			if((Options & ClientsOptions.Unloaded) != 0)
			{
				Arguments.Append(" -U");
			}
			return Command<ClientsRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 delete

		/// <summary>
		/// Execute the 'delete' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<DeleteRecord> Delete(int ChangeNumber, DeleteOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("delete");
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if((Options & DeleteOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if((Options & DeleteOptions.KeepWorkspaceFiles) != 0)
			{
				Arguments.Append(" -k");
			}
			if((Options & DeleteOptions.WithoutSyncing) != 0)
			{
				Arguments.Append(" -v");
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}
			return Command<DeleteRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 describe

		/// <summary>
		/// Describes a single changelist
		/// </summary>
		/// <param name="ChangeNumber">The changelist number to retrieve description for</param>
		/// <returns>Response from the server; either a describe record or error code</returns>
		public PerforceResponse<DescribeRecord> Describe(int ChangeNumber)
		{
			PerforceResponseList<DescribeRecord> Records = Describe(new int[] { ChangeNumber });
			if (Records.Count != 1)
			{
				throw new PerforceException("Expected only one record returned from p4 describe command, got {0}", Records.Count);
			}
			return Records[0];
		}

		/// <summary>
		/// Describes a set of changelists
		/// </summary>
		/// <param name="ChangeNumbers">The changelist numbers to retrieve descriptions for</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<DescribeRecord> Describe(params int[] ChangeNumbers)
		{
			StringBuilder Arguments = new StringBuilder("describe -s");
			foreach(int ChangeNumber in ChangeNumbers)
			{
				Arguments.AppendFormat(" {0}", ChangeNumber);
			}
			return Command<DescribeRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Describes a set of changelists
		/// </summary>
		/// <param name="ChangeNumbers">The changelist numbers to retrieve descriptions for</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<DescribeRecord> Describe(DescribeOptions Options, int MaxResults, params int[] ChangeNumbers)
		{
			StringBuilder Arguments = new StringBuilder("describe -s");
			if((Options & DescribeOptions.ShowDescriptionForRestrictedChanges) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & DescribeOptions.Identity) != 0)
			{
				Arguments.Append(" -I");
			}
			if(MaxResults != -1)
			{
				Arguments.AppendFormat(" -m{0}", MaxResults);
			}
			if((Options & DescribeOptions.OriginalChangeNumber) != 0)
			{
				Arguments.Append(" -O");
			}
			if((Options & DescribeOptions.Shelved) != 0)
			{
				Arguments.Append(" -S");
			}
			foreach(int ChangeNumber in ChangeNumbers)
			{
				Arguments.AppendFormat(" {0}", ChangeNumber);
			}
			return Command<DescribeRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 edit

		/// <summary>
		/// Opens files for edit
		/// </summary>
		/// <param name="ChangeNumber">Changelist to add files to</param>
		/// <param name="FileSpecs">Files to be opened for edit</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<EditRecord> Edit(int ChangeNumber, params string[] FileSpecs)
		{
			return Edit(ChangeNumber, null, EditOptions.None, FileSpecs);
		}

		/// <summary>
		/// Opens files for edit
		/// </summary>
		/// <param name="ChangeNumber">Changelist to add files to</param>
		/// <param name="FileType">Type for new files</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to be opened for edit</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<EditRecord> Edit(int ChangeNumber, string FileType, EditOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("edit");
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if((Options & EditOptions.KeepWorkspaceFiles) != 0)
			{
				Arguments.Append(" -k");
			}
			if((Options & EditOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if(FileType != null)
			{
				Arguments.AppendFormat(" -t \"{0}\"", FileType);
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<EditRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 filelog

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FileLogRecord> FileLog(FileLogOptions Options, params string[] FileSpecs)
		{
			return FileLog(-1, -1, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="MaxChanges">Number of changelists to show. Ignored if zero or negative.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FileLogRecord> FileLog(int MaxChanges, FileLogOptions Options, params string[] FileSpecs)
		{
			return FileLog(-1, MaxChanges, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'filelog' command
		/// </summary>
		/// <param name="ChangeNumber">Show only files modified by this changelist. Ignored if zero or negative.</param>
		/// <param name="MaxChanges">Number of changelists to show. Ignored if zero or negative.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FileLogRecord> FileLog(int ChangeNumber, int MaxChanges, FileLogOptions Options, params string[] FileSpecs)
		{
			// Build the argument list
			StringBuilder Arguments = new StringBuilder("filelog");
			if(ChangeNumber > 0)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if((Options & FileLogOptions.ContentHistory) != 0)
			{
				Arguments.Append(" -h");
			}
			if((Options & FileLogOptions.FollowAcrossBranches) != 0)
			{
				Arguments.Append(" -i");
			}
			if((Options & FileLogOptions.FullDescriptions) != 0)
			{
				Arguments.Append(" -l");
			}
			if((Options & FileLogOptions.LongDescriptions) != 0)
			{
				Arguments.Append(" -L");
			}
			if(MaxChanges > 0)
			{
				Arguments.AppendFormat(" -m {0}", MaxChanges);
			}
			if((Options & FileLogOptions.DoNotFollowPromotedTaskStreams) != 0)
			{
				Arguments.Append(" -p");
			}
			if((Options & FileLogOptions.IgnoreNonContributoryIntegrations) != 0)
			{
				Arguments.Append(" -s");
			}

			// Always include times to simplify parsing
			Arguments.Append(" -t");

			// Add the file arguments
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			// Execute the command
			return Command<FileLogRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 fstat

		/// <summary>
		/// Execute the 'fstat' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FStatRecord> FStat(FStatOptions Options, params string[] FileSpecs)
		{
			return FStat(-1, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'fstat' command
		/// </summary>
		/// <param name="MaxFiles">Produce fstat output for only the first max files.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FStatRecord> FStat(int MaxFiles, FStatOptions Options, params string[] FileSpecs)
		{
			return FStat(-1, -1, null, MaxFiles, Options, FileSpecs);
		}

		/// <summary>
		/// Execute the 'fstat' command
		/// </summary>
		/// <param name="AfterChangeNumber">Return only files affected after the given changelist number.</param>
		/// <param name="OnlyChangeNumber">Return only files affected by the given changelist number.</param>
		/// <param name="Filter">List only those files that match the criteria specified.</param>
		/// <param name="MaxFiles">Produce fstat output for only the first max files.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">List of file specifications to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<FStatRecord> FStat(int AfterChangeNumber, int OnlyChangeNumber, string Filter, int MaxFiles, FStatOptions Options, params string[] FileSpecs)
		{
			// Build the argument list
			StringBuilder Arguments = new StringBuilder("fstat");
			if (AfterChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", AfterChangeNumber);
			}
			if(OnlyChangeNumber != -1)
			{
				Arguments.AppendFormat(" -e {0}", OnlyChangeNumber);
			}
			if(Filter != null)
			{
				Arguments.AppendFormat(" -F \"{0}\"", Filter);
			}
			if((Options & FStatOptions.ReportDepotSyntax) != 0)
			{
				Arguments.Append(" -L");
			}
			if((Options & FStatOptions.AllRevisions) != 0)
			{
				Arguments.Append(" -Of");
			}
			if((Options & FStatOptions.IncludeFileSizes) != 0)
			{
				Arguments.Append(" -Ol");
			}
			if((Options & FStatOptions.ClientFileInPerforceSyntax) != 0)
			{
				Arguments.Append(" -Op");
			}
			if((Options & FStatOptions.ShowPendingIntegrations) != 0)
			{
				Arguments.Append(" -Or");
			}
			if((Options & FStatOptions.ShortenOutput) != 0)
			{
				Arguments.Append(" -Os");
			}
			if((Options & FStatOptions.ReverseOrder) != 0)
			{
				Arguments.Append(" -r");
			}
			if((Options & FStatOptions.OnlyMapped) != 0)
			{
				Arguments.Append(" -Rc");
			}
			if((Options & FStatOptions.OnlyHave) != 0)
			{
				Arguments.Append(" -Rh");
			}
			if((Options & FStatOptions.OnlyOpenedBeforeHead) != 0)
			{
				Arguments.Append(" -Rn");
			}
			if((Options & FStatOptions.OnlyOpenInWorkspace) != 0)
			{
				Arguments.Append(" -Ro");
			}
			if((Options & FStatOptions.OnlyOpenAndResolved) != 0)
			{
				Arguments.Append(" -Rr");
			}
			if((Options & FStatOptions.OnlyShelved) != 0)
			{
				Arguments.Append(" -Rs");
			}
			if((Options & FStatOptions.OnlyUnresolved) != 0)
			{
				Arguments.Append(" -Ru");
			}
			if((Options & FStatOptions.SortByDate) != 0)
			{
				Arguments.Append(" -Sd");
			}
			if((Options & FStatOptions.SortByHaveRevision) != 0)
			{
				Arguments.Append(" -Sh");
			}
			if((Options & FStatOptions.SortByHeadRevision) != 0)
			{
				Arguments.Append(" -Sr");
			}
			if((Options & FStatOptions.SortByFileSize) != 0)
			{
				Arguments.Append(" -Ss");
			}
			if((Options & FStatOptions.SortByFileType) != 0)
			{
				Arguments.Append(" -St");
			}
			if((Options & FStatOptions.IncludeFilesInUnloadDepot) != 0)
			{
				Arguments.Append(" -U");
			}

			// Add the file arguments
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			// Execute the command
			return Command<FStatRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 info

		/// <summary>
		/// Execute the 'info' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <returns>Response from the server; an InfoRecord or error code</returns>
		public PerforceResponse<InfoRecord> Info(InfoOptions Options)
		{
			// Build the argument list
			StringBuilder Arguments = new StringBuilder("info");
			if((Options & InfoOptions.ShortOutput) != 0)
			{
				Arguments.Append(" -s");
			}
			return SingleResponseCommand<InfoRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 opened

		/// <summary>
		/// Execute the 'opened' command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="ChangeNumber">List the files in pending changelist change. To list files in the default changelist, use DefaultChange.</param>
		/// <param name="ClientName">List only files that are open in the given client</param>
		/// <param name="UserName">List only files that are opened by the given user</param>
		/// <param name="MaxResults">Maximum number of results to return</param>
		/// <param name="FileSpecs">Specification for the files to list</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<FStatRecord> Opened(OpenedOptions Options, int ChangeNumber, string ClientName, string UserName, int MaxResults, params string[] FileSpecs)
		{
			// Build the argument list
			StringBuilder Arguments = new StringBuilder("opened");
			if((Options & OpenedOptions.AllWorkspaces) != 0)
			{
				Arguments.AppendFormat(" -a");
			}
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if(ClientName != null)
			{
				Arguments.AppendFormat(" -C \"{0}\"", ClientName);
			}
			if(UserName != null)
			{
				Arguments.AppendFormat(" -u \"{0}\"", UserName);
			}
			if(MaxResults == DefaultChange)
			{
				Arguments.AppendFormat(" -m default");
			}
			else if(MaxResults != -1)
			{
				Arguments.AppendFormat(" -m {0}", MaxResults);
			}
			if((Options & OpenedOptions.ShortOutput) != 0)
			{
				Arguments.AppendFormat(" -s");
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}
			return Command<FStatRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 print

		/// <summary>
		/// Execute the 'print' command
		/// </summary>
		/// <param name="OutputFile">Output file to redirect output to</param>
		/// <param name="FileSpec">Specification for the files to print</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse<PrintRecord> Print(string OutputFile, string FileSpec)
		{
			// Build the argument list
			StringBuilder Arguments = new StringBuilder("print");
			Arguments.AppendFormat(" -o \"{0}\"", OutputFile);
			Arguments.AppendFormat(" \"{0}\"", FileSpec);
			return SingleResponseCommand<PrintRecord>(Arguments.ToString(), null);
		}

		class PrintHandler : IDisposable
		{
			Dictionary<string, string> DepotFileToLocalFile;
			FileStream OutputStream;

			public PrintHandler(Dictionary<string, string> DepotFileToLocalFile)
			{
				this.DepotFileToLocalFile = DepotFileToLocalFile;
			}

			public void Dispose()
			{
				CloseStream();
			}

			private void OpenStream(string FileName)
			{
				CloseStream();
				Directory.CreateDirectory(Path.GetDirectoryName(FileName));
				OutputStream = File.Open(FileName, FileMode.Create, FileAccess.Write, FileShare.None);
			}

			private void CloseStream()
			{
				if(OutputStream != null)
				{
					OutputStream.Dispose();
					OutputStream = null;
				}
			}

			public void HandleRecord(List<KeyValuePair<string, object>> Fields)
			{
				if(Fields[0].Key != "code")
				{
					throw new Exception("Missing code field");
				}

				string Value = (string)Fields[0].Value;
				if(Value == "stat")
				{
					string DepotFile = Fields.First(x => x.Key == "depotFile").Value.ToString();

					string LocalFile;
					if(!DepotFileToLocalFile.TryGetValue(DepotFile, out LocalFile))
					{
						throw new PerforceException("Depot file '{0}' not found in input dictionary", DepotFile);
					}

					OpenStream(LocalFile);
				}
				else if(Value == "binary" || Value == "text")
				{
					byte[] Data = (byte[])Fields.First(x => x.Key == "data").Value;
					OutputStream.Write(Data, 0, Data.Length);
				}
				else
				{
					throw new Exception("Unexpected record type");
				}
			}
		}

		/// <summary>
		/// Execute the 'print' command
		/// </summary>
		/// <param name="OutputFile">Output file to redirect output to</param>
		/// <param name="FileSpec">Specification for the files to print</param>
		/// <returns>Response from the server</returns>
		public void Print(Dictionary<string, string> DepotFileToLocalFile)
		{
			string ListFileName = Path.GetTempFileName();
			try
			{
				// Write the list of depot files
				File.WriteAllLines(ListFileName, DepotFileToLocalFile.Keys);

				// Execute Perforce, consuming the binary output into a memory stream
				using(PrintHandler Handler = new PrintHandler(DepotFileToLocalFile))
				{
					Command(String.Format("-x \"{0}\" print", ListFileName), null, Handler.HandleRecord);
				}
			}
			finally
			{
				File.Delete(ListFileName);
			}
		}

		#endregion

		#region p4 reconcile
		
		/// <summary>
		/// Open files for add, delete, and/or edit in order to reconcile a workspace with changes made outside of Perforce.
		/// </summary>
		/// <param name="ChangeNumber">Changelist to open files to</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to be reverted</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<ReconcileRecord> Reconcile(int ChangeNumber, ReconcileOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("reconcile");
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if((Options & ReconcileOptions.Edit) != 0)
			{
				Arguments.Append(" -e");
			}
			if((Options & ReconcileOptions.Add) != 0)
			{
				Arguments.Append(" -a");
			}
			if((Options & ReconcileOptions.Delete) != 0)
			{
				Arguments.Append(" -d");
			}
			if((Options & ReconcileOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if((Options & ReconcileOptions.AllowWildcards) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & ReconcileOptions.NoIgnore) != 0)
			{
				Arguments.Append(" -I");
			}
			if((Options & ReconcileOptions.LocalFileSyntax) != 0)
			{
				Arguments.Append(" -l");
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<ReconcileRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 resolve

		/// <summary>
		/// Resolve conflicts between file revisions.
		/// </summary>
		/// <param name="ChangeNumber">Changelist to open files to</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to be reverted</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<ResolveRecord> Resolve(int ChangeNumber, ResolveOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("resolve");
			if ((Options & ResolveOptions.Automatic) != 0)
			{
				Arguments.Append(" -am");
			}
			if ((Options & ResolveOptions.AcceptYours) != 0)
			{
				Arguments.Append(" -ay");
			}
			if ((Options & ResolveOptions.AcceptTheirs) != 0)
			{
				Arguments.Append(" -at");
			}
			if ((Options & ResolveOptions.SafeAccept) != 0)
			{
				Arguments.Append(" -as");
			}
			if ((Options & ResolveOptions.ForceAccept) != 0)
			{
				Arguments.Append(" -af");
			}
			if((Options & ResolveOptions.IgnoreWhitespaceOnly) != 0)
			{
				Arguments.Append(" -db");
			}
			if((Options & ResolveOptions.IgnoreWhitespace) != 0)
			{
				Arguments.Append(" -dw");
			}
			if((Options & ResolveOptions.IgnoreLineEndings) != 0)
			{
				Arguments.Append(" -dl");
			}
			if ((Options & ResolveOptions.ResolveAgain) != 0)
			{
				Arguments.Append(" -f");
			}
			if ((Options & ResolveOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<ResolveRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 revert

		/// <summary>
		/// Reverts files that have been added to a pending changelist.
		/// </summary>
		/// <param name="ChangeNumber">Changelist to add files to</param>
		/// <param name="ClientName">Revert another user’s open files. </param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to be reverted</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<RevertRecord> Revert(int ChangeNumber, string ClientName, RevertOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("revert");
			if((Options & RevertOptions.Unchanged) != 0)
			{
				Arguments.Append(" -a");
			}
			if((Options & RevertOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if((Options & RevertOptions.KeepWorkspaceFiles) != 0)
			{
				Arguments.Append(" -k");
			}
			if((Options & RevertOptions.DeleteAddedFiles) != 0)
			{
				Arguments.Append(" -w");
			}
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			if(ClientName != null)
			{
				Arguments.AppendFormat(" -C \"{0}\"", ClientName);
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<RevertRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 shelve

		/// <summary>
		/// Shelves a set of files
		/// </summary>
		/// <param name="ChangeNumber">The change number to receive the shelved files</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<ShelveRecord> Shelve(int ChangeNumber, ShelveOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("shelve");
			Arguments.AppendFormat(" -c {0}", ChangeNumber);
			if((Options & ShelveOptions.OnlyChanged) != 0)
			{
				Arguments.Append(" -a leaveunchanged");
			}
			if((Options & ShelveOptions.Overwrite) != 0)
			{
				Arguments.Append(" -f");
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<ShelveRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Deletes files from a shelved changelist
		/// </summary>
		/// <param name="ChangeNumber">Changelist containing shelved files to be deleted</param>
		/// <param name="FileSpecs">Files to delete</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse DeleteShelvedFiles(int ChangeNumber, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("shelve -d");
			if(ChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", ChangeNumber);
			}
			foreach (string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return SingleResponseCommand(Arguments.ToString(), null, null);
		}

		#endregion

		#region p4 streams

		/// <summary>
		/// Enumerates all streams in a depot
		/// </summary>
		/// <param name="StreamPath">The path for streams to enumerate (eg. "//UE4/...")</param>
		/// <returns>List of streams matching the given criteria</returns>
		public PerforceResponseList<StreamRecord> Streams(string StreamPath)
		{
			return Streams(StreamPath, -1, null, false);
		}

		/// <summary>
		/// Enumerates all streams in a depot
		/// </summary>
		/// <param name="StreamPath">The path for streams to enumerate (eg. "//UE4/...")</param>
		/// <param name="MaxResults">Maximum number of results to return</param>
		/// <param name="Filter">Additional filter to be applied to the results</param>
		/// <param name="bUnloaded">Whether to enumerate unloaded workspaces</param>
		/// <returns>List of streams matching the given criteria</returns>
		public PerforceResponseList<StreamRecord> Streams(string StreamPath, int MaxResults, string Filter, bool bUnloaded)
		{
			// Build the command line
			StringBuilder Arguments = new StringBuilder("streams");
			if (bUnloaded)
			{
				Arguments.Append(" -U");
			}
			if (Filter != null)
			{
				Arguments.AppendFormat("-F \"{0}\"", Filter);
			}
			if (MaxResults > 0)
			{
				Arguments.AppendFormat("-m {0}", MaxResults);
			}
			Arguments.AppendFormat(" \"{0}\"", StreamPath);

			// Execute the command
			return Command<StreamRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 submit

		/// <summary>
		/// Submits a pending changelist
		/// </summary>
		/// <param name="ChangeNumber">The changelist to submit</param>
		/// <param name="Options">Options for the command</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse<SubmitRecord> Submit(int ChangeNumber, SubmitOptions Options)
		{
			StringBuilder Arguments = new StringBuilder("submit");
			if((Options & SubmitOptions.ReopenAsEdit) != 0)
			{
				Arguments.Append(" -r");
			}
			Arguments.AppendFormat(" -c {0}", ChangeNumber);

			return Command<SubmitRecord>(Arguments.ToString(), null)[0];
		}

		#endregion

		#region p4 sync

		/// <summary>
		/// Syncs files from the server
		/// </summary>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<SyncRecord> Sync(params string[] FileSpecs)
		{
			return Sync(SyncOptions.None, -1, FileSpecs);
		}

		/// <summary>
		/// Syncs files from the server
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxFiles">Syncs only the first number of files specified.</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<SyncRecord> Sync(SyncOptions Options, int MaxFiles, params string[] FileSpecs)
		{
			return Sync(Options, -1, -1, -1, -1, -1, -1, FileSpecs);
		}

		/// <summary>
		/// Syncs files from the server
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxFiles">Syncs only the first number of files specified</param>
		/// <param name="NumThreads">Sync in parallel using the given number of threads</param>
		/// <param name="Batch">The number of files in a batch</param>
		/// <param name="BatchSize">The number of bytes in a batch</param>
		/// <param name="Min">Minimum number of files in a parallel sync</param>
		/// <param name="MinSize">Minimum number of bytes in a parallel sync</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<SyncRecord> Sync(SyncOptions Options, int MaxFiles, int NumThreads, int Batch, int BatchSize, int Min, int MinSize, params string[] FileSpecs)
		{
			string Arguments = GetSyncArguments(Options, MaxFiles, NumThreads, Batch, BatchSize, Min, MinSize, FileSpecs, false);
			return Command<SyncRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Syncs files from the server
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxFiles">Syncs only the first number of files specified.</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<SyncSummaryRecord> SyncQuiet(SyncOptions Options, int MaxFiles, params string[] FileSpecs)
		{
			return SyncQuiet(Options, -1, -1, -1, -1, -1, -1, FileSpecs);
		}

		/// <summary>
		/// Syncs files from the server without returning detailed file info
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxFiles">Syncs only the first number of files specified</param>
		/// <param name="NumThreads">Sync in parallel using the given number of threads</param>
		/// <param name="Batch">The number of files in a batch</param>
		/// <param name="BatchSize">The number of bytes in a batch</param>
		/// <param name="Min">Minimum number of files in a parallel sync</param>
		/// <param name="MinSize">Minimum number of bytes in a parallel sync</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<SyncSummaryRecord> SyncQuiet(SyncOptions Options, int MaxFiles, int NumThreads, int Batch, int BatchSize, int Min, int MinSize, params string[] FileSpecs)
		{
			string Arguments = GetSyncArguments(Options, MaxFiles, NumThreads, Batch, BatchSize, Min, MinSize, FileSpecs, true);
			return Command<SyncSummaryRecord>(Arguments.ToString(), null);
		}

		/// <summary>
		/// Gets arguments for a sync command
		/// </summary>
		/// <param name="Options">Options for the command</param>
		/// <param name="MaxFiles">Syncs only the first number of files specified</param>
		/// <param name="NumThreads">Sync in parallel using the given number of threads</param>
		/// <param name="Batch">The number of files in a batch</param>
		/// <param name="BatchSize">The number of bytes in a batch</param>
		/// <param name="Min">Minimum number of files in a parallel sync</param>
		/// <param name="MinSize">Minimum number of bytes in a parallel sync</param>
		/// <param name="FileSpecs">Files to sync</param>
		/// <param name="bQuiet">Whether to use quiet output</param>
		/// <returns>Arguments for the command</returns>
		static string GetSyncArguments(SyncOptions Options, int MaxFiles, int NumThreads, int Batch, int BatchSize, int Min, int MinSize, string[] FileSpecs, bool bQuiet)
		{
			StringBuilder Arguments = new StringBuilder("sync");
			if((Options & SyncOptions.Force) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & SyncOptions.KeepWorkspaceFiles) != 0)
			{
				Arguments.Append(" -k");
			}
			if((Options & SyncOptions.FullDepotSyntax) != 0)
			{
				Arguments.Append(" -L");
			}
			if((Options & SyncOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if((Options & SyncOptions.NetworkPreviewOnly) != 0)
			{
				Arguments.Append(" -N");
			}
			if((Options & SyncOptions.DoNotUpdateHaveList) != 0)
			{
				Arguments.Append(" -p");
			}
			if(bQuiet)
			{
				Arguments.Append(" -q");
			}
			if((Options & SyncOptions.ReopenMovedFiles) != 0)
			{
				Arguments.Append(" -r");
			}
			if((Options & SyncOptions.Safe) != 0)
			{
				Arguments.Append(" -s");
			}
			if(MaxFiles != -1)
			{
				Arguments.AppendFormat(" -m {0}", MaxFiles);
			}
			if(NumThreads != -1)
			{
				Arguments.AppendFormat(" --parallel-threads={0}", NumThreads);
				if(Batch != -1)
				{
					Arguments.AppendFormat(",batch={0}", Batch);
				}
				if(BatchSize != -1)
				{
					Arguments.AppendFormat(",batchsize={0}", BatchSize);
				}
				if(Min != -1)
				{
					Arguments.AppendFormat(",min={0}", Min);
				}
				if(MinSize != -1)
				{
					Arguments.AppendFormat(",minsize={0}", MinSize);
				}
			}
			foreach (string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}
			return Arguments.ToString();
		}

		#endregion

		#region p4 unshelve

		/// <summary>
		/// Restore shelved files from a pending change into a workspace
		/// </summary>
		/// <param name="ChangeNumber">The changelist containing shelved files</param>
		/// <param name="IntoChangeNumber">The changelist to receive the unshelved files</param>
		/// <param name="UsingBranchSpec">The branchspec to use when unshelving files</param>
		/// <param name="UsingStream">Specifies the use of a stream-derived branch view to map the shelved files between the specified stream and its parent stream.</param>
		/// <param name="ForceParentStream">Unshelve to the specified parent stream. Overrides the parent defined in the source stream specification.</param>
		/// <param name="Options">Options for the command</param>
		/// <param name="FileSpecs">Files to unshelve</param>
		/// <returns>Response from the server</returns>
		public PerforceResponseList<UnshelveRecord> Unshelve(int ChangeNumber, int IntoChangeNumber, string UsingBranchSpec, string UsingStream, string ForceParentStream, UnshelveOptions Options, params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("unshelve");
			Arguments.AppendFormat(" -s {0}", ChangeNumber);
			if((Options & UnshelveOptions.ForceOverwrite) != 0)
			{
				Arguments.Append(" -f");
			}
			if((Options & UnshelveOptions.PreviewOnly) != 0)
			{
				Arguments.Append(" -n");
			}
			if(IntoChangeNumber != -1)
			{
				Arguments.AppendFormat(" -c {0}", IntoChangeNumber);
			}
			if(UsingBranchSpec != null)
			{
				Arguments.AppendFormat(" -b \"{0}\"", UsingBranchSpec);
			}
			if(UsingStream != null)
			{
				Arguments.AppendFormat(" -S \"{0}\"", UsingStream);
			}
			if(ForceParentStream != null)
			{
				Arguments.AppendFormat(" -P \"{0}\"", ForceParentStream);
			}
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}

			return Command<UnshelveRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 user

		/// <summary>
		/// Enumerates all streams in a depot
		/// </summary>
		/// <param name="UserName">Name of the user to fetch information for</param>
		/// <returns>Response from the server</returns>
		public PerforceResponse<UserRecord> User(string UserName)
		{
			StringBuilder Arguments = new StringBuilder("user");
			Arguments.AppendFormat(" -o \"{0}\"", UserName);
			return SingleResponseCommand<UserRecord>(Arguments.ToString(), null);
		}

		#endregion

		#region p4 where

		/// <summary>
		/// Retrieves the location of a file of set of files in the workspace
		/// </summary>
		/// <param name="FileSpecs">Patterns for the files to query</param>
		/// <returns>List of responses from the server</returns>
		public PerforceResponseList<WhereRecord> Where(params string[] FileSpecs)
		{
			StringBuilder Arguments = new StringBuilder("where");
			foreach(string FileSpec in FileSpecs)
			{
				Arguments.AppendFormat(" \"{0}\"", FileSpec);
			}
			return Command<WhereRecord>(Arguments.ToString(), null);
		}

		#endregion
	}
}
