// Desenho do labirinto em texto: ASCII puro ou caracteres de desenho de caixa (Unicode).
#pragma once

#include <string>
#include <vector>

#include "maze.hpp"

namespace lab {

struct TextOptions {
    bool unicode = true;
    const std::vector<int>* path = nullptr;     // caminho a destacar
    const std::vector<int>* weights = nullptr;  // mostra o peso de cada célula
};

// Entrada marcada com E (no alto à esquerda) e saída com S (embaixo à direita).
std::string render_text(const Maze& maze, const TextOptions& options);

}  // namespace lab
