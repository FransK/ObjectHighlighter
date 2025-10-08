#ifndef NODE_H
#define NODE_H

#include "DataStructs.h"

#include <concepts>
#include <optional>

template <typename T>
concept Node = requires(T t, Frame &f, const Frame &cf, std::stop_token st) {
    { t.getFrame(st) } -> std::same_as<std::optional<Frame>>;
    t.updateFrame(f);
    t.passFrame(cf, st);
};

#endif