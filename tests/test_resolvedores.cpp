// Resolvedores: caminhos válidos, menor caminho igual em BFS, A* e Dijkstra, e
// custo mínimo com pesos conferido contra Bellman-Ford.
#include <limits>

#include "apoio.hpp"
#include "solvers.hpp"
#include "teste.h"

using lab::Solver;

namespace {

const Solver kTodos[] = {Solver::Bfs, Solver::Dfs, Solver::AStar, Solver::Dijkstra, Solver::DeadEnds, Solver::RightHand};

// Referência independente para o menor custo: Bellman-Ford, O(V * E).
long long bellman_ford(const lab::Maze& m, const std::vector<int>& w) {
    const long long inf = std::numeric_limits<long long>::max() / 4;
    std::vector<long long> d(static_cast<std::size_t>(m.cells()), inf);
    d[static_cast<std::size_t>(m.start())] = 0;
    for (int it = 0; it < m.cells(); ++it) {
        bool changed = false;
        for (int c = 0; c < m.cells(); ++c) {
            if (d[static_cast<std::size_t>(c)] == inf) continue;
            for (int nb : m.open_neighbors(c)) {
                const long long nd = d[static_cast<std::size_t>(c)] + w[static_cast<std::size_t>(nb)];
                if (nd < d[static_cast<std::size_t>(nb)]) {
                    d[static_cast<std::size_t>(nb)] = nd;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }
    return d[static_cast<std::size_t>(m.goal())];
}

}  // namespace

TESTE(labirintos_perfeitos_tem_um_unico_caminho) {
    for (const auto& info : lab::generator_list()) {
        for (const auto& [w, h] : tamanhos()) {
            for (std::uint64_t seed = 1; seed <= 3; ++seed) {
                const lab::Maze m = gerar(info.id, w, h, seed);
                const lab::Solution ref = lab::solve(m, Solver::Bfs);
                CHECK(lab::is_valid_path(m, ref.path));
                for (Solver s : kTodos) {
                    const lab::Solution sol = lab::solve(m, s);
                    CHECK(lab::is_valid_path(m, sol.path));
                    CHECK(sol.path == ref.path);  // num labirinto perfeito o caminho é único
                    CHECK(sol.explored_count >= sol.path.size());
                    CHECK(sol.explored_count <= static_cast<std::size_t>(m.cells()));
                }
            }
        }
    }
}

TESTE(com_ciclos_bfs_astar_e_dijkstra_concordam) {
    for (const auto& info : lab::generator_list()) {
        for (std::uint64_t seed = 1; seed <= 4; ++seed) {
            const lab::Maze m = gerar(info.id, 25, 18, seed, 0.6);
            const lab::Solution bfs = lab::solve(m, Solver::Bfs);
            const lab::Solution astar = lab::solve(m, Solver::AStar);
            const lab::Solution dij = lab::solve(m, Solver::Dijkstra);
            const lab::Solution becos = lab::solve(m, Solver::DeadEnds);
            CHECK(lab::is_valid_path(m, bfs.path));
            CHECK_EQ(astar.path.size(), bfs.path.size());
            CHECK_EQ(dij.path.size(), bfs.path.size());
            // Tapar becos nunca remove um caminho entre a entrada e a saída.
            CHECK_EQ(becos.path.size(), bfs.path.size());
            CHECK(lab::is_valid_path(m, astar.path));
            CHECK(lab::is_valid_path(m, dij.path));
            CHECK(lab::is_valid_path(m, becos.path));
            // Com heurística consistente, o A* nunca examina mais células que o Dijkstra.
            CHECK(astar.explored_count <= dij.explored_count);
            for (Solver s : {Solver::Dfs, Solver::RightHand}) {
                const lab::Solution sol = lab::solve(m, s);
                CHECK(lab::is_valid_path(m, sol.path));
                CHECK(sol.path.size() >= bfs.path.size());
            }
        }
    }
}

TESTE(dijkstra_e_astar_com_pesos) {
    for (const auto& info : lab::generator_list()) {
        for (std::uint64_t seed = 1; seed <= 3; ++seed) {
            lab::Maze m(20, 15);
            lab::Rng rng(seed);
            lab::generate(m, info.id, rng);
            lab::braid(m, 0.7, rng);
            const std::vector<int> w = lab::random_weights(m, rng);
            const lab::Solution dij = lab::solve(m, Solver::Dijkstra, &w);
            const lab::Solution astar = lab::solve(m, Solver::AStar, &w);
            const long long esperado = bellman_ford(m, w);
            CHECK_EQ(dij.cost, esperado);
            CHECK_EQ(astar.cost, esperado);
            CHECK_EQ(lab::path_cost(dij.path, &w), esperado);
            CHECK(lab::is_valid_path(m, dij.path));
            CHECK(lab::is_valid_path(m, astar.path));
            // O caminho mais barato nunca custa mais que o caminho mais curto.
            const lab::Solution bfs = lab::solve(m, Solver::Bfs, &w);
            CHECK(dij.cost <= bfs.cost);
        }
    }
}

TESTE(mao_direita_anda_mais_que_o_caminho) {
    const lab::Maze m = gerar(lab::Generator::Prim, 30, 30, 8);
    const lab::Solution s = lab::solve(m, Solver::RightHand);
    CHECK(lab::is_valid_path(m, s.path));
    CHECK(s.walked >= s.path.size() - 1);
    // Num labirinto perfeito, cada passagem é percorrida no máximo uma vez em cada sentido.
    CHECK(s.walked <= 2 * m.passages());
}

TESTE(labirinto_de_uma_celula) {
    const lab::Maze m(1, 1);
    for (Solver s : kTodos) {
        const lab::Solution sol = lab::solve(m, s);
        CHECK_EQ(sol.path.size(), 1u);
        CHECK_EQ(sol.cost, 0);
    }
}

TESTE(caminho_invalido_e_detectado) {
    lab::Maze m(3, 1);
    m.carve(0, lab::E);
    CHECK(!lab::is_valid_path(m, {0, 1, 2}));  // parede entre 1 e 2
    m.carve(1, lab::E);
    CHECK(lab::is_valid_path(m, {0, 1, 2}));
    CHECK(!lab::is_valid_path(m, {0, 2}));      // pula uma célula
    CHECK(!lab::is_valid_path(m, {1, 2}));      // não começa na entrada
    CHECK(!lab::is_valid_path(m, {}));
}
