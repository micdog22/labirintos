#include "raster.hpp"

namespace lab {

const std::vector<Rgb>& palette() {
    static const std::vector<Rgb> colors = {
        {255, 255, 255},  // corredor
        {28, 31, 36},     // parede
        {200, 204, 210},  // pendente
        {191, 219, 254},  // explorada
        {234, 88, 12},    // caminho
        {22, 163, 74},    // entrada
        {220, 38, 38},    // saída
        {37, 99, 235},    // cursor
        // pesos de 1 (claro) a 9 (escuro)
        {254, 249, 231}, {253, 240, 200}, {252, 228, 165}, {249, 212, 128}, {240, 192, 96},
        {222, 168, 72}, {196, 140, 56}, {166, 112, 44}, {136, 86, 34},
    };
    return colors;
}

Image draw_maze(const Maze& m, const DrawOptions& o) {
    const int cs = o.cell;
    const int ws = o.wall;
    const int step = cs + ws;
    Image img(m.width() * step + ws, m.height() * step + ws, kWall);
    const std::size_t n = static_cast<std::size_t>(m.cells());

    // Direções em que o caminho sai de cada célula, para pintar também as passagens.
    std::vector<std::uint8_t> path_dirs(n, 0);
    std::vector<bool> on_path(n, false);
    if (o.path != nullptr) {
        const std::vector<int>& p = *o.path;
        for (std::size_t i = 0; i < p.size(); ++i) {
            on_path[static_cast<std::size_t>(p[i])] = true;
            if (i + 1 == p.size()) break;
            for (Dir d : kDirs) {
                if (m.neighbor(p[i], d) == p[i + 1]) {
                    path_dirs[static_cast<std::size_t>(p[i])] |= d;
                    path_dirs[static_cast<std::size_t>(p[i + 1])] |= opposite(d);
                }
            }
        }
    }

    auto cell_color = [&](int c) -> std::uint8_t {
        const std::size_t u = static_cast<std::size_t>(c);
        if (c == o.cursor) return kCursor;
        if (o.path != nullptr && c == m.start()) return kStart;
        if (o.path != nullptr && c == m.goal()) return kGoal;
        if (on_path[u]) return kPath;
        // Com pesos, o "terreno" diz mais que as células examinadas.
        if (o.weights != nullptr) return static_cast<std::uint8_t>(kWeight1 + (*o.weights)[u] - 1);
        if (o.explored != nullptr && (*o.explored)[u]) return kExplored;
        if (o.show_pending && m.degree(c) == 0) return kPending;
        return kFloor;
    };

    for (int c = 0; c < m.cells(); ++c) {
        const int px = ws + m.x_of(c) * step;
        const int py = ws + m.y_of(c) * step;
        const std::uint8_t color = cell_color(c);
        img.fill_rect(px, py, cs, cs, color);
        // Passagens para a direita e para baixo: a faixa da parede vira corredor.
        if (m.is_open(c, E)) {
            const bool both = (path_dirs[static_cast<std::size_t>(c)] & E) != 0;
            const int nb = m.neighbor(c, E);
            const std::uint8_t pass = both ? kPath : (color == cell_color(nb) ? color : kFloor);
            img.fill_rect(px + cs, py, ws, cs, pass);
        }
        if (m.is_open(c, S)) {
            const bool both = (path_dirs[static_cast<std::size_t>(c)] & S) != 0;
            const int nb = m.neighbor(c, S);
            const std::uint8_t pass = both ? kPath : (color == cell_color(nb) ? color : kFloor);
            img.fill_rect(px, py + cs, cs, ws, pass);
        }
    }
    // Aberturas na borda: entrada em cima da primeira célula e saída embaixo da última.
    const std::uint8_t edge = o.path != nullptr && !o.path->empty() ? kPath : kFloor;
    img.fill_rect(ws + m.x_of(m.start()) * step, 0, cs, ws, edge);
    img.fill_rect(ws + m.x_of(m.goal()) * step, img.height - ws, cs, ws, edge);

    // Cantos: só ficam escuros se alguma parede encosta neles (labirintos com
    // ciclos ou a divisão recursiva podem ter cantos soltos).
    for (int y = 0; y <= m.height(); ++y) {
        for (int x = 0; x <= m.width(); ++x) {
            bool any = x == 0 || y == 0 || x == m.width() || y == m.height();
            if (!any) {
                const int tl = m.index(x - 1, y - 1);  // célula acima e à esquerda do canto
                const int br = m.index(x, y);
                any = !m.is_open(tl, E) || !m.is_open(tl, S) || !m.is_open(br, N) || !m.is_open(br, W);
            }
            if (!any) {
                // Canto solto: pinta com a cor das quatro células em volta, se for a mesma.
                const std::uint8_t c1 = cell_color(m.index(x - 1, y - 1));
                const bool same = c1 == cell_color(m.index(x, y - 1)) && c1 == cell_color(m.index(x - 1, y)) &&
                                  c1 == cell_color(m.index(x, y));
                img.fill_rect(x * step, y * step, ws, ws, same ? c1 : kFloor);
            }
        }
    }
    return img;
}

}  // namespace lab
