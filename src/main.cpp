#include <exception>
#include <iostream>
#include <cmath>
#include <list>
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

const double SAVE_FREQUENCY = 0.3;
const int MEM_SIZE_LIM_MUL = 10;
const double MAX_HEAP_OVERLOAD = 10.;

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

class BadWaveCollapse: public exception {
public:
    const char * what () const throw () {
        return "unsatisfied constraint encountered";
    }
};

class ImpossibleWaveCollapse: public exception {
public:
    const char * what () const throw () {
        return "impossible to fulfill the constraints. abort wave collapse.";
    }
};

class HistoryStack {
public:
  HistoryStack(int n, int m, int num_tiles) {
    int tot = max(n * m, n * m * num_tiles / 8);
    int ratio =  int(sqrt(tot)) / MEM_SIZE_LIM_MUL;
    MEM_SIZE_LIM = max(10, ratio);
  }

  void save_state(
    Index index,
    TileID tileId,
    TileMap<TileID> const& generated,
    TileMap<OneHotTiles> const& wave,
    TileStateLazyHeap const& heap) {
      m_index.push_back(index);
      m_tileId.push_back(tileId);
      m_generated.push_back(generated);
      m_wave.push_back(wave);
      m_heap.push_back(heap);
      if ((int)m_index.size() > MEM_SIZE_LIM) {
        remove_random_history();
      }
  }

  void remove_random_history() {
    uniform_int_distribution dist(1, int(m_index.size()) - 1);
    int pos = dist(random_gen);
    search_and_destroy(m_index, pos);
    search_and_destroy(m_tileId, pos);
    search_and_destroy(m_generated, pos);
    search_and_destroy(m_wave, pos);
    search_and_destroy(m_heap, pos);
  }

  Index restore_and_backtrack(
    TileMap<TileID>& generated,
    TileMap<OneHotTiles>& wave,
    TileStateLazyHeap& heap) {
    if (m_index.empty())
      throw ImpossibleWaveCollapse();
    Index index = m_index.back();
    TileID tileId = m_tileId.back();
    generated = m_generated.back();
    wave = m_wave.back();
    heap = m_heap.back();
    m_index.pop_back();
    m_tileId.pop_back();
    m_generated.pop_back();
    m_wave.pop_back();
    m_heap.pop_back();
    generated(index) = NO_TILE;
    wave(index).set(tileId, false);
    return index;
  }

private:
  template<typename T>
  void search_and_destroy(list<T>& hist, int pos) {
    auto it = begin(hist);
    advance(it, pos);
    hist.erase(it);
  }

  int MEM_SIZE_LIM;
  list<TileMap<TileID>> m_generated;
  list<TileMap<OneHotTiles>> m_wave;
  list<Index> m_index;
  list<TileID> m_tileId;
  list<TileStateLazyHeap> m_heap;
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
    TileMap<TileID> const& generated,
    TileMap<OneHotTiles>& wave,
    TileStateLazyHeap& heap,
    Index start
) {
    Index step = start_right;
    OneHotTiles mask(nb_max_tiles, false);
    OneHotTiles available = wave(start);
    if (available.none())
        throw BadWaveCollapse();
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

TileID sample_tile(Histogram const& histo, OneHotTiles const& onehot) {
    if (onehot.none())
        throw ImpossibleWaveCollapse();
    Histogram filtered = histo;
    for (int i = 0; i < (int)histo.size(); ++i) {
        if (!onehot[i]) {
            filtered[i] = 0.;
        }
    }
    discrete_distribution<int> dist(begin(filtered), end(filtered));
    return dist(random_gen);
}

TileMap<TileID> wave_function_collapse(WFCImage const& example, int n, int m) {
  ConstraintsHandler constraints(example.tileMap, example.nb_tiles());
  const auto histo = example.histogram;
  const auto all_tiles_ok = OneHotTiles{example.nb_tiles()}.set();
  TileMap<TileID> generated(n, m, NO_TILE);
  TileMap<OneHotTiles> wave(generated.n(), generated.m(), all_tiles_ok);

  TileStateLazyHeap heap;
  wave.for_each([&heap,&histo](int i, int j, OneHotTiles const& onehot){
      double h = entropy(histo, onehot);
      TileState state(Index(i, j), h);
      heap.update_key(state);
  });

  HistoryStack bt(n, m, example.nb_tiles());
  int depth = 0;
  uniform_real_distribution coin_tosser(0., 1.);
  while (!heap.empty()) {
    Index index = heap.top().index;
    heap.pop();
    auto const& onehot = wave(index);
    TileID tile = sample_tile(histo, onehot);

    double coin = coin_tosser(random_gen);
    if (coin < SAVE_FREQUENCY || depth == 0) {
      bt.save_state(index, tile, generated, wave, heap);
    }

    wave(index) = OneHotTiles{example.nb_tiles()}.set(tile);
    generated(index) = tile;
    depth += 1;

    Index last_index = index;
    int attempt = 1;
    while (true) {
      try {
        propagate(constraints, histo, example.nb_tiles(), generated, wave, heap, last_index);
        break ;
      } catch(BadWaveCollapse const& e) {
        cout << "Attempt " << attempt << ": " << e.what() << " at depth " << depth;
        last_index = bt.restore_and_backtrack(generated, wave, heap);
        attempt += 1;
        depth = 0;
        generated.for_each([&](int i, int j, TileID tile){
          if (tile != NO_TILE)
            depth += 1;
        });
        cout << " back to " << depth << "\n";
      }
    }

    if (heap.overload_ratio() > MAX_HEAP_OVERLOAD) {
      save_memory(heap);
    }
  }

  return generated;
}

// -----------------------------------------------------------------------------

void export_img(std::string const& path,
                TileMap<TileID>& generated,
                WFCImage& original)
{
    sf::Image output_image;
    uint ts = original.tile_size();
    uint n = generated.n();
    uint m = generated.m();
    output_image.create(n*ts, m*ts);
    for (uint i = 0 ; i < n; i++){
        for (uint j = 0 ; j < m ; j++){
            int ind = generated(i,j);
            output_image.copy(original.tiles[ind], i*ts, j*ts);
        }
    }
    output_image.saveToFile(path);
}

// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  try {
    if (argc>1) {
      std::string input_file = argv[1];
      int n = 30;
      int m = 30;
      int ts = 1;
      if (argc >= 5){
        n = atoi(argv[2]);
        m = atoi(argv[3]);
        ts = atoi(argv[4]);
      }
      WFCImage example;
      example.read_from_file(input_file, ts);
      auto generated = wave_function_collapse(example, n, m);
      export_img("output.png", generated, example);
      cout << "Success ! Image written at ./output.png\n";
    }
  } catch (exception const& e) {
    cout << "\nError: " << e.what() << "\n";
  }

  return 0;
}
