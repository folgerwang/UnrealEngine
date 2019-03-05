// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_DiaVariant.h"
#include <utility>

dia::Variant::Variant(IDiaSymbol* symbol)
	: m_var{ VT_EMPTY }
	, m_str(nullptr)
{
	if (symbol->get_value(&m_var) == S_OK)
	{
		// the information we're interested in is always stored as string
		if (m_var.vt == VT_BSTR)
		{
			m_str = m_var.bstrVal;
		}
	}
}


dia::Variant::~Variant(void)
{
	if (m_str)
	{
		::VariantClear(&m_var);
	}
}


dia::Variant::Variant(Variant&& other)
	: m_var(std::move(other.m_var))
	, m_str(other.m_str)
{
	other.m_str = nullptr;
}
