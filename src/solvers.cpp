#include "solvers.hpp"

#include <cstdlib>
#include <limits>
#include <queue>
#include <tuple>

namespace lab {

namespace {

const std::vector<SolverInfo> kSolvers = {
    {Solver::Bfs, "bfs", "busca em largura: menor número de passos"},
    {Solver::Dfs, "dfs", "busca em profundidade: acha um caminho, não necessariamente o menor"},
    {Solver::AStar, "astar", "A* com distância de Manhattan: menor caminho examinando menos células"},
    {Solver::Dijkstra, "dijkstra", "Dijkstra: menor custo quando as células têm pesos (--pesos)"},
    {Solver::DeadEnds, "becos", "preenchimento de becos sem saída: tapa becos até sobrar a solução"},
    {Solver::RightHand, "mao-direita", "seguidor de parede: anda sempre com a mão direita na parede"},
};

std::vector<int> rebuild(const std::vector<int>& prev, int goal) {
    std::vector<int> path;
    for (int c = goal; c != -1; c = prev[static_cast<std::size_t>(c)]) path.push_back(c);
    return std::vector<int>(path.rbegin(), path.rend());
}

void mark(Solution& s, int cell) {
    if (!s.explored[static_cast<std::size_t>(cell)]) {
        s.explored[static_cast<std::size_t>(cell)] = true;
        ++s.explored_count;
    }
}

Solution bfs(const Maze& m, const std::vector<bool>* allowed = nullptr) {
    Solution s;
    const std::size_t n = static_cast<std::size_t>(m.cells());
    s.explored.assign(n, false);
    std::vector<int> prev(n, -1);
    std::vector<bool> seen(n, false);
    std::queue<int> fila;
    seen[static_cast<std::size_t>(m.start())] = true;
    fila.push(m.start());
    while (!fila.empty()) {
        const int c = fila.front();
        fila.pop();
        mark(s, c);
        if (c == m.goal()) {
            s.path = rebuild(prev, c);
            break;
        }
        for (int nb : m.open_neighbors(c)) {
            const std::size_t u = static_cast<std::size_t>(nb);
            if (seen[u] || (allowed != nullptr && !(*allowed)[u])) continue;
            seen[u] = true;
            prev[u] = c;
            fila.push(nb);
        }
    }
    return s;
}

Solution dfs(const Maze& m) {
    Solution s;
    const std::size_t n = static_cast<std::size_t>(m.cells());
    s.explored.assign(n, false);
    std::vector<int> prev(n, -1);
    std::vector<bool> seen(n, false);
    std::vector<int> stack{m.start()};
    seen[static_cast<std::size_t>(m.start())] = true;
    while (!stack.empty()) {
        const int c = stack.back();
        stack.pop_back();
        mark(s, c);
        if (c == m.goal()) {
            s.path = rebuild(prev, c);
            break;
        }
        for (int nb : m.open_neighbors(c)) {
            const std::size_t u = static_cast<std::size_t>(nb);
            if (seen[u]) continue;
            seen[u] = true;
            prev[u] = c;
            stack.push_back(nb);
        }
    }
    return s;
}

// Dijkstra (heuristic = false) e A* (heuristic = true) com a mesma fila de prioridade.
// Com pesos de no mínimo 1, a distância de Manhattan nunca superestima o custo
// restante (é admissível), então o A* continua achando o caminho de menor custo.
Solution best_first(const Maze& m, const std::vector<int>* weights, bool heuristic) {
    Solution s;
    const std::size_t n = static_cast<std::size_t>(m.cells());
    s.explored.assign(n, false);
    const long long inf = std::numeric_limits<long long>::max();
    std::vector<long long> dist(n, inf);
    std::vector<int> prev(n, -1);
    const int gx = m.x_of(m.goal());
    const int gy = m.y_of(m.goal());
    auto h = [&](int c) -> long long {
        return heuristic ? std::abs(m.x_of(c) - gx) + std::abs(m.y_of(c) - gy) : 0;
    };
    // (f, h, célula): em empate de f, prefere quem está mais perto do objetivo.
    using Item = std::tuple<long long, long long, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    dist[static_cast<std::size_t>(m.start())] = 0;
    open.emplace(h(m.start()), h(m.start()), m.start());
    while (!open.empty()) {
        const auto [f, hc, c] = open.top();
        open.pop();
        const std::size_t uc = static_cast<std::size_t>(c);
        if (f - hc != dist[uc]) continue;  // entrada velha: já achamos caminho melhor
        mark(s, c);
        if (c == m.goal()) {
            s.path = rebuild(prev, c);
            s.cost = dist[uc];
            break;
        }
        for (int nb : m.open_neighbors(c)) {
            const std::size_t u = static_cast<std::size_t>(nb);
            const long long step = weights != nullptr ? (*weights)[u] : 1;
            if (dist[uc] + step < dist[u]) {
                dist[u] = dist[uc] + step;
                prev[u] = c;
                open.emplace(dist[u] + h(nb), h(nb), nb);
            }
        }
    }
    return s;
}

// Preenchimento de becos: tapa (marca) toda célula com no máximo uma passagem
// livre, exceto entrada e saída, e repete com as vizinhas que viraram becos.
// Num labirinto perfeito sobra exatamente o caminho da solução.
Solution dead_end_filling(const Maze& m) {
    const std::size_t n = static_cast<std::size_t>(m.cells());
    std::vector<int> free_deg(n);
    std::vector<bool> filled(n, false);
    std::vector<int> stack;
    for (int c = 0; c < m.cells(); ++c) {
        free_deg[static_cast<std::size_t>(c)] = m.degree(c);
        if (free_deg[static_cast<std::size_t>(c)] <= 1 && c != m.start() && c != m.goal()) stack.push_back(c);
    }
    std::vector<bool> examined(n, false);
    while (!stack.empty()) {
        const int c = stack.back();
        stack.pop_back();
        const std::size_t uc = static_cast<std::size_t>(c);
        if (filled[uc]) continue;
        filled[uc] = true;
        examined[uc] = true;
        for (int nb : m.open_neighbors(c)) {
            const std::size_t u = static_cast<std::size_t>(nb);
            if (filled[u]) continue;
            if (--free_deg[u] <= 1 && nb != m.start() && nb != m.goal()) stack.push_back(nb);
        }
    }
    // O que não foi tapado contém todas as soluções; se houver ciclos (labirinto
    // trançado), uma busca em largura restrita a essas células escolhe a menor.
    std::vector<bool> allowed(n);
    for (std::size_t i = 0; i < n; ++i) allowed[i] = !filled[i];
    Solution s = bfs(m, &allowed);
    for (std::size_t i = 0; i < n; ++i) {
        if (examined[i]) mark(s, static_cast<int>(i));
    }
    return s;
}

// Seguidor de parede: tenta virar à direita; se não der, segue em frente; se
// não der, vira à esquerda; senão, volta. Como entrada e saída ficam na borda
// externa, seguir a parede da direita sempre chega à saída. O caminho final é o
// percurso com os laços apagados (as idas e voltas em becos somem).
Solution right_hand(const Maze& m) {
    Solution s;
    const std::size_t n = static_cast<std::size_t>(m.cells());
    s.explored.assign(n, false);
    int c = m.start();
    Dir facing = S;  // entra pelo topo, olhando para baixo
    std::vector<int> walk{c};
    mark(s, c);
    const std::size_t limit = 16 * n + 16;  // cada parede é seguida no máximo uma vez de cada lado
    while (c != m.goal() && walk.size() < limit) {
        const Dir order[4] = {turn_right(facing), facing, turn_left(facing), opposite(facing)};
        for (Dir d : order) {
            if (m.is_open(c, d)) {
                facing = d;
                c = m.neighbor(c, d);
                break;
            }
        }
        walk.push_back(c);
        mark(s, c);
    }
    s.walked = walk.size() - 1;
    if (c != m.goal()) return s;
    // Apaga laços: ao revisitar uma célula, volta o caminho até a primeira visita.
    std::vector<int> pos(n, -1);
    for (int cell : walk) {
        const std::size_t u = static_cast<std::size_t>(cell);
        if (pos[u] >= 0) {
            while (static_cast<int>(s.path.size()) > pos[u] + 1) {
                pos[static_cast<std::size_t>(s.path.back())] = -1;
                s.path.pop_back();
            }
        } else {
            pos[u] = static_cast<int>(s.path.size());
            s.path.push_back(cell);
        }
    }
    return s;
}

}  // namespace

const std::vector<SolverInfo>& solver_list() { return kSolvers; }

bool parse_solver(const std::string& name, Solver& out) {
    for (const SolverInfo& s : kSolvers) {
        if (name == s.name) {
            out = s.id;
            return true;
        }
    }
    return false;
}

const char* solver_name(Solver s) {
    for (const SolverInfo& info : kSolvers) {
        if (info.id == s) return info.name;
    }
    return "?";
}

Solution solve(const Maze& maze, Solver s, const std::vector<int>* weights) {
    Solution out;
    switch (s) {
        case Solver::Bfs: out = bfs(maze); break;
        case Solver::Dfs: out = dfs(maze); break;
        case Solver::AStar: out = best_first(maze, weights, true); break;
        case Solver::Dijkstra: out = best_first(maze, weights, false); break;
        case Solver::DeadEnds: out = dead_end_filling(maze); break;
        case Solver::RightHand: out = right_hand(maze); break;
    }
    out.cost = path_cost(out.path, weights);
    if (out.walked == 0 && !out.path.empty()) out.walked = out.path.size() - 1;
    return out;
}

bool is_valid_path(const Maze& maze, const std::vector<int>& path) {
    if (path.empty() || path.front() != maze.start() || path.back() != maze.goal()) return false;
    for (std::size_t i = 1; i < path.size(); ++i) {
        bool linked = false;
        for (Dir d : kDirs) {
            if (maze.is_open(path[i - 1], d) && maze.neighbor(path[i - 1], d) == path[i]) linked = true;
        }
        if (!linked) return false;
    }
    return true;
}

long long path_cost(const std::vector<int>& path, const std::vector<int>* weights) {
    if (path.empty()) return 0;
    if (weights == nullptr) return static_cast<long long>(path.size()) - 1;
    long long total = 0;
    for (std::size_t i = 1; i < path.size(); ++i) total += (*weights)[static_cast<std::size_t>(path[i])];
    return total;
}

}  // namespace lab
