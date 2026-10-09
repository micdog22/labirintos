// labirinto: gera, resolve e desenha labirintos.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "generators.hpp"
#include "maze.hpp"
#include "png.hpp"
#include "raster.hpp"
#include "render.hpp"
#include "solvers.hpp"
#include "stats.hpp"

namespace {

const char* const kVersion = "1.0.0";

const char* const kHelp =
    "Uso: labirinto [opções]\n"
    "\n"
    "Gera um labirinto, resolve e desenha no terminal (e, se quiser, em PNG).\n"
    "\n"
    "Tamanho e geração:\n"
    "  -l, --largura N       colunas (padrão 20)\n"
    "  -a, --altura N        linhas (padrão 10)\n"
    "  -g, --gerador NOME    backtracker, prim, kruskal, wilson, eller, aldous-broder\n"
    "                        ou divisao (padrão backtracker)\n"
    "  -s, --semente N       semente do sorteio: a mesma semente gera o mesmo labirinto\n"
    "      --trancar F       remove a fração F (de 0 a 1) dos becos sem saída, criando ciclos\n"
    "      --pesos           dá a cada célula um custo de 1 a 9 (para dijkstra e astar)\n"
    "\n"
    "Solução:\n"
    "  -r, --resolver NOME   bfs, dfs, astar, dijkstra, becos, mao-direita ou nenhum\n"
    "                        (padrão bfs)\n"
    "\n"
    "Saída:\n"
    "      --ascii           desenha só com ASCII (o padrão usa caracteres de caixa Unicode)\n"
    "      --sem-desenho     não desenha no terminal\n"
    "      --estatisticas    becos, bifurcações, ciclos e o maior caminho\n"
    "      --png ARQUIVO     grava a imagem com a solução e as células examinadas\n"
    "      --celula N        lado da célula no PNG, em pixels (padrão 12)\n"
    "      --quadros PASTA   grava a geração passo a passo em PNGs numerados\n"
    "      --a-cada N        um quadro a cada N passos (padrão: perto de 100 quadros)\n"
    "      --listar          lista os geradores e os resolvedores\n"
    "      --versao          mostra a versão\n"
    "      --ajuda           mostra esta ajuda\n"
    "\n"
    "Exemplos:\n"
    "  labirinto -l 30 -a 12 -g wilson -s 7\n"
    "  labirinto -g kruskal --trancar 0.5 -r astar --png labirinto.png\n"
    "  labirinto -g divisao --quadros quadros --celula 8\n";

struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Options {
    int width = 20;
    int height = 10;
    lab::Generator generator = lab::Generator::Backtracker;
    bool has_seed = false;
    std::uint64_t seed = 0;
    double braid = 0.0;
    bool weights = false;
    bool solve = true;
    lab::Solver solver = lab::Solver::Bfs;
    bool ascii = false;
    bool draw = true;
    bool stats = false;
    std::string png;
    int cell = 12;
    std::string frames_dir;
    long every = 0;
};

long parse_long(const std::string& option, const std::string& text, long min, long max) {
    char* end = nullptr;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (text.empty() || end == nullptr || *end != '\0' || v < min || v > max) {
        throw UsageError(option + " precisa de um número inteiro de " + std::to_string(min) + " a " +
                         std::to_string(max) + " (recebeu \"" + text + "\")");
    }
    return v;
}

double parse_fraction(const std::string& option, std::string text) {
    for (char& c : text) {
        if (c == ',') c = '.';  // aceita 0,5 e 0.5
    }
    char* end = nullptr;
    const double v = std::strtod(text.c_str(), &end);
    if (text.empty() || end == nullptr || *end != '\0' || !(v >= 0.0 && v <= 1.0)) {
        throw UsageError(option + " precisa de um número de 0 a 1 (recebeu \"" + text + "\")");
    }
    return v;
}

Options parse(int argc, char** argv, bool& exit_now) {
    Options o;
    exit_now = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw UsageError(a + " precisa de um valor");
            return argv[++i];
        };
        if (a == "--ajuda" || a == "-h" || a == "--help") {
            std::cout << kHelp;
            exit_now = true;
            return o;
        }
        if (a == "--versao" || a == "--version") {
            std::cout << "labirinto " << kVersion << "\n";
            exit_now = true;
            return o;
        }
        if (a == "--listar") {
            std::cout << "Geradores:\n";
            for (const auto& g : lab::generator_list()) std::printf("  %-14s %s\n", g.name, g.description);
            std::cout << "\nResolvedores:\n";
            for (const auto& s : lab::solver_list()) std::printf("  %-14s %s\n", s.name, s.description);
            exit_now = true;
            return o;
        }
        if (a == "-l" || a == "--largura") {
            o.width = static_cast<int>(parse_long(a, value(), 1, lab::Maze::kMaxSide));
        } else if (a == "-a" || a == "--altura") {
            o.height = static_cast<int>(parse_long(a, value(), 1, lab::Maze::kMaxSide));
        } else if (a == "-g" || a == "--gerador") {
            const std::string name = value();
            if (!lab::parse_generator(name, o.generator)) {
                throw UsageError("gerador desconhecido: " + name + " (veja --listar)");
            }
        } else if (a == "-s" || a == "--semente") {
            o.seed = static_cast<std::uint64_t>(parse_long(a, value(), 0, 2147483647L));
            o.has_seed = true;
        } else if (a == "--trancar") {
            o.braid = parse_fraction(a, value());
        } else if (a == "--pesos") {
            o.weights = true;
        } else if (a == "-r" || a == "--resolver") {
            const std::string name = value();
            if (name == "nenhum") {
                o.solve = false;
            } else if (!lab::parse_solver(name, o.solver)) {
                throw UsageError("resolvedor desconhecido: " + name + " (veja --listar)");
            }
        } else if (a == "--ascii") {
            o.ascii = true;
        } else if (a == "--sem-desenho") {
            o.draw = false;
        } else if (a == "--estatisticas") {
            o.stats = true;
        } else if (a == "--png") {
            o.png = value();
        } else if (a == "--celula") {
            o.cell = static_cast<int>(parse_long(a, value(), 2, 64));
        } else if (a == "--quadros") {
            o.frames_dir = value();
        } else if (a == "--a-cada") {
            o.every = parse_long(a, value(), 1, 100000000L);
        } else {
            throw UsageError("opção desconhecida: " + a);
        }
    }
    return o;
}

std::string percent(double part, double total) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f%%", total > 0 ? 100.0 * part / total : 0.0);
    std::string s = buf;
    for (char& c : s) {
        if (c == '.') c = ',';
    }
    return s;
}

std::string coord(const lab::Maze& m, int cell) {
    return "(" + std::to_string(m.x_of(cell)) + ", " + std::to_string(m.y_of(cell)) + ")";
}

// Grava um PNG a cada `every` passos da geração.
class FrameWriter {
public:
    FrameWriter(std::string dir, long every, int cell) : dir_(std::move(dir)), every_(every), cell_(cell) {}

    void step(const lab::Maze& m, int cursor) {
        if (++steps_ % every_ == 0) save(m, cursor);
    }

    void finish(const lab::Maze& m) {
        if (steps_ % every_ != 0 || saved_ == 0) save(m, -1);
    }

    int saved() const { return saved_; }

private:
    std::string dir_;
    long every_;
    int cell_;
    long steps_ = 0;
    int saved_ = 0;

    void save(const lab::Maze& m, int cursor) {
        lab::DrawOptions d;
        d.cell = cell_;
        d.show_pending = true;
        d.cursor = cursor;
        char name[32];
        std::snprintf(name, sizeof name, "quadro-%05d.png", ++saved_);
        const std::string path = (std::filesystem::path(dir_) / name).string();
        if (!lab::write_file(path, lab::encode_png(lab::draw_maze(m, d), lab::palette()))) {
            throw std::runtime_error("não foi possível gravar " + path);
        }
    }
};

int run(int argc, char** argv) {
    bool exit_now = false;
    const Options o = parse(argc, argv, exit_now);
    if (exit_now) return 0;

    const std::uint64_t seed = o.has_seed ? o.seed : (std::random_device{}() & 0x7FFFFFFF);
    lab::Rng rng(seed);
    lab::Maze maze(o.width, o.height);

    if (!o.frames_dir.empty()) {
        std::filesystem::create_directories(o.frames_dir);
        const long every = o.every > 0 ? o.every : std::max(1L, static_cast<long>(maze.cells()) / 100);
        FrameWriter frames(o.frames_dir, every, o.cell);
        lab::generate(maze, o.generator, rng, [&](const lab::Maze& m, int cursor) { frames.step(m, cursor); });
        frames.finish(maze);
        std::cout << frames.saved() << " quadros gravados em " << o.frames_dir << "\n";
    } else {
        lab::generate(maze, o.generator, rng);
    }

    int removed = 0;
    if (o.braid > 0.0) removed = lab::braid(maze, o.braid, rng);
    std::vector<int> weights;
    if (o.weights) weights = lab::random_weights(maze, rng);
    const std::vector<int>* w = o.weights ? &weights : nullptr;

    lab::Solution sol;
    if (o.solve) sol = lab::solve(maze, o.solver, w);

    if (o.draw) {
        lab::TextOptions t;
        t.unicode = !o.ascii;
        t.path = o.solve ? &sol.path : nullptr;
        t.weights = w;
        std::cout << lab::render_text(maze, t);
    }

    std::cout << "gerador: " << lab::generator_name(o.generator) << ", " << o.width << "x" << o.height << ", semente "
              << seed;
    if (o.braid > 0.0) std::cout << ", " << removed << " becos removidos";
    std::cout << "\n";
    if (o.solve) {
        if (sol.path.empty()) {
            std::cout << "solução (" << lab::solver_name(o.solver) << "): nenhum caminho encontrado\n";
        } else {
            std::cout << "solução (" << lab::solver_name(o.solver) << "): " << sol.path.size() - 1 << " passos";
            if (o.weights) std::cout << ", custo " << sol.cost;
            if (o.solver == lab::Solver::RightHand) std::cout << ", " << sol.walked << " passos andados seguindo a parede";
            std::cout << ", " << sol.explored_count << " de " << maze.cells() << " células examinadas\n";
        }
    }

    if (o.stats) {
        const lab::Stats s = lab::compute_stats(maze);
        std::cout << "células: " << s.cells << "\n";
        std::cout << "passagens: " << s.passages;
        if (s.perfect) {
            std::cout << " (labirinto perfeito: um único caminho entre quaisquer duas células)\n";
        } else {
            std::cout << " (" << s.cycles << " a mais que uma árvore: há ciclos)\n";
        }
        std::cout << "becos sem saída: " << s.dead_ends << " (" << percent(s.dead_ends, s.cells) << ")\n";
        std::cout << "bifurcações: " << s.junctions << "\n";
        std::cout << "corredores: " << s.corridors << "\n";
        std::cout << "maior caminho: " << s.longest << " passos, de " << coord(maze, s.longest_from) << " a "
                  << coord(maze, s.longest_to) << "\n";
    }

    if (!o.png.empty()) {
        lab::DrawOptions d;
        d.cell = o.cell;
        d.path = o.solve ? &sol.path : nullptr;
        d.explored = o.solve ? &sol.explored : nullptr;
        d.weights = w;
        const lab::Image img = lab::draw_maze(maze, d);
        if (!lab::write_file(o.png, lab::encode_png(img, lab::palette()))) {
            throw std::runtime_error("não foi possível gravar " + o.png);
        }
        std::cout << "imagem: " << o.png << " (" << img.width << "x" << img.height << " pixels)\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const UsageError& e) {
        std::cerr << "labirinto: " << e.what() << "\nUse labirinto --ajuda para ver as opções.\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "labirinto: " << e.what() << "\n";
        return 1;
    }
}
