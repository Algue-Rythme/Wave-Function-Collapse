#pragma once

#include <utility>

typedef std::pair<int, int> Index;

Index rot90(Index const& index) {
    return Index(index.second, - index.first);
}

Index compose(Index const& a, Index const& b) {
    return Index(a.first+b.first, a.second+b.second);
}

Index operator+(Index const& a, Index const&b) {
    return Index(a.first+b.first, a.second+b.second);
}
