// Geradores de labirintos perfeitos (exatamente um caminho entre quaisquer duas
// células) e o "trançado", que remove becos sem saída e cria ciclos.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "maze.hpp"

namespace lab {

enum class Generator { Backtracker, Prim, Kruskal, Wilson, Eller, AldousBroder, Division };

struct GeneratorInfo {
    Generator id;
    const char* name;         // nome usado na linha de comando
    const char* description;  // uma linha sobre o estilo do labirinto
};

const std::vector<GeneratorInfo>& generator_list();
bool parse_generator(const std::string& name, Generator& out);
const char* generator_name(Generator g);

// Chamado a cada passo da geração (uma parede aberta ou levantada), com a
// célula "atual" do algoritmo (-1 se não houver). Usado para salvar quadros.
using StepFn = std::function<void(const Maze&, int cursor)>;

// Gera um labirinto perfeito na grade (que deve começar com todas as paredes fechadas).
void generate(Maze& maze, Generator g, Rng& rng, const StepFn& on_step = nullptr);

// Remove becos sem saída: cada um, com probabilidade `fraction`, ganha uma
// passagem nova (de preferência para outro beco). Devolve quantos foram removidos.
int braid(Maze& maze, double fraction, Rng& rng);

// Custo de 1 a 9 para entrar em cada célula (usado pelo Dijkstra e pelo A*).
std::vector<int> random_weights(const Maze& maze, Rng& rng);

}  // namespace lab
