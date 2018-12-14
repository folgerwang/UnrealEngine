// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Options for serializing an object to JSON
	/// </summary>
	public enum JsonSerializeOptions
	{
		/// <summary>
		/// No options
		/// </summary>
		None,

		/// <summary>
		/// Whether to format the output for readability
		/// </summary>
		PrettyPrint = 1,
	}

	/// <summary>
	/// Wrapper class around fastJSON methods.
	/// </summary>
	public static class Json
	{
		/// <summary>
		/// Static constructor. Registers known types to the serializer.
		/// </summary>
		static Json()
		{
			fastJSON.JSON.Instance.RegisterCustomType(typeof(FileReference), x => EscapeString(x.ToString()), x => new FileReference(x));
			fastJSON.JSON.Instance.RegisterCustomType(typeof(DirectoryReference), x => EscapeString(x.ToString()), x => new DirectoryReference(x));
		}

		/// <summary>
		/// Serializes an object to JSON
		/// </summary>
		/// <param name="Object">Object to be serialized</param>
		/// <returns>JSON representation of the object</returns>
		public static string Serialize(object Object)
		{
			return Serialize(Object, JsonSerializeOptions.PrettyPrint);
		}

		/// <summary>
		/// Serializes an object to JSON
		/// </summary>
		/// <param name="Object">Object to be serialized</param>
		/// <param name="Options">Options for formatting the object</param>
		/// <returns>JSON representation of the object</returns>
		public static string Serialize(object Object, JsonSerializeOptions Options)
		{
			fastJSON.JSONParameters Params = new fastJSON.JSONParameters();
			Params.EnableAnonymousTypes = true;

			string Text = fastJSON.JSON.Instance.ToJSON(Object, Params);
			if((Options & JsonSerializeOptions.PrettyPrint) != 0)
			{
				Text = Format(Text);
			}
			return Text;
		}

		/// <summary>
		/// Deserialize an object from text
		/// </summary>
		/// <typeparam name="T">Type of the object to deserialize</typeparam>
		/// <param name="Text">String to parse</param>
		/// <returns>Constructed object</returns>
		public static T Deserialize<T>(string Text)
		{
			return fastJSON.JSON.Instance.ToObject<T>(Text);
		}

		/// <summary>
		/// Read a JSON object from a file on disk
		/// </summary>
		/// <typeparam name="T">The type of the object to load</typeparam>
		/// <param name="Location">The location of the file to read</param>
		/// <returns>The deserialized object</returns>
		public static T Load<T>(FileReference Location)
		{
			string Text;
			try
			{
				Text = FileReference.ReadAllText(Location);
			}
			catch(Exception Ex)
			{
				throw new Exception(String.Format("Unable to read '{0}'", Location), Ex);
			}
			return Deserialize<T>(Text);
		}

		/// <summary>
		/// Save a JSON object to a file on disk
		/// </summary>
		/// <param name="Location">The location of the file to read</param>
		/// <param name="Object">The object to save</param>
		public static void Save(FileReference Location, object Object)
		{
			string Text = Serialize(Object);
			try
			{
				DirectoryReference.CreateDirectory(Location.Directory);
				FileReference.WriteAllText(Location, Text);
			}
			catch(Exception Ex)
			{
				throw new Exception(String.Format("Unable to write '{0}'", Location), Ex);
			}
		}

		/// <summary>
		/// Formats the given JSON string in 'Epic' format (parentheses/brackets on new lines, tabs instead of spaces, no space before colons in key/value pairs)
		/// </summary>
		/// <param name="Input">The input string</param>
		/// <returns>The formatted string</returns>
		public static string Format(string Input)
		{
			StringBuilder Result = new StringBuilder(Input.Length * 2);

			bool bOnEmptyLine = true;

			int Depth = 0;
			for(int Idx = 0; Idx < Input.Length; Idx++)
			{
				switch(Input[Idx])
				{
					case '{':
					case '[':
						if(!bOnEmptyLine)
						{
							Result.AppendLine();
							bOnEmptyLine = true;
						}

						AppendIndent(Result, Depth++);
						Result.Append(Input[Idx]);
						Result.AppendLine();
						break;
					case '}':
					case ']':
						if(!bOnEmptyLine)
						{
							Result.AppendLine();
						}

						AppendIndent(Result, --Depth);
						Result.Append(Input[Idx]);
						bOnEmptyLine = false;
						break;
					case ':':
						Result.Append(Input[Idx]);
						Result.Append(' ');
						break;
					case ',':
						Result.Append(Input[Idx]);
						Result.AppendLine();
						bOnEmptyLine = true;
						break;
					default:
						if(!Char.IsWhiteSpace(Input[Idx]))
						{
							// If this is an empty line, append the indent
							if(bOnEmptyLine)
							{
								AppendIndent(Result, Depth);
								bOnEmptyLine = false;
							}

							// Append the character
							Result.Append(Input[Idx]);

							// If this was a string, also append everything to the end of the string
							if(Input[Idx] == '\"')
							{
								Idx++;
								for(; Idx < Input.Length; Idx++)
								{
									Result.Append(Input[Idx]);
									if(Input[Idx] == '\\' && Idx + 1 < Input.Length)
									{
										Result.Append(Input[++Idx]);
									}
									else if(Input[Idx] == '\"')
									{
										break;
									}
								}
							}
						}
						break;
				}
			}

			return Result.ToString();
		}

		/// <summary>
		/// Appends an indent to the output string
		/// </summary>
		/// <param name="Builder">The string builder to append to</param>
		/// <param name="Depth">The number of tabs to append</param>
		private static void AppendIndent(StringBuilder Builder, int Depth)
		{
			for(int Idx = 0; Idx < Depth; Idx++)
			{
				Builder.Append('\t');
			}
		}

		/// <summary>
		/// Escapes a string for serializing to JSON
		/// </summary>
		/// <param name="Value">The string to escape</param>
		/// <returns>The escaped string</returns>
		public static string EscapeString(string Value)
		{
			StringBuilder Result = new StringBuilder();
			for (int Idx = 0; Idx < Value.Length; Idx++)
			{
				switch (Value[Idx])
				{
					case '\"':
						Result.Append("\\\"");
						break;
					case '\\':
						Result.Append("\\\\");
						break;
					case '\b':
						Result.Append("\\b");
						break;
					case '\f':
						Result.Append("\\f");
						break;
					case '\n':
						Result.Append("\\n");
						break;
					case '\r':
						Result.Append("\\r");
						break;
					case '\t':
						Result.Append("\\t");
						break;
					default:
						if (Char.IsControl(Value[Idx]))
						{
							Result.AppendFormat("\\u{0:X4}", (int)Value[Idx]);
						}
						else
						{
							Result.Append(Value[Idx]);
						}
						break;
				}
			}
			return Result.ToString();
		}
	}
}
