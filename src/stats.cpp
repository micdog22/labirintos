#include "stats.hpp"

#include <queue>
#include <vector>

namespace lab {

namespace {

// Distâncias (em passos) a partir de uma célula; -1 onde não alcança.
std::vector<int> distances(const Maze& m, int from) {
    std::vector<int> dist(static_cast<std::size_t>(m.cells()), -1);
    std::queue<int> q;
    dist[static_cast<std::size_t>(from)] = 0;
    q.push(from);
    while (!q.empty()) {
        const int c = q.front();
        q.pop();
        for (int nb : m.open_neighbors(c)) {
            if (dist[static_cast<std::size_t>(nb)] < 0) {
                dist[static_cast<std::size_t>(nb)] = dist[static_cast<std::size_t>(c)] + 1;
                q.push(nb);
            }
        }
    }
    return dist;
}

int farthest(const std::vector<int>& dist) {
    int best = 0;
    for (std::size_t i = 0; i < dist.size(); ++i) {
        if (dist[i] > dist[static_cast<std::size_t>(best)]) best = static_cast<int>(i);
    }
    return best;
}

}  // namespace

Stats compute_stats(const Maze& m) {
    Stats s;
    s.cells = m.cells();
    s.passages = m.passages();
    s.cycles = static_cast<long long>(s.passages) - (static_cast<long long>(s.cells) - 1);
    for (int c = 0; c < m.cells(); ++c) {
        const int deg = m.degree(c);
        if (deg == 1) ++s.dead_ends;
        if (deg == 2) ++s.corridors;
        if (deg >= 3) ++s.junctions;
    }
    const std::vector<int> from_start = distances(m, m.start());
    s.connected = true;
    for (int d : from_start) s.connected = s.connected && d >= 0;
    s.perfect = s.connected && s.cycles == 0;
    s.longest_from = farthest(from_start);
    const std::vector<int> from_a = distances(m, s.longest_from);
    s.longest_to = farthest(from_a);
    s.longest = from_a[static_cast<std::size_t>(s.longest_to)];
    return s;
}

}  // namespace lab
