#include "render.hpp"

namespace lab {

namespace {

// Cantos em desenho de caixa, indexados por quais paredes encostam no ponto:
// cima = 1, baixo = 2, esquerda = 4, direita = 8.
const char* const kCorners[16] = {
    " ",           "╵", "╷", "│",  // nada, cima, baixo, cima+baixo
    "╴",      "┘", "┐", "┤",  // esquerda, ...
    "╶",      "└", "┌", "├",  // direita, ...
    "─",      "┴", "┬", "┼",  // esquerda+direita, ...
};

class TextRenderer {
public:
    TextRenderer(const Maze& m, const TextOptions& o) : m_(m), o_(o), dirs_(static_cast<std::size_t>(m.cells()), 0) {
        if (o.path != nullptr) {
            const std::vector<int>& p = *o.path;
            on_path_.assign(static_cast<std::size_t>(m.cells()), false);
            for (std::size_t i = 0; i < p.size(); ++i) {
                on_path_[static_cast<std::size_t>(p[i])] = true;
                if (i + 1 < p.size()) {
                    for (Dir d : kDirs) {
                        if (m.neighbor(p[i], d) == p[i + 1]) {
                            dirs_[static_cast<std::size_t>(p[i])] |= d;
                            dirs_[static_cast<std::size_t>(p[i + 1])] |= opposite(d);
                        }
                    }
                }
            }
        }
    }

    std::string run() const {
        std::string out;
        for (int y = 0; y <= m_.height(); ++y) {
            out += wall_row(y);
            if (y < m_.height()) out += cell_row(y);
        }
        return out;
    }

private:
    const Maze& m_;
    TextOptions o_;
    std::vector<bool> on_path_;
    std::vector<std::uint8_t> dirs_;  // direções em que o caminho sai de cada célula

    bool path_through(int cell, Dir d) const { return (dirs_[static_cast<std::size_t>(cell)] & d) != 0; }
    bool on_path(int cell) const { return !on_path_.empty() && on_path_[static_cast<std::size_t>(cell)]; }
    const char* dot() const { return o_.unicode ? "·" : "."; }

    // Existe parede horizontal acima da célula (x, y)? (y == altura: borda de baixo)
    bool h_wall(int x, int y) const {
        if (y == 0) return m_.index(x, 0) != m_.start();
        if (y == m_.height()) return m_.index(x, y - 1) != m_.goal();
        return !m_.is_open(m_.index(x, y - 1), S);
    }

    // Existe parede vertical à esquerda da célula (x, y)? (x == largura: borda direita)
    bool v_wall(int x, int y) const {
        if (x == 0 || x == m_.width()) return true;
        return !m_.is_open(m_.index(x - 1, y), E);
    }

    std::string wall_row(int y) const {
        std::string line;
        for (int x = 0; x <= m_.width(); ++x) {
            line += corner(x, y);
            if (x == m_.width()) break;
            if (h_wall(x, y)) {
                line += o_.unicode ? "───" : "---";
            } else if (path_link_vertical(x, y)) {
                line += std::string(" ") + dot() + " ";
            } else {
                line += "   ";
            }
        }
        return line + "\n";
    }

    // O caminho atravessa a abertura acima da célula (x, y)? Inclui a entrada e a saída.
    bool path_link_vertical(int x, int y) const {
        if (y == 0) return on_path(m_.index(x, 0));
        if (y == m_.height()) return on_path(m_.index(x, y - 1));
        return path_through(m_.index(x, y - 1), S);
    }

    std::string cell_row(int y) const {
        std::string line;
        for (int x = 0; x <= m_.width(); ++x) {
            if (v_wall(x, y)) {
                line += o_.unicode ? "│" : "|";
            } else {
                line += path_through(m_.index(x - 1, y), E) ? dot() : " ";
            }
            if (x == m_.width()) break;
            line += interior(m_.index(x, y));
        }
        return line + "\n";
    }

    std::string interior(int cell) const {
        if (cell == m_.start()) return " E ";
        if (cell == m_.goal()) return " S ";
        if (o_.weights != nullptr) {
            const char digit = static_cast<char>('0' + (*o_.weights)[static_cast<std::size_t>(cell)] % 10);
            return on_path(cell) ? std::string("(") + digit + ")" : std::string(" ") + digit + " ";
        }
        return on_path(cell) ? std::string(" ") + dot() + " " : "   ";
    }

    std::string corner(int x, int y) const {
        if (!o_.unicode) return "+";
        int mask = 0;
        if (y > 0 && v_wall(x, y - 1)) mask |= 1;
        if (y < m_.height() && v_wall(x, y)) mask |= 2;
        if (x > 0 && h_wall(x - 1, y)) mask |= 4;
        if (x < m_.width() && h_wall(x, y)) mask |= 8;
        return kCorners[mask];
    }
};

}  // namespace

std::string render_text(const Maze& maze, const TextOptions& options) {
    return TextRenderer(maze, options).run();
}

}  // namespace lab
