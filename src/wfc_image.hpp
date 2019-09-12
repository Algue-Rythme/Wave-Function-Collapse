#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream> // DEBUG

#include <SFML/Graphics.hpp>

#include "tile_map.hpp"

typedef uint TileID;
typedef sf::Image Tile;

bool equal(Tile const& a, Tile const& b){
    sf::Vector2u size_a = a.getSize();
    sf::Vector2u size_b = b.getSize();
    if (size_a.x != size_b.x || size_a.y != size_b.y) return false;
    for (uint i = 0 ; i < size_a.x ; i++) {
        for (uint j = 0 ; j < size_a.y ; j++) {
            if (a.getPixel(i,j) != b.getPixel(i,j)) return false;
        }
    }
    return true;
}

class WFCImage {

public:
    WFCImage() = default;

    void read_from_file(std::string const& path, uint ts = 1u) {
        if (_loaded) return;

        if (not img.loadFromFile(path)){
            std::cerr << "Image " << path << " could not be loaded" << std::endl;
        }

        _tile_size = ts;
        sf::Vector2u size = img.getSize();
        size_x = size.x/ts;
        size_y = size.y/ts;
        tileMap = TileMap<TileID>(size_x, size_y);

        for (uint x = 0 ; x < size_x ; x++) {
            for (uint y = 0 ; y < size_y ; y++) {
                Tile tile_xy;
                tile_xy.create(ts, ts);
                tile_xy.copy(img, 0, 0,
                    sf::IntRect(x*ts, y*ts,
                                ts, ts));
                for (uint find = 0 ; find <= tiles.size() ; find++) {
                    if (find == tiles.size()) {
                        tiles.push_back(tile_xy);
                        tileMap(x, y) = TILE_ID;
                        TILE_ID++;
                        break;
                    }
                    if (equal(tiles[find], tile_xy)) {
                        tileMap(x, y) = find;
                        break;
                    }
                }
            }
        }
        create_histogram();
        _loaded = true;
    }

    uint nb_tiles() const {
        return tiles.size();
    }

    uint tile_size() const {
        return _tile_size;
    }

    void create_histogram() {
        histogram = std::vector<double>(tiles.size(),0.);
        tileMap.for_each([&](int i, int j, TileID const& tile) {
            histogram[tile] += 1.;
        });
        int total_tiles = tileMap.n() * tileMap.m();
        for (auto &x : histogram) {
            x /= total_tiles;
        }
    }

    const Tile operator[](TileID i) const {
        return tiles[i];
    }

    Tile& operator[](TileID i) {
        return tiles[i];
    }

    std::vector<double> histogram; // id -> proportion of given tile
    std::vector<Tile> tiles; // id -> data of the tile
    TileMap<TileID> tileMap; // neighbourhood data

private:
    sf::Image img;
    bool _loaded = false;
    uint size_x;
    uint size_y;
    uint _tile_size;

    unsigned long TILE_ID = 0u;
};
