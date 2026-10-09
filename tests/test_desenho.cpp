// Desenho em texto (com uma "fotografia" de um labirinto pequeno) e estatísticas.
#include <string>

#include "apoio.hpp"
#include "render.hpp"
#include "solvers.hpp"
#include "stats.hpp"
#include "teste.h"

namespace {

// Saídas esperadas (conferidas visualmente na primeira vez).
const char* const kFotoAscii =
    "+ . +---+---+---+---+---+\n"
    "| E . . . .     |       |\n"
    "+   +---+ . +   +   +   +\n"
    "|   |   | . |       |   |\n"
    "+   +   + . +---+   +---+\n"
    "|   |   | . |       |   |\n"
    "+   +   + . +---+---+   +\n"
    "|       | . . . . . . S |\n"
    "+---+---+---+---+---+ . +\n";

const char* const kFotoUnicode =
    "╷   ╶───┬───────┐\n"
    "│ E     │       │\n"
    "│   ╶───┘   ╶───┤\n"
    "│               │\n"
    "├───╴   ╶───┐   │\n"
    "│           │ S │\n"
    "└───────────┘   ╵\n";

}  // namespace

TESTE(fotografia_ascii) {
    // Se o gerador, o sorteio ou o desenho mudarem, esta comparação acusa.
    const lab::Maze m = gerar(lab::Generator::Backtracker, 6, 4, 1);
    const lab::Solution s = lab::solve(m, lab::Solver::Bfs);
    lab::TextOptions t;
    t.unicode = false;
    t.path = &s.path;
    CHECK_EQ(lab::render_text(m, t), std::string(kFotoAscii));
}

TESTE(fotografia_unicode) {
    const lab::Maze m = gerar(lab::Generator::Kruskal, 4, 3, 2);
    lab::TextOptions t;
    CHECK_EQ(lab::render_text(m, t), std::string(kFotoUnicode));
}

TESTE(desenho_com_pesos) {
    lab::Maze m(3, 1);
    m.carve(0, lab::E);
    m.carve(1, lab::E);
    const std::vector<int> w = {4, 7, 2};
    const std::vector<int> path = {0, 1, 2};
    lab::TextOptions t;
    t.unicode = false;
    t.weights = &w;
    t.path = &path;
    CHECK_EQ(lab::render_text(m, t), std::string("+ . +---+---+\n"
                                                  "| E .(7). S |\n"
                                                  "+---+---+ . +\n"));
}

TESTE(estatisticas_de_um_corredor) {
    lab::Maze m(5, 1);
    for (int x = 0; x < 4; ++x) m.carve(x, lab::E);
    const lab::Stats s = lab::compute_stats(m);
    CHECK(s.perfect);
    CHECK_EQ(s.dead_ends, 2);
    CHECK_EQ(s.corridors, 3);
    CHECK_EQ(s.junctions, 0);
    CHECK_EQ(s.longest, 4);
    // Um ciclo: grade 2x2 toda aberta.
    lab::Maze q(2, 2);
    q.open_all();
    const lab::Stats c = lab::compute_stats(q);
    CHECK(!c.perfect);
    CHECK_EQ(c.cycles, 1);
    CHECK_EQ(c.dead_ends, 0);
    CHECK_EQ(c.longest, 2);
}
