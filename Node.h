#ifndef NODE_H
#define NODE_H

#include "DataStructs.h"

#include <concepts>

template <typename T>
concept Node = requires(T t, Frame f) {
    { t.update(f) };
};

#endif