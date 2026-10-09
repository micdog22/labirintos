// Grade de células com paredes entre vizinhas, e o gerador de números aleatórios.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lab {

// Direções como bits: cada célula guarda em 4 bits para onde há passagem.
enum Dir : std::uint8_t { N = 1, S = 2, E = 4, W = 8 };
constexpr Dir kDirs[4] = {N, S, E, W};

int dx(Dir d) noexcept;
int dy(Dir d) noexcept;  // y cresce para baixo
Dir opposite(Dir d) noexcept;
Dir turn_right(Dir d) noexcept;
Dir turn_left(Dir d) noexcept;

// splitmix64: simples, rápido e igual em qualquer plataforma, então a mesma
// semente gera exatamente o mesmo labirinto.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() noexcept;
    std::uint64_t below(std::uint64_t n) noexcept;  // uniforme em [0, n), sem viés
    bool chance(double p) noexcept;                 // verdadeiro com probabilidade p

    template <class T>
    void shuffle(std::vector<T>& v) noexcept {  // Fisher-Yates
        for (std::size_t i = v.size(); i > 1; --i) std::swap(v[i - 1], v[below(i)]);
    }

private:
    std::uint64_t state_;
};

class Maze {
public:
    static constexpr int kMaxSide = 2000;

    // Labirinto com todas as paredes fechadas. Lança std::invalid_argument se o
    // tamanho for inválido.
    Maze(int width, int height);

    int width() const noexcept { return w_; }
    int height() const noexcept { return h_; }
    int cells() const noexcept { return w_ * h_; }
    int index(int x, int y) const noexcept { return y * w_ + x; }
    int x_of(int cell) const noexcept { return cell % w_; }
    int y_of(int cell) const noexcept { return cell / w_; }

    // Entrada no alto à esquerda e saída embaixo à direita.
    int start() const noexcept { return 0; }
    int goal() const noexcept { return cells() - 1; }

    // Célula vizinha na direção d, ou -1 se sair da grade.
    int neighbor(int cell, Dir d) const noexcept;
    bool is_open(int cell, Dir d) const noexcept { return (open_[static_cast<std::size_t>(cell)] & d) != 0; }
    std::uint8_t openings(int cell) const noexcept { return open_[static_cast<std::size_t>(cell)]; }

    // Abre (ou fecha) a parede entre a célula e a vizinha, dos dois lados.
    void carve(int cell, Dir d);
    void build_wall(int cell, Dir d);
    // Abre todas as paredes internas (ponto de partida da divisão recursiva).
    void open_all();

    int degree(int cell) const noexcept;  // quantas passagens a célula tem
    std::vector<int> open_neighbors(int cell) const;
    std::size_t passages() const noexcept;  // quantidade de paredes internas abertas

    // Representação compacta, para comparar labirintos nos testes.
    std::string serialize() const;

private:
    int w_;
    int h_;
    std::vector<std::uint8_t> open_;
};

}  // namespace lab
