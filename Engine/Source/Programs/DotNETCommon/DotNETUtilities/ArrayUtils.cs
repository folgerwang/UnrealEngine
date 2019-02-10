// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	static class ArrayUtils
	{
		/// <summary>
		/// Compares two byte arrays for equality
		/// </summary>
		/// <param name="A">The first byte array</param>
		/// <param name="B">The second byte array</param>
		/// <returns>True if the two arrays are equal, false otherwise</returns>
		public static bool ByteArraysEqual(byte[] A, byte[] B)
		{
			if(A.Length != B.Length)
			{
				return false;
			}

			for(int Idx = 0; Idx < A.Length; Idx++)
			{
				if(A[Idx] != B[Idx])
				{
					return false;
				}
			}

			return true;
		}
	}
}
