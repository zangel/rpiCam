#pragma once

#include "Config.hpp"

class Buffer
{
public:
    Buffer();
    virtual ~Buffer();

    virtual bool isValid() const = 0;
    virtual void* data() = 0;
    virtual std::size_t size() const = 0;
};

