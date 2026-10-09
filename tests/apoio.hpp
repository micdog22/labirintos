// Apoio aos testes dos labirintos.
#pragma once

#include <string>
#include <vector>

#include "generators.hpp"
#include "maze.hpp"
#include "teste.h"

// Tamanhos que pegam os casos de borda: 1x1, corredores de largura 1, quadrados e retângulos.
inline const std::vector<std::pair<int, int>>& tamanhos() {
    static const std::vector<std::pair<int, int>> t = {{1, 1}, {1, 7}, {7, 1}, {2, 2}, {5, 7}, {20, 20}, {31, 17}};
    return t;
}

inline lab::Maze gerar(lab::Generator g, int w, int h, std::uint64_t seed, double trancar = 0.0) {
    lab::Maze m(w, h);
    lab::Rng rng(seed);
    lab::generate(m, g, rng);
    if (trancar > 0) lab::braid(m, trancar, rng);
    return m;
}

// Toda passagem aparece dos dois lados e nenhuma sai da grade.
inline bool paredes_consistentes(const lab::Maze& m) {
    for (int c = 0; c < m.cells(); ++c) {
        for (lab::Dir d : lab::kDirs) {
            const int nb = m.neighbor(c, d);
            if (nb < 0) {
                if (m.is_open(c, d)) return false;
            } else if (m.is_open(c, d) != m.is_open(nb, lab::opposite(d))) {
                return false;
            }
        }
    }
    return true;
}
