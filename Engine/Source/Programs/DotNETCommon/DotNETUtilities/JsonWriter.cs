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
	/// Writer for JSON data, which indents the output text appropriately, and adds commas and newlines between fields
	/// </summary>
	public class JsonWriter : IDisposable
	{
		TextWriter Writer;
		bool bLeaveOpen;
		bool bRequiresComma;
		string Indent;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="FileName">File to write to</param>
		public JsonWriter(string FileName)
			: this(new StreamWriter(FileName))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="FileName">File to write to</param>
		public JsonWriter(FileReference FileName)
			: this(new StreamWriter(FileName.FullName))
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Writer">The text writer to output to</param>
		/// <param name="bLeaveOpen">Whether to leave the writer open when the object is disposed</param>
		public JsonWriter(TextWriter Writer, bool bLeaveOpen = false)
		{
			this.Writer = Writer;
			this.bLeaveOpen = bLeaveOpen;
			Indent = "";
		}

		/// <summary>
		/// Dispose of any managed resources
		/// </summary>
		public void Dispose()
		{
			if(!bLeaveOpen && Writer != null)
			{
				Writer.Dispose();
				Writer = null;
			}
		}

		/// <summary>
		/// Write the opening brace for an object
		/// </summary>
		public void WriteObjectStart()
		{
			WriteCommaNewline();

			Writer.Write(Indent);
			Writer.Write("{");

			Indent += "\t";
			bRequiresComma = false;
		}

		/// <summary>
		/// Write the name and opening brace for an object
		/// </summary>
		/// <param name="ObjectName">Name of the field</param>
		public void WriteObjectStart(string ObjectName)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\": ", Indent, ObjectName);

			bRequiresComma = false;

			WriteObjectStart();
		}

		/// <summary>
		/// Write the closing brace for an object
		/// </summary>
		public void WriteObjectEnd()
		{
			Indent = Indent.Substring(0, Indent.Length - 1);

			Writer.WriteLine();
			Writer.Write(Indent);
			Writer.Write("}");

			bRequiresComma = true;
		}

		/// <summary>
		/// Write the name and opening bracket for an array
		/// </summary>
		/// <param name="ArrayName">Name of the field</param>
		public void WriteArrayStart(string ArrayName)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\": [", Indent, ArrayName);

			Indent += "\t";
			bRequiresComma = false;
		}

		/// <summary>
		/// Write the closing bracket for an array
		/// </summary>
		public void WriteArrayEnd()
		{
			Indent = Indent.Substring(0, Indent.Length - 1);

			Writer.WriteLine();
			Writer.Write("{0}]", Indent);

			bRequiresComma = true;
		}

		/// <summary>
		/// Write an array of strings
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Values">Values for the field</param>
		public void WriteStringArrayField(string Name, IEnumerable<string> Values)
		{
			WriteArrayStart(Name);
			foreach(string Value in Values)
			{
				WriteValue(Value);
			}
			WriteArrayEnd();
		}

		/// <summary>
		/// Write an array of enum values
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Values">Values for the field</param>
		public void WriteEnumArrayField<T>(string Name, IEnumerable<T> Values) where T : struct
		{
			WriteStringArrayField(Name, Values.Select(x => x.ToString()));
		}

		/// <summary>
		/// Write a value with no field name, for the contents of an array
		/// </summary>
		/// <param name="Value">Value to write</param>
		public void WriteValue(string Value)
		{
			WriteCommaNewline();

			Writer.Write(Indent);
			WriteEscapedString(Value);

			bRequiresComma = true;
		}

		/// <summary>
		/// Write a field name and string value
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Value">Value for the field</param>
		public void WriteValue(string Name, string Value)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\": ", Indent, Name);
			WriteEscapedString(Value);

			bRequiresComma = true;
		}

		/// <summary>
		/// Write a field name and integer value
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Value">Value for the field</param>
		public void WriteValue(string Name, int Value)
		{
			WriteValueInternal(Name, Value.ToString());
		}

		/// <summary>
		/// Write a field name and double value
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Value">Value for the field</param>
		public void WriteValue(string Name, double Value)
		{
			WriteValueInternal(Name, Value.ToString());
		}

		/// <summary>
		/// Write a field name and bool value
		/// </summary>
		/// <param name="Name">Name of the field</param>
		/// <param name="Value">Value for the field</param>
		public void WriteValue(string Name, bool Value)
		{
			WriteValueInternal(Name, Value ? "true" : "false");
		}

		void WriteCommaNewline()
		{
			if (bRequiresComma)
			{
				Writer.WriteLine(",");
			}
			else if (Indent.Length > 0)
			{
				Writer.WriteLine();
			}
		}

		void WriteValueInternal(string Name, string Value)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\": {2}", Indent, Name, Value);

			bRequiresComma = true;
		}

		void WriteEscapedString(string Value)
		{
			// Escape any characters which may not appear in a JSON string (see http://www.json.org).
			Writer.Write("\"");
			if (Value != null)
			{
				Writer.Write(Json.EscapeString(Value));
			}
			Writer.Write("\"");
		}
	}
}
