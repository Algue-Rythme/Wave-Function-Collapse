#include <exception>
#include <iostream>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <cstdlib>

#include "boost/dynamic_bitset.hpp"

#include "lazy_heap.hpp"
#include "index.hpp"
#include "wfc_image.hpp"

using namespace std;

typedef boost::dynamic_bitset<> OneHotTiles;
typedef int TileID;
typedef vector<double> Histogram;

struct TileState {
    TileState(Index const _index, double _entropy):
    index(_index), entropy(_entropy) {}

    Index index;
    double entropy;
};

struct CompareTileStateHeap {
    bool operator()(TileState const& a, TileState const& b) const {
        return a.entropy < b.entropy;
    }
};

struct CompareTileStateHash {
    bool operator()(TileState const& a, TileState const& b) const {
        return a.index < b.index;
    }
};

typedef LazyHeap<
    TileState,
    CompareTileStateHeap,
    CompareTileStateHash
> TileStateLazyHeap;

random_device random_seed;
default_random_engine random_gen(random_seed());
const int NO_TILE = -1;
const Index start_right(0, 1);

class ConstraintsHandler {
public:
    ConstraintsHandler(
        TileMap<TileID> const& source,
        unsigned long nb_max_tiles):
    m_constraints(4, nb_max_tiles, OneHotTiles{nb_max_tiles}) {
        source.for_each([&](int i, int j, TileID tile){
            Index pos(i, j);
            Index step = start_right;
            for (int dir = 0; dir < 4; ++dir) {
                Index index = compose(pos, step);
                step = rot90(step);
                if (!source.inside(index))
                    continue ;
                TileID neighbor_tile = source(index);
                m_constraints(dir, tile).set(neighbor_tile);
            }
        });
    }

    OneHotTiles compatible(int dir, TileID num_tile) const {
        return m_constraints(dir, num_tile);
    }

private:
    TileMap<OneHotTiles> m_constraints;
};

double entropy(Histogram const& histo, OneHotTiles const& onehot) {
    assert(histo.size() == tile.size());
    double h = 0.;
    for (int i = 0; i < (int)histo.size(); ++i) {
        if (onehot[i]) {
            double p = histo[i];
            double x = (p * log(p));
            h -= x;
        }
    }
    return h;
}

void propagate(
    ConstraintsHandler const& constraints,
    Histogram const& histo,
    unsigned long nb_max_tiles,
    TileMap<TileID> generated,
    TileMap<OneHotTiles>& wave,
    TileStateLazyHeap& heap,
    Index start
) {
    Index step = start_right;
    OneHotTiles mask(nb_max_tiles, false);
    OneHotTiles available = wave(start);
    for (int dir = 0; dir < 4; ++dir) {
        auto index = compose(start, step);
        step = rot90(step);
        if (!wave.inside(index) || generated(index) != NO_TILE)
            continue ;
        OneHotTiles mask(nb_max_tiles);
        for (int tile = 0; tile < (int)nb_max_tiles; ++tile) {
            if (available[tile]) {
                mask = mask | constraints.compatible(dir, tile);
            }
        }
        auto old_constraint = wave(index);
        wave(index) = wave(index) & mask;
        double h = entropy(histo, wave(index));
        heap.update_key(TileState(index, h));
        if (old_constraint != wave(index)) {
            propagate(constraints, histo, nb_max_tiles, generated, wave, heap, index);
        }
    }
}

class BadWaveCollapse: public exception {
public:
    const char * what () const throw () {
        return "unsatisfied constraint encountered";
    }
};

TileID sample_tile(Histogram const& histo, OneHotTiles const& onehot) {
    if (onehot.none())
        throw BadWaveCollapse();
    Histogram filtered = histo;
    for (int i = 0; i < (int)histo.size(); ++i) {
        if (!onehot[i]) {
            filtered[i] = 0.;
        }
    }
    discrete_distribution<int> dist(begin(filtered), end(filtered));
    return dist(random_gen);
}

void wave_function_collapse_attempt(
    WFCImage const& example,
    TileMap<TileID>& generated
) {
    ConstraintsHandler constraints(example.tileMap, example.nb_tiles());
    const auto histo = example.histogram;
    const auto all_tiles_ok = OneHotTiles{example.nb_tiles()}.set();
    TileMap<OneHotTiles> wave(generated.n(), generated.m(), all_tiles_ok);

    TileStateLazyHeap heap;
    wave.for_each([&heap,&histo](int i, int j, OneHotTiles const& onehot){
        double h = entropy(histo, onehot);
        TileState state(Index(i, j), h);
        heap.update_key(state);
    });

    // (wave.n(), wave.m(), NO_TILE);
    while (!heap.empty()) {
        Index index = heap.top().index;
        heap.pop();
        auto const& onehot = wave(index);
        TileID tile = sample_tile(histo, onehot);
        wave(index) = OneHotTiles{example.nb_tiles()}.set(tile);
        generated(index) = tile;
        propagate(constraints, histo, example.nb_tiles(), generated, wave, heap, index);
    }
}

TileMap<TileID>
wave_function_collapse(
    WFCImage example,
    int n, int m) {

    int attempt = 1;
    while (true) {
        cout << "Attempt " << attempt << "... ";
        try {
            TileMap<TileID> generated(n, m, NO_TILE);
            wave_function_collapse_attempt(example, generated);
            cout << "success!\n";
            return generated;
        } catch (BadWaveCollapse const& e) {
            cout << e.what() << "\n";
            attempt += 1;
        }

        if (attempt>10) break;
    }
}

// -----------------------------------------------------------------------------

void print_ascii(
    TileMap<TileID> const& generated,
    WFCImage example) {
    generated.for_each([&](int i, int j, TileID tile) {
        cout << example.tiles[tile];
        if (j+1 == generated.m()) {
            cout << "\n";
        }
    });
}

// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {

    if (argc>1){
        std::string input_file = argv[1];

        int n,m;
        if (argc>3){
            n = atoi(argv[2]);
            m = atoi(argv[3]);
        } else {
            n = 30;
            m = 30;
        }

        WFCImage example;
        example.read_from_txt(input_file);
        auto generated = wave_function_collapse(example, n, m);
        print_ascii(generated, example);
    }


    return 0;
}
