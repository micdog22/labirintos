// Geradores: labirintos perfeitos, determinismo pela semente, trançado e viés.
#include <map>
#include <set>

#include "apoio.hpp"
#include "stats.hpp"
#include "teste.h"

using lab::Generator;

TESTE(todos_os_geradores_fazem_labirintos_perfeitos) {
    for (const auto& info : lab::generator_list()) {
        for (const auto& [w, h] : tamanhos()) {
            for (std::uint64_t seed = 1; seed <= 5; ++seed) {
                const lab::Maze m = gerar(info.id, w, h, seed);
                const lab::Stats s = lab::compute_stats(m);
                // Conexo e com exatamente células - 1 passagens: é uma árvore (sem ciclos).
                CHECK(s.connected);
                CHECK_EQ(m.passages(), static_cast<std::size_t>(m.cells() - 1));
                CHECK(s.perfect);
                CHECK(paredes_consistentes(m));
                if (!s.perfect) std::printf("    %s %dx%d semente %llu\n", info.name, w, h, static_cast<unsigned long long>(seed));
            }
        }
    }
}

TESTE(mesma_semente_mesmo_labirinto) {
    for (const auto& info : lab::generator_list()) {
        const lab::Maze a = gerar(info.id, 15, 9, 2026);
        const lab::Maze b = gerar(info.id, 15, 9, 2026);
        const lab::Maze c = gerar(info.id, 15, 9, 2027);
        CHECK_EQ(a.serialize(), b.serialize());
        CHECK(a.serialize() != c.serialize());
    }
}

TESTE(passos_da_geracao) {
    // Geradores que abrem paredes dão células - 1 passos; a divisão recursiva
    // levanta todas as paredes internas que não viram passagem.
    for (const auto& info : lab::generator_list()) {
        lab::Maze m(12, 8);
        lab::Rng rng(5);
        long passos = 0;
        lab::generate(m, info.id, rng, [&](const lab::Maze&, int) { ++passos; });
        const long internas = (12 - 1) * 8 + 12 * (8 - 1);
        const long esperado = info.id == Generator::Division ? internas - (m.cells() - 1) : m.cells() - 1;
        CHECK_EQ(passos, esperado);
    }
}

TESTE(trancar_remove_becos_e_cria_ciclos) {
    for (const auto& info : lab::generator_list()) {
        lab::Maze m = gerar(info.id, 16, 12, 9);
        const int becos = lab::compute_stats(m).dead_ends;
        lab::Rng rng(10);
        const int removidos = lab::braid(m, 1.0, rng);
        const lab::Stats s = lab::compute_stats(m);
        CHECK_EQ(s.dead_ends, 0);
        CHECK(removidos > 0 && removidos <= becos);
        CHECK(s.connected);
        CHECK(s.cycles > 0);
        CHECK_EQ(static_cast<long long>(m.passages()), static_cast<long long>(m.cells() - 1 + removidos));
        CHECK(paredes_consistentes(m));
    }
    // Fração zero não muda nada; num corredor 1xN as pontas não têm para onde abrir.
    lab::Maze m = gerar(Generator::Prim, 10, 10, 3);
    const std::string antes = m.serialize();
    lab::Rng rng(1);
    CHECK_EQ(lab::braid(m, 0.0, rng), 0);
    CHECK_EQ(m.serialize(), antes);
    lab::Maze corredor = gerar(Generator::Backtracker, 1, 9, 3);
    CHECK_EQ(lab::braid(corredor, 1.0, rng), 0);
    CHECK_EQ(lab::compute_stats(corredor).dead_ends, 2);
}

TESTE(trancar_pela_metade) {
    lab::Maze m = gerar(Generator::Kruskal, 40, 40, 11);
    const int becos = lab::compute_stats(m).dead_ends;
    lab::Rng rng(12);
    lab::braid(m, 0.5, rng);
    const int restantes = lab::compute_stats(m).dead_ends;
    CHECK(restantes > becos / 5);
    CHECK(restantes < becos * 4 / 5);
}

namespace {

// Identifica uma árvore geradora da grade 3x3 pelas 12 paredes internas.
int codigo_3x3(const lab::Maze& m) {
    int code = 0;
    int bit = 0;
    for (int c = 0; c < 9; ++c) {
        if (m.neighbor(c, lab::E) >= 0) code |= (m.is_open(c, lab::E) ? 1 : 0) << bit++;
        if (m.neighbor(c, lab::S) >= 0) code |= (m.is_open(c, lab::S) ? 1 : 0) << bit++;
    }
    return code;
}

// Qui-quadrado da frequência de cada uma das 192 árvores geradoras da grade 3x3
// contra a distribuição uniforme.
double qui_quadrado_3x3(Generator g, int amostras, std::uint64_t seed, std::size_t& distintas) {
    std::map<int, int> contagem;
    lab::Rng rng(seed);
    for (int i = 0; i < amostras; ++i) {
        lab::Maze m(3, 3);
        lab::generate(m, g, rng);
        ++contagem[codigo_3x3(m)];
    }
    distintas = contagem.size();
    const double esperado = static_cast<double>(amostras) / 192.0;
    double chi = 0;
    for (const auto& kv : contagem) chi += (kv.second - esperado) * (kv.second - esperado) / esperado;
    chi += static_cast<double>(192 - contagem.size()) * esperado;  // árvores que nunca apareceram
    return chi;
}

}  // namespace

TESTE(wilson_e_aldous_broder_nao_tem_vies) {
    // A grade 3x3 tem exatamente 192 árvores geradoras. Um gerador sem viés
    // sorteia cada uma com chance 1/192. Com 191 graus de liberdade, o
    // qui-quadrado fica abaixo de 260 com probabilidade maior que 99,9%.
    for (Generator g : {Generator::Wilson, Generator::AldousBroder}) {
        std::size_t distintas = 0;
        const double chi = qui_quadrado_3x3(g, 192 * 100, 77, distintas);
        CHECK_EQ(distintas, 192u);
        CHECK(chi < 260.0);
        if (chi >= 260.0) std::printf("    %s: qui-quadrado %.1f\n", lab::generator_name(g), chi);
    }
}

TESTE(backtracker_tem_vies) {
    // A busca em profundidade prefere corredores longos: algumas árvores saem
    // muito mais que outras e o qui-quadrado explode.
    std::size_t distintas = 0;
    const double chi = qui_quadrado_3x3(Generator::Backtracker, 192 * 100, 77, distintas);
    CHECK(chi > 2000.0);
}

TESTE(vies_visivel_nos_becos) {
    // Viés clássico: o backtracker faz poucos becos; Prim e Kruskal, muitos.
    const lab::Stats bt = lab::compute_stats(gerar(Generator::Backtracker, 50, 50, 21));
    const lab::Stats pr = lab::compute_stats(gerar(Generator::Prim, 50, 50, 21));
    const lab::Stats kr = lab::compute_stats(gerar(Generator::Kruskal, 50, 50, 21));
    CHECK(bt.dead_ends * 2 < pr.dead_ends);
    CHECK(bt.dead_ends * 2 < kr.dead_ends);
    // E o maior caminho do backtracker é bem mais longo.
    CHECK(bt.longest > 2 * pr.longest);
}

TESTE(pesos_entre_1_e_9) {
    lab::Maze m(30, 30);
    lab::Rng rng(4);
    const std::vector<int> w = lab::random_weights(m, rng);
    std::set<int> vistos(w.begin(), w.end());
    CHECK_EQ(vistos.size(), 9u);
    CHECK_EQ(*vistos.begin(), 1);
    CHECK_EQ(*vistos.rbegin(), 9);
}

TESTE(tamanho_invalido) {
    int erros = 0;
    for (auto [w, h] : {std::pair<int, int>{0, 5}, {5, 0}, {-1, 3}, {lab::Maze::kMaxSide + 1, 2}}) {
        try {
            lab::Maze m(w, h);
        } catch (const std::invalid_argument&) {
            ++erros;
        }
    }
    CHECK_EQ(erros, 4);
}
