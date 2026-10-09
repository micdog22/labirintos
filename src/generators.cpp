#include "generators.hpp"

#include <algorithm>
#include <map>
#include <numeric>

namespace lab {

namespace {

const std::vector<GeneratorInfo> kGenerators = {
    {Generator::Backtracker, "backtracker", "busca em profundidade: corredores longos e sinuosos, poucos becos"},
    {Generator::Prim, "prim", "Prim aleatório: cresce a partir de um ponto, muitos becos curtos"},
    {Generator::Kruskal, "kruskal", "Kruskal aleatório com union-find: muitos becos curtos, sem direção preferida"},
    {Generator::Wilson, "wilson", "passeios aleatórios sem laços: escolhe entre todos os labirintos com a mesma chance"},
    {Generator::Eller, "eller", "linha por linha, com memória proporcional à largura"},
    {Generator::AldousBroder, "aldous-broder", "passeio aleatório puro: sem viés, mas lento para terminar"},
    {Generator::Division, "divisao", "divisão recursiva: levanta paredes em vez de abrir, aspecto retangular"},
};

// Union-find simples (compressão por "halving" e união por tamanho), usado pelo
// Kruskal e pelo Eller.
class Sets {
public:
    explicit Sets(std::size_t n = 0) : parent_(n), size_(n, 1) { std::iota(parent_.begin(), parent_.end(), std::size_t{0}); }
    std::size_t add() {
        parent_.push_back(parent_.size());
        size_.push_back(1);
        return parent_.size() - 1;
    }
    std::size_t find(std::size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }
    bool unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        if (size_[a] < size_[b]) std::swap(a, b);
        parent_[b] = a;
        size_[a] += size_[b];
        return true;
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;
};

void step(const StepFn& on_step, const Maze& m, int cursor) {
    if (on_step) on_step(m, cursor);
}

// Direção aleatória entre as que não saem da grade.
Dir random_dir(const Maze& m, int cell, Rng& rng) {
    Dir options[4];
    int k = 0;
    for (Dir d : kDirs) {
        if (m.neighbor(cell, d) >= 0) options[k++] = d;
    }
    return options[rng.below(static_cast<std::uint64_t>(k))];
}

// Busca em profundidade com pilha explícita: anda para um vizinho ainda não
// visitado; quando não há nenhum, volta (backtrack).
void backtracker(Maze& m, Rng& rng, const StepFn& on_step) {
    const int n = m.cells();
    std::vector<bool> visited(static_cast<std::size_t>(n), false);
    std::vector<int> stack;
    const int first = static_cast<int>(rng.below(static_cast<std::uint64_t>(n)));
    visited[static_cast<std::size_t>(first)] = true;
    stack.push_back(first);
    while (!stack.empty()) {
        const int c = stack.back();
        Dir options[4];
        int k = 0;
        for (Dir d : kDirs) {
            const int nb = m.neighbor(c, d);
            if (nb >= 0 && !visited[static_cast<std::size_t>(nb)]) options[k++] = d;
        }
        if (k == 0) {
            stack.pop_back();
            continue;
        }
        const Dir d = options[rng.below(static_cast<std::uint64_t>(k))];
        const int nb = m.neighbor(c, d);
        m.carve(c, d);
        visited[static_cast<std::size_t>(nb)] = true;
        stack.push_back(nb);
        step(on_step, m, nb);
    }
}

// Prim aleatório: mantém a "fronteira" (células fora do labirinto vizinhas de
// alguma de dentro); sorteia uma e liga a um vizinho de dentro qualquer.
void prim(Maze& m, Rng& rng, const StepFn& on_step) {
    enum : unsigned char { Out, Frontier, In };
    const int n = m.cells();
    std::vector<unsigned char> state(static_cast<std::size_t>(n), Out);
    std::vector<int> frontier;
    auto add = [&](int c) {
        state[static_cast<std::size_t>(c)] = In;
        for (Dir d : kDirs) {
            const int nb = m.neighbor(c, d);
            if (nb >= 0 && state[static_cast<std::size_t>(nb)] == Out) {
                state[static_cast<std::size_t>(nb)] = Frontier;
                frontier.push_back(nb);
            }
        }
    };
    add(static_cast<int>(rng.below(static_cast<std::uint64_t>(n))));
    while (!frontier.empty()) {
        const std::size_t i = static_cast<std::size_t>(rng.below(frontier.size()));
        const int c = frontier[i];
        frontier[i] = frontier.back();
        frontier.pop_back();
        Dir options[4];
        int k = 0;
        for (Dir d : kDirs) {
            const int nb = m.neighbor(c, d);
            if (nb >= 0 && state[static_cast<std::size_t>(nb)] == In) options[k++] = d;
        }
        m.carve(c, options[rng.below(static_cast<std::uint64_t>(k))]);
        add(c);
        step(on_step, m, c);
    }
}

// Kruskal aleatório: embaralha todas as paredes internas e derruba cada uma que
// separa células de conjuntos diferentes (o union-find diz isso em O(α(n))).
void kruskal(Maze& m, Rng& rng, const StepFn& on_step) {
    struct Wall {
        int cell;
        Dir dir;
    };
    std::vector<Wall> walls;
    for (int c = 0; c < m.cells(); ++c) {
        if (m.neighbor(c, E) >= 0) walls.push_back({c, E});
        if (m.neighbor(c, S) >= 0) walls.push_back({c, S});
    }
    rng.shuffle(walls);
    Sets sets(static_cast<std::size_t>(m.cells()));
    for (const Wall& w : walls) {
        const int nb = m.neighbor(w.cell, w.dir);
        if (sets.unite(static_cast<std::size_t>(w.cell), static_cast<std::size_t>(nb))) {
            m.carve(w.cell, w.dir);
            step(on_step, m, nb);
        }
    }
}

// Wilson: a partir de cada célula fora da árvore, faz um passeio aleatório até
// encostar na árvore. Guardando só a última saída de cada célula, os laços do
// passeio se apagam sozinhos; o caminho que sobra é incorporado à árvore.
void wilson(Maze& m, Rng& rng, const StepFn& on_step) {
    const int n = m.cells();
    std::vector<bool> in_tree(static_cast<std::size_t>(n), false);
    std::vector<Dir> exit_dir(static_cast<std::size_t>(n), N);
    in_tree[rng.below(static_cast<std::uint64_t>(n))] = true;
    std::vector<int> order(static_cast<std::size_t>(n));
    std::iota(order.begin(), order.end(), 0);
    rng.shuffle(order);
    for (int start : order) {
        int c = start;
        while (!in_tree[static_cast<std::size_t>(c)]) {
            const Dir d = random_dir(m, c, rng);
            exit_dir[static_cast<std::size_t>(c)] = d;
            c = m.neighbor(c, d);
        }
        c = start;
        while (!in_tree[static_cast<std::size_t>(c)]) {
            const Dir d = exit_dir[static_cast<std::size_t>(c)];
            m.carve(c, d);
            in_tree[static_cast<std::size_t>(c)] = true;
            c = m.neighbor(c, d);
            step(on_step, m, c);
        }
    }
}

// Eller: processa uma linha por vez. Cada célula pertence a um conjunto; une
// vizinhas da linha ao acaso (se forem de conjuntos diferentes) e cada conjunto
// desce para a linha seguinte por pelo menos uma célula. Na última linha, une
// tudo o que ainda estiver separado.
void eller(Maze& m, Rng& rng, const StepFn& on_step) {
    const int w = m.width();
    const int h = m.height();
    Sets sets;
    std::vector<std::size_t> row(static_cast<std::size_t>(w));
    std::vector<bool> has_set(static_cast<std::size_t>(w), false);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!has_set[static_cast<std::size_t>(x)]) {
                row[static_cast<std::size_t>(x)] = sets.add();
                has_set[static_cast<std::size_t>(x)] = true;
            }
        }
        const bool last = y == h - 1;
        for (int x = 0; x + 1 < w; ++x) {
            const std::size_t a = row[static_cast<std::size_t>(x)];
            const std::size_t b = row[static_cast<std::size_t>(x) + 1];
            if (sets.find(a) != sets.find(b) && (last || rng.chance(0.5))) {
                sets.unite(a, b);
                m.carve(m.index(x, y), E);
                step(on_step, m, m.index(x + 1, y));
            }
        }
        if (last) break;
        // Agrupa as colunas por conjunto (std::map: ordem determinística).
        std::map<std::size_t, std::vector<int>> groups;
        for (int x = 0; x < w; ++x) groups[sets.find(row[static_cast<std::size_t>(x)])].push_back(x);
        std::fill(has_set.begin(), has_set.end(), false);
        for (auto& [id, xs] : groups) {
            rng.shuffle(xs);
            const std::size_t down = 1 + static_cast<std::size_t>(rng.below(xs.size()));
            for (std::size_t i = 0; i < down; ++i) {
                const int x = xs[i];
                m.carve(m.index(x, y), S);
                has_set[static_cast<std::size_t>(x)] = true;  // a célula de baixo herda o conjunto
                step(on_step, m, m.index(x, y + 1));
            }
            (void)id;
        }
    }
}

// Aldous-Broder: anda ao acaso pela grade e, toda vez que entra em uma célula
// nunca visitada, abre a parede por onde entrou.
void aldous_broder(Maze& m, Rng& rng, const StepFn& on_step) {
    const int n = m.cells();
    std::vector<bool> visited(static_cast<std::size_t>(n), false);
    int c = static_cast<int>(rng.below(static_cast<std::uint64_t>(n)));
    visited[static_cast<std::size_t>(c)] = true;
    int remaining = n - 1;
    while (remaining > 0) {
        const Dir d = random_dir(m, c, rng);
        const int nb = m.neighbor(c, d);
        if (!visited[static_cast<std::size_t>(nb)]) {
            m.carve(c, d);
            visited[static_cast<std::size_t>(nb)] = true;
            --remaining;
            step(on_step, m, nb);
        }
        c = nb;
    }
}

// Divisão recursiva: começa sem paredes internas e divide cada câmara com uma
// parede que tem uma única passagem, até as câmaras terem 1 de largura ou altura.
void division(Maze& m, Rng& rng, const StepFn& on_step) {
    m.open_all();
    struct Chamber {
        int x, y, w, h;
    };
    std::vector<Chamber> stack{{0, 0, m.width(), m.height()}};
    while (!stack.empty()) {
        const Chamber c = stack.back();
        stack.pop_back();
        if (c.w < 2 || c.h < 2) continue;
        // Câmara mais alta que larga: parede horizontal (e vice-versa); se for
        // quadrada, sorteia.
        const bool horizontal = c.h > c.w || (c.h == c.w && rng.below(2) == 0);
        if (horizontal) {
            const int wy = c.y + static_cast<int>(rng.below(static_cast<std::uint64_t>(c.h - 1)));
            const int gap = c.x + static_cast<int>(rng.below(static_cast<std::uint64_t>(c.w)));
            for (int x = c.x; x < c.x + c.w; ++x) {
                if (x == gap) continue;
                m.build_wall(m.index(x, wy), S);
                step(on_step, m, m.index(x, wy));
            }
            stack.push_back({c.x, c.y, c.w, wy - c.y + 1});
            stack.push_back({c.x, wy + 1, c.w, c.y + c.h - wy - 1});
        } else {
            const int wx = c.x + static_cast<int>(rng.below(static_cast<std::uint64_t>(c.w - 1)));
            const int gap = c.y + static_cast<int>(rng.below(static_cast<std::uint64_t>(c.h)));
            for (int y = c.y; y < c.y + c.h; ++y) {
                if (y == gap) continue;
                m.build_wall(m.index(wx, y), E);
                step(on_step, m, m.index(wx, y));
            }
            stack.push_back({c.x, c.y, wx - c.x + 1, c.h});
            stack.push_back({wx + 1, c.y, c.x + c.w - wx - 1, c.h});
        }
    }
}

}  // namespace

const std::vector<GeneratorInfo>& generator_list() { return kGenerators; }

bool parse_generator(const std::string& name, Generator& out) {
    for (const GeneratorInfo& g : kGenerators) {
        if (name == g.name) {
            out = g.id;
            return true;
        }
    }
    return false;
}

const char* generator_name(Generator g) {
    for (const GeneratorInfo& info : kGenerators) {
        if (info.id == g) return info.name;
    }
    return "?";
}

void generate(Maze& maze, Generator g, Rng& rng, const StepFn& on_step) {
    switch (g) {
        case Generator::Backtracker: backtracker(maze, rng, on_step); break;
        case Generator::Prim: prim(maze, rng, on_step); break;
        case Generator::Kruskal: kruskal(maze, rng, on_step); break;
        case Generator::Wilson: wilson(maze, rng, on_step); break;
        case Generator::Eller: eller(maze, rng, on_step); break;
        case Generator::AldousBroder: aldous_broder(maze, rng, on_step); break;
        case Generator::Division: division(maze, rng, on_step); break;
    }
}

int braid(Maze& maze, double fraction, Rng& rng) {
    std::vector<int> dead_ends;
    for (int c = 0; c < maze.cells(); ++c) {
        if (maze.degree(c) == 1) dead_ends.push_back(c);
    }
    rng.shuffle(dead_ends);
    int removed = 0;
    for (int c : dead_ends) {
        if (maze.degree(c) != 1 || !rng.chance(fraction)) continue;  // pode ter deixado de ser beco
        Dir best[4];
        Dir any[4];
        int nb_best = 0;
        int nb_any = 0;
        for (Dir d : kDirs) {
            const int nb = maze.neighbor(c, d);
            if (nb < 0 || maze.is_open(c, d)) continue;
            any[nb_any++] = d;
            if (maze.degree(nb) == 1) best[nb_best++] = d;  // ligar dois becos resolve os dois
        }
        if (nb_any == 0) continue;  // corredor de largura 1: não há para onde abrir
        const Dir d = nb_best > 0 ? best[rng.below(static_cast<std::uint64_t>(nb_best))]
                                  : any[rng.below(static_cast<std::uint64_t>(nb_any))];
        maze.carve(c, d);
        ++removed;
    }
    return removed;
}

std::vector<int> random_weights(const Maze& maze, Rng& rng) {
    std::vector<int> w(static_cast<std::size_t>(maze.cells()));
    for (int& x : w) x = 1 + static_cast<int>(rng.below(9));
    return w;
}

}  // namespace lab
