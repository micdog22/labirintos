// Algoritmos para achar o caminho da entrada (célula 0) até a saída (última célula).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "maze.hpp"

namespace lab {

enum class Solver { Bfs, Dfs, AStar, Dijkstra, DeadEnds, RightHand };

struct SolverInfo {
    Solver id;
    const char* name;
    const char* description;
};

const std::vector<SolverInfo>& solver_list();
bool parse_solver(const std::string& name, Solver& out);
const char* solver_name(Solver s);

struct Solution {
    std::vector<int> path;       // da entrada até a saída, inclusive; vazio se não houver caminho
    std::vector<bool> explored;  // células que o algoritmo examinou
    std::size_t explored_count = 0;
    long long cost = 0;          // soma dos pesos das células em que entrou (ou passos, sem pesos)
    std::size_t walked = 0;      // passos dados de fato (mão direita: inclui idas e voltas)
};

// weights: custo para entrar em cada célula (opcional; sem pesos, cada passo custa 1).
// Só Dijkstra e A* levam os pesos em conta.
Solution solve(const Maze& maze, Solver s, const std::vector<int>* weights = nullptr);

// O caminho começa na entrada, termina na saída e só passa por paredes abertas?
bool is_valid_path(const Maze& maze, const std::vector<int>& path);

// Custo de um caminho: soma dos pesos das células depois da primeira.
long long path_cost(const std::vector<int>& path, const std::vector<int>* weights);

}  // namespace lab
