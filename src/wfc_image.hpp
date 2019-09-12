#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>

#include <SFML/Graphics.hpp>

#include "tile_map.hpp"

typedef int TileID;
typedef char Tile;

class WFCImage {

public:
    WFCImage() = default;

    void read_from_txt(std::string const& path) {
        if (_loaded) return;

        std::ifstream stream;
        stream.open(path);

        int n,m;
        stream >> n >> m;
        tileMap = TileMap<TileID>(n, m);

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                char c;
                stream >> c;
                for (uint find = 0 ; find <= tiles.size() ; find++){
                    if (find == tiles.size()){
                        tiles.push_back(c);
                        tileMap(i, j) = TILE_ID;
                        TILE_ID++;
                        break;
                    } else if (tiles[find] == c){
                        tileMap(i, j) = find;
                        break;
                    }
                }
            }
        }
        create_histogram();
        _loaded = true;
    }

    /*
    void read_from_img(std::string const& path, uint _tile_size = 1u) {
        if (_loaded) return;
        sf::Image img;
        if (not img.loadFromFile(path)){
            std::cerr << "Image " << path << " could not be loaded" << std::endl;
        }
        create_histogram();
        _loaded = true;
    }
    */

    uint nb_tiles() const {
        return tiles.size();
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
    bool _loaded = false;
    uint size_x;
    uint size_y;

    //uint tile_size;

    unsigned long TILE_ID = 0u;
};
