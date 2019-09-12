#ifndef TILE_MAP
#define TILE_MAP

#include <utility>
#include <vector>

#include "index.hpp"

template<typename T>
class TileMap {
public:
    TileMap() = default;
    
    TileMap(int _n, int _m, T const& default_tile = T()):
        m_n(_n), m_m(_m), m_tiles(m_n * m_m, default_tile) {
        }

    TileMap(TileMap const&) = default;
    TileMap& operator=(TileMap const&) = default;

    int n() const {
        return m_n;
    }

    int m() const {
        return m_m;
    }

    const T operator()(Index const& index) const {
        return this->operator()(index.first, index.second);
    }

    T& operator()(Index const& index) {
        return this->operator()(index.first, index.second);
    }

    const T operator()(int i, int j) const {
        return m_tiles.at(i*m() + j);
    }

    T& operator()(int i, int j) {
        return m_tiles.at(i*m() + j);
    }

    template<typename F>
    void for_each(F f) const {
        for (int i = 0; i < n(); ++i) {
            for (int j = 0; j < m(); ++j) {
                f(i, j, this->operator()(i, j));
            }
        }
    }

    bool inside(Index const& index) const {
        return inside(index.first, index.second);
    }

    bool inside(int i, int j) const {
        return i >= 0 && i < n() && j >= 0 && j < m();
    }

private:
    int m_n;
    int m_m;
    std::vector<T> m_tiles;
};

#endif
