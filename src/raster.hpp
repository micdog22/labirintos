// Desenho do labirinto em uma imagem com paleta (para gravar em PNG).
#pragma once

#include <vector>

#include "maze.hpp"
#include "png.hpp"

namespace lab {

// Índices da paleta.
enum Color : std::uint8_t {
    kFloor = 0,     // corredor
    kWall = 1,      // parede
    kPending = 2,   // célula ainda fora do labirinto (quadros da geração)
    kExplored = 3,  // examinada pelo algoritmo de solução
    kPath = 4,      // caminho da solução
    kStart = 5,
    kGoal = 6,
    kCursor = 7,    // célula atual do gerador (quadros)
    kWeight1 = 8,   // pesos 1 a 9 usam os índices 8 a 16
};

const std::vector<Rgb>& palette();

struct DrawOptions {
    int cell = 12;                                 // lado da célula em pixels
    int wall = 2;                                  // espessura da parede em pixels
    const std::vector<int>* path = nullptr;
    const std::vector<bool>* explored = nullptr;
    const std::vector<int>* weights = nullptr;
    bool show_pending = false;                     // células sem passagem ficam cinza
    int cursor = -1;
};

Image draw_maze(const Maze& maze, const DrawOptions& options);

}  // namespace lab
