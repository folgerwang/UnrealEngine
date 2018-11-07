// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

/**
 * Multiple producer/multiple consumer thread safe queue
 */
template <typename T>
class TSharedQueue
{
public:
	TSharedQueue()
	{
	}

	template <typename... ARGS>
	void Emplace(ARGS&&... Args)
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		Queue.emplace(std::forward<ARGS>(Args)...);
		DataCondVar.notify_one();
	}

	template <typename T>
	void Push(T&& Item)
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		Queue.push(std::forward<T>(Item));
		DataCondVar.notify_one();
	}

	/**
	 * Tries to pop an item from the queue. It does not block waiting for items
	 * to be available.
	 *
	 * @param OutItem popped item on exit (if an item was retrieved)
	 * @return true if an item as retrieved, false otherwise
	 */
	bool TryPop(T& OutItem)
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		if (Queue.empty())
		{
			return false;
		}
		OutItem = std::move(Queue.front());
		Queue.pop();
		return true;
	}

	/**
	 * Retrieves all items into the supplied queue.
	 * This should be more efficient than retrieving one item at a time when a
	 * thread wants to process as many items as there are currently in the
	 * queue. Example:
	 * std::queue<Foo> all;
	 * if (q.TryPopAll(all)) {
	 *     ... process all items in the retrieved queue ...
	 * }
	 *
	 * @param OutQueue will contain the retrieved items on exit. Any pre-existing
	 *	items will be lost.
	 * @return true if any items were retrieved
	 */
	bool TryPopAll(std::queue<T>& OutQueue)
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		OutQueue = std::move(Queue);
		return OutQueue.size() != 0;
	}

	/**
	 * Pops an item, blocking if necessary to wait for one if the queue is currently
	 * empty.
	 * @param OutItem popped item on exit
	 */
	void Pop(T& OutItem)
	{
		std::unique_lock<std::mutex> Lk(Mtx);
		DataCondVar.wait(Lk, [this] { return !Queue.empty(); });
		OutItem = std::move(Queue.front());
		Queue.pop();
	}

	/**
	 * Retrieves an item, blocking if necessary for the specified duration
	 * until items are available arrive.
	 *
	 * @param OutItem popped item on exit (if an item was retrieved)
	 * @param TimeoutMs How long to wait for an item to be available
	 * @return true if an item as retrieved, false if it timed out before an item
	 *	was available
	 */
	bool Pop(T& OutItem, int64_t TimeoutMs)
	{
		std::unique_lock<std::mutex> Lk(Mtx);
		if (!DataCondVar.wait_for(Lk, std::chrono::milliseconds(TimeoutMs), [this] { return !Queue.empty(); }))
		{
			return false;
		}

		OutItem = std::move(Queue.front());
		Queue.pop();
		return true;
	}

	/**
	 * Checks if the queue is empty
	 */
	bool IsEmpty() const
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		return Queue.empty();
	}

	/**
	 * Tells how many items are available in the queue
	 */
	size_t Size() const
	{
		std::lock_guard<std::mutex> Lk(Mtx);
		return Queue.size();
	}

private:
	std::queue<T> Queue;
	mutable std::mutex Mtx;
	std::condition_variable DataCondVar;

	TSharedQueue& operator=(const TSharedQueue&) = delete;
	TSharedQueue(const TSharedQueue& other) = delete;
};

using FWorkQueue = TSharedQueue<std::function<void()>>;
