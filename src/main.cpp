#include <exception>
#include <iostream>
#include <cmath>
#include <random>

#include <vector>

#include "boost/dynamic_bitset.hpp"

#include "lazy_heap.hpp"
#include "tile_map.hpp"

using namespace std;

Index rot90(Index const& index) {
  return Index(index.second, - index.first);
}

Index compose(Index const& a, Index const& b) {
  return Index(a.first+b.first, a.second+b.second);
}

typedef boost::dynamic_bitset<> OneHotTiles;
typedef int TileType;
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
    TileMap<TileType> const& source,
    unsigned long nb_max_tiles):
  m_constraints(4, nb_max_tiles, OneHotTiles{nb_max_tiles}) {
    source.for_each([&](int i, int j, TileType tile){
      Index pos(i, j);
      Index step = start_right;
      for (int dir = 0; dir < 4; ++dir) {
        Index index = compose(pos, step);
        step = rot90(step);
        if (!source.inside(index))
          continue ;
        TileType neighbor_tile = source(index);
        m_constraints(dir, tile).set(neighbor_tile);
      }
    });
  }

  OneHotTiles compatible(int dir, TileType num_tile) const {
    return m_constraints(dir, num_tile);
  }

private:
  TileMap<OneHotTiles> m_constraints;
};


Histogram create_histogram(
  TileMap<TileType> const& source,
  unsigned long nb_max_tiles) {
  Histogram histo(nb_max_tiles, 0.);
  source.for_each([&histo](int i, int j, TileType const& tile) {
    histo[tile] += 1.;
  });
  int total_tiles = source.n() * source.m();
  for (int i = 0; i < (int)histo.size(); ++i) {
    histo[i] /= total_tiles;
  }
  return histo;
}

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
  TileMap<TileType> generated,
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

TileType sample_tile(Histogram const& histo, OneHotTiles const& onehot) {
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
  TileMap<TileType> const& source,
  TileMap<TileType>& generated,
  unsigned long nb_max_tiles
) {
  const vector<double> histo = create_histogram(source, nb_max_tiles);
  ConstraintsHandler constraints(source, nb_max_tiles);
  const auto all_tiles_ok = OneHotTiles{nb_max_tiles}.set();
  TileMap<OneHotTiles> wave(generated.n(), generated.m(), all_tiles_ok);

  TileStateLazyHeap heap;
  wave.for_each([&heap,&histo](int i, int j, OneHotTiles const& onehot){
    double h = entropy(histo, onehot);
    TileState state(Index(i, j), h);
    heap.update_key(state);
  });

  (wave.n(), wave.m(), NO_TILE);
  while (!heap.empty()) {
    Index index = heap.top().index;
    heap.pop();
    auto const& onehot = wave(index);
    TileType tile = sample_tile(histo, onehot);
    wave(index) = OneHotTiles{nb_max_tiles}.set(tile);
    generated(index) = tile;
    propagate(constraints, histo, nb_max_tiles, generated, wave, heap, index);
  }
}

TileMap<TileType>
wave_function_collapse(
  TileMap<TileType> const& source,
  int n, int m, unsigned long nb_max_tiles) {
  int attempt = 1;
  while (true) {
    cout << "Attempt " << attempt << "... ";
    try {
      TileMap<TileType> generated(n, m, NO_TILE);
      wave_function_collapse_attempt(source, generated, nb_max_tiles);
      cout << "success!\n";
      return generated;
    } catch (BadWaveCollapse const& e) {
      cout << e.what() << "\n";
      attempt += 1;
    }
  }
}

TileMap<TileType> read_ascii(vector<char>& to_print) {
  vector<int> to_read(256, -1);
  int n, m;
  cin >> n >> m;
  TileMap<TileType> source(n, m);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < m; ++j) {
      char c;
      cin >> c;
      int pos = static_cast<int>(c);
      if (to_read[pos] == -1) {
        to_read[pos] = to_print.size();
        to_print.push_back(c);
      }
      int tile = to_read[pos];
      source(i, j) = tile;
    }
  }
  return source;
}

void print_ascii(
  TileMap<TileType> const& generated,
  vector<char> const& to_print) {
  generated.for_each([&](int i, int j, TileType tile) {
    cout << to_print[tile];
    if (j+1 == generated.m()) {
      cout << "\n";
    }
  });
}

int main(int, char* []) {
  vector<char> to_print;
  TileMap<TileType> source = read_ascii(to_print);
  int n, m;
  cin >> n >> m;
  const unsigned long nb_max_tiles = to_print.size();
  auto generated = wave_function_collapse(source, n, m, nb_max_tiles);
  print_ascii(generated, to_print);
  return 0;
}
