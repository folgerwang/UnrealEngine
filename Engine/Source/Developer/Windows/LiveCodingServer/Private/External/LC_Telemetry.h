// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <chrono>

namespace telemetry
{
	// scoped timing information
	class Scope
	{
	public:
		explicit Scope(const char* name);
		~Scope(void);

		double ReadSeconds(void) const;
		double ReadMilliSeconds(void) const;
		double ReadMicroSeconds(void) const;
		void Restart(void);
		void End(void);

	private:
		const char* m_name;
		std::chrono::high_resolution_clock::time_point m_start;
	};


	class Accumulator
	{
	public:
		explicit Accumulator(const char* name);

		void Accumulate(uint64_t value);
		void ResetCurrent(void);

		uint64_t ReadCurrent(void) const;
		uint64_t ReadAccumulated(void) const;
		
		void Print(void);

	private:
		const char* m_name;
		uint64_t m_current;
		uint64_t m_accumulated;
	};
}
