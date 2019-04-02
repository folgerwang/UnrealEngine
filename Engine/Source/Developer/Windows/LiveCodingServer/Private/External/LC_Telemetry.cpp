// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Telemetry.h"
#include "LC_Logging.h"
#include <ratio>
#include <inttypes.h>

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

namespace
{
	template <typename T>
	static double ReadChrono(std::chrono::high_resolution_clock::time_point start)
	{
		const std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double, T> timeSpan = now - start;

		return timeSpan.count();
	}


	static void Print(const char* name, std::chrono::high_resolution_clock::time_point start)
	{
		const double seconds = ReadChrono<std::ratio<1, 1>>(start);
		LC_LOG_TELEMETRY("Scope \"%s\" took %.3fs (%.3fms)", name, seconds, seconds*1000.0);
	}
}


telemetry::Scope::Scope(const char* name)
	: m_name(name)
	, m_start(std::chrono::high_resolution_clock::now())
{
}


telemetry::Scope::~Scope(void)
{
	if (m_name)
	{
		Print(m_name, m_start);
	}
}


double telemetry::Scope::ReadSeconds(void) const
{
	return ::ReadChrono<std::ratio<1, 1>>(m_start);
}


double telemetry::Scope::ReadMilliSeconds(void) const
{
	return ::ReadChrono<std::milli>(m_start);
}


double telemetry::Scope::ReadMicroSeconds(void) const
{
	return ::ReadChrono<std::micro>(m_start);
}


void telemetry::Scope::Restart(void)
{
	m_start = std::chrono::high_resolution_clock::now();
}


void telemetry::Scope::End(void)
{
	Print(m_name, m_start);

	// do not print again when going out of scope
	m_name = nullptr;
}


telemetry::Accumulator::Accumulator(const char* name)
	: m_name(name)
	, m_current(0ull)
	, m_accumulated(0ull)
{
}


void telemetry::Accumulator::Accumulate(uint64_t value)
{
	::InterlockedAdd64(reinterpret_cast<volatile LONG64*>(&m_current), static_cast<LONG64>(value));
	::InterlockedAdd64(reinterpret_cast<volatile LONG64*>(&m_accumulated), static_cast<LONG64>(value));
}


void telemetry::Accumulator::ResetCurrent(void)
{
	m_current = 0ull;
}


uint64_t telemetry::Accumulator::ReadCurrent(void) const
{
	return m_current;
}


uint64_t telemetry::Accumulator::ReadAccumulated(void) const
{
	return m_accumulated;
}


void telemetry::Accumulator::Print(void)
{
	LC_LOG_TELEMETRY("Accumulator \"%s\"", m_name);

	LC_LOG_INDENT_TELEMETRY;
	LC_LOG_TELEMETRY("Current: %" PRId64 " (%.3f KB, %.3f MB)", m_current, m_current / 1024.0f, m_current / 1048576.0f);
	LC_LOG_TELEMETRY("Accumulated: %" PRId64 " (%.3f KB, %.3f MB)", m_accumulated, m_accumulated / 1024.0f, m_accumulated / 1048576.0f);
}

#include "Windows/HideWindowsPlatformAtomics.h"
