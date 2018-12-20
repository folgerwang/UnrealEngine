// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Wrapper around the HashSet container that only allows read operations
	/// </summary>
	/// <typeparam name="T">Type of element for the hashset</typeparam>
	public class ReadOnlyHashSet<T> : IReadOnlyCollection<T>
	{
		/// <summary>
		/// The mutable hashset
		/// </summary>
		HashSet<T> Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The mutable hashset</param>
		public ReadOnlyHashSet(HashSet<T> Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Collection">Elements for the hash set</param>
		public ReadOnlyHashSet(IEnumerable<T> Elements)
		{
			this.Inner = new HashSet<T>(Elements);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Collection">Elements for the hash set</param>
		/// <param name="Comparer">Comparer for elements in the set</param>
		public ReadOnlyHashSet(IEnumerable<T> Elements, IEqualityComparer<T> Comparer)
		{
			this.Inner = new HashSet<T>(Elements, Comparer);
		}

		/// <summary>
		/// Number of elements in the set
		/// </summary>
		public int Count
		{
			get { return Inner.Count; }
		}

		/// <summary>
		/// The comparer for elements in the set
		/// </summary>
		public IEqualityComparer<T> Comparer
		{
			get { return Inner.Comparer; }
		}

		/// <summary>
		/// Tests whether a given item is in the set
		/// </summary>
		/// <param name="Item">Item to check for</param>
		/// <returns>True if the item is in the set</returns>
		public bool Contains(T Item)
		{
			return Inner.Contains(Item);
		}

		/// <summary>
		/// Gets an enumerator for set elements
		/// </summary>
		/// <returns>Enumerator instance</returns>
		public IEnumerator<T> GetEnumerator()
		{
			return Inner.GetEnumerator();
		}

		/// <summary>
		/// Gets an enumerator for set elements
		/// </summary>
		/// <returns>Enumerator instance</returns>
		IEnumerator IEnumerable.GetEnumerator()
		{
			return ((IEnumerable)Inner).GetEnumerator();
		}
	}
}
