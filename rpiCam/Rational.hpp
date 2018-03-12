#pragma once

#include "Config.hpp"

class Rational
{
public:
    Rational();
    Rational(Rational const &that);
    Rational(std::int32_t num, std::int32_t den);

    inline Rational& operator =(Rational const &rhs) { numerator = rhs.numerator; denominator = rhs.denominator; return *this; }

    inline operator float() const { return static_cast<float>(numerator)/static_cast<float>(denominator); }

    inline bool operator <(Rational const &rhs) const { return static_cast<float>(*this) < static_cast<float>(rhs); }
    inline bool operator >(Rational const &rhs) const { return static_cast<float>(*this) > static_cast<float>(rhs); }
    inline bool operator ==(Rational const &rhs) const { return numerator == rhs.numerator && denominator == rhs.denominator; }
    inline bool operator <=(Rational const &rhs) const { return *this < rhs || *this == rhs; }
    inline bool operator >=(Rational const &rhs) const { *this > rhs || *this == rhs; }
    inline bool operator !=(Rational const &rhs) const { !(*this == rhs); }

    std::int32_t numerator;
    std::int32_t denominator;
};
