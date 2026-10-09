#include "maze.hpp"

#include <stdexcept>

namespace lab {

int dx(Dir d) noexcept { return d == E ? 1 : (d == W ? -1 : 0); }
int dy(Dir d) noexcept { return d == S ? 1 : (d == N ? -1 : 0); }

Dir opposite(Dir d) noexcept {
    switch (d) {
        case N: return S;
        case S: return N;
        case E: return W;
        default: return E;
    }
}

// Com y para baixo, girar à direita (sentido horário na tela) é N -> E -> S -> W.
Dir turn_right(Dir d) noexcept {
    switch (d) {
        case N: return E;
        case E: return S;
        case S: return W;
        default: return N;
    }
}

Dir turn_left(Dir d) noexcept { return opposite(turn_right(d)); }

std::uint64_t Rng::next() noexcept {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t Rng::below(std::uint64_t n) noexcept {
    // Descarta o "resto" do topo para todos os valores terem a mesma chance.
    const std::uint64_t rest = (UINT64_MAX % n + 1) % n;
    std::uint64_t x = next();
    while (rest != 0 && x > UINT64_MAX - rest) x = next();
    return x % n;
}

bool Rng::chance(double p) noexcept {
    return static_cast<double>(next() >> 11) * 0x1.0p-53 < p;
}

Maze::Maze(int width, int height) : w_(width), h_(height) {
    if (width < 1 || height < 1 || width > kMaxSide || height > kMaxSide) {
        throw std::invalid_argument("largura e altura precisam estar entre 1 e " + std::to_string(kMaxSide));
    }
    open_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
}

int Maze::neighbor(int cell, Dir d) const noexcept {
    const int x = x_of(cell) + dx(d);
    const int y = y_of(cell) + dy(d);
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return -1;
    return index(x, y);
}

void Maze::carve(int cell, Dir d) {
    const int nb = neighbor(cell, d);
    if (nb < 0) throw std::logic_error("tentativa de abrir a parede externa do labirinto");
    open_[static_cast<std::size_t>(cell)] |= d;
    open_[static_cast<std::size_t>(nb)] |= opposite(d);
}

void Maze::build_wall(int cell, Dir d) {
    const int nb = neighbor(cell, d);
    if (nb < 0) return;
    open_[static_cast<std::size_t>(cell)] &= static_cast<std::uint8_t>(~d);
    open_[static_cast<std::size_t>(nb)] &= static_cast<std::uint8_t>(~opposite(d));
}

void Maze::open_all() {
    for (int c = 0; c < cells(); ++c) {
        for (Dir d : kDirs) {
            if (neighbor(c, d) >= 0) open_[static_cast<std::size_t>(c)] |= d;
        }
    }
}

int Maze::degree(int cell) const noexcept {
    int n = 0;
    for (Dir d : kDirs) n += is_open(cell, d) ? 1 : 0;
    return n;
}

std::vector<int> Maze::open_neighbors(int cell) const {
    std::vector<int> out;
    for (Dir d : kDirs) {
        if (is_open(cell, d)) out.push_back(neighbor(cell, d));
    }
    return out;
}

std::size_t Maze::passages() const noexcept {
    std::size_t total = 0;
    for (int c = 0; c < cells(); ++c) {
        total += is_open(c, E) ? 1 : 0;
        total += is_open(c, S) ? 1 : 0;
    }
    return total;
}

std::string Maze::serialize() const {
    static const char* const hex = "0123456789abcdef";
    std::string out = std::to_string(w_) + "x" + std::to_string(h_) + ":";
    for (std::uint8_t v : open_) out += hex[v & 15];
    return out;
}

}  // namespace lab
