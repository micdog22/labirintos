// Estatísticas de um labirinto: becos, bifurcações, ciclos e o maior caminho.
#pragma once

#include <cstddef>
#include <string>

#include "maze.hpp"

namespace lab {

struct Stats {
    int cells = 0;
    std::size_t passages = 0;
    long long cycles = 0;      // passagens a mais que uma árvore teria (cells - 1)
    int dead_ends = 0;         // células com uma única passagem
    int junctions = 0;         // células com três ou quatro passagens
    int corridors = 0;         // células com exatamente duas passagens
    bool connected = false;
    bool perfect = false;      // conexo e sem ciclos: um único caminho entre quaisquer duas células
    int longest = 0;           // maior caminho (em passos) entre duas células
    int longest_from = 0;
    int longest_to = 0;
};

// O maior caminho é calculado com duas buscas em largura: a célula mais
// distante de qualquer ponto é uma ponta do maior caminho. É exato em
// labirintos perfeitos (árvores); com ciclos, é uma boa estimativa.
Stats compute_stats(const Maze& maze);

}  // namespace lab
