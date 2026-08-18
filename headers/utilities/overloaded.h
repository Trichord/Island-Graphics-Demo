#pragma once

// Helper for the visitor pattern
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
