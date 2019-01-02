// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace Chaos
{
template<class T>
class Complex
{
  public:
	Complex() {}
	Complex(const T Real, const T Imaginary)
	    : MReal(Real), MImaginary(Imaginary) {}
	Complex Conjugated() { return Complex(MReal, -MImaginary); }
	Complex operator*(const T Other) const { return Complex(MReal * Other, MImaginary * Other); }
	Complex operator+(const Complex<T> Other) const { return Complex(MReal + Other.MReal, MImaginary + Other.MImaginary); }
	Complex& operator-=(const Complex<T> Other)
	{
		MReal -= Other.MReal;
		MImaginary -= Other.MImaginary;
		return *this;
	}
	inline void MakeReal() { MImaginary = 0; }
	inline const T Real() const { return MReal; }
	inline const T Imaginary() const { return MImaginary; }

  private:
	T MReal;
	T MImaginary;
};
template<class T>
Complex<T> operator*(const float Other, const Complex<T> Complex)
{
	return Complex * Other;
}
}
