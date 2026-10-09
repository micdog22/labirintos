// PNG: CRC-32 e Adler-32 com valores conhecidos, fluxo zlib com blocos stored
// e leitura de volta do arquivo gerado.
#include <cstring>
#include <string>
#include <vector>

#include "apoio.hpp"
#include "png.hpp"
#include "raster.hpp"
#include "teste.h"

namespace {

std::vector<std::uint8_t> bytes(const std::string& s) { return std::vector<std::uint8_t>(s.begin(), s.end()); }

std::uint32_t u32_be(const std::vector<std::uint8_t>& b, std::size_t at) {
    return (static_cast<std::uint32_t>(b[at]) << 24) | (static_cast<std::uint32_t>(b[at + 1]) << 16) |
           (static_cast<std::uint32_t>(b[at + 2]) << 8) | b[at + 3];
}

// Lê um fluxo zlib feito só de blocos stored. Devolve false se algo não bater.
bool inflate_stored(const std::vector<std::uint8_t>& z, std::vector<std::uint8_t>& out, int& blocks) {
    out.clear();
    blocks = 0;
    if (z.size() < 6 || z[0] != 0x78 || ((z[0] << 8) | z[1]) % 31 != 0) return false;
    std::size_t pos = 2;
    bool final = false;
    while (!final) {
        if (pos + 5 > z.size()) return false;
        final = (z[pos] & 1) != 0;
        if ((z[pos] >> 1 & 3) != 0) return false;  // BTYPE precisa ser 00
        const std::size_t len = z[pos + 1] | (z[pos + 2] << 8);
        const std::size_t nlen = z[pos + 3] | (z[pos + 4] << 8);
        if ((len ^ 0xFFFF) != nlen) return false;
        pos += 5;
        if (pos + len > z.size()) return false;
        out.insert(out.end(), z.begin() + static_cast<long>(pos), z.begin() + static_cast<long>(pos + len));
        pos += len;
        ++blocks;
    }
    if (pos + 4 != z.size()) return false;
    return u32_be(z, pos) == lab::adler32(out.data(), out.size());
}

// Adler-32 direto da definição, sem a otimização dos blocos de 5552 bytes.
std::uint32_t adler_ingenuo(const std::vector<std::uint8_t>& d) {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t x : d) {
        a = (a + x) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

}  // namespace

TESTE(crc32_valores_conhecidos) {
    const auto v = bytes("123456789");
    CHECK_EQ(lab::crc32(v.data(), v.size()), 0xCBF43926u);  // valor de verificação do CRC-32
    CHECK_EQ(lab::crc32(nullptr, 0), 0u);
    const auto iend = bytes("IEND");
    CHECK_EQ(lab::crc32(iend.data(), iend.size()), 0xAE426082u);  // CRC do bloco IEND de todo PNG
    // Em partes dá o mesmo que de uma vez.
    const std::uint32_t parte = lab::crc32(v.data(), 4);
    CHECK_EQ(lab::crc32(v.data() + 4, v.size() - 4, parte), 0xCBF43926u);
}

TESTE(adler32_valores_conhecidos) {
    const auto w = bytes("Wikipedia");
    CHECK_EQ(lab::adler32(w.data(), w.size()), 0x11E60398u);
    CHECK_EQ(lab::adler32(nullptr, 0), 1u);
    // Um megabyte de 0xFF força várias reduções módulo 65521.
    std::vector<std::uint8_t> grande(1 << 20, 0xFF);
    CHECK_EQ(lab::adler32(grande.data(), grande.size()), adler_ingenuo(grande));
    lab::Rng rng(3);
    std::vector<std::uint8_t> aleatorio(100003);
    for (auto& x : aleatorio) x = static_cast<std::uint8_t>(rng.below(256));
    CHECK_EQ(lab::adler32(aleatorio.data(), aleatorio.size()), adler_ingenuo(aleatorio));
}

TESTE(zlib_com_blocos_stored) {
    lab::Rng rng(4);
    for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{65535}, std::size_t{65536}, std::size_t{200000}}) {
        std::vector<std::uint8_t> raw(n);
        for (auto& x : raw) x = static_cast<std::uint8_t>(rng.below(256));
        const auto z = lab::zlib_stored(raw);
        CHECK_EQ(z[0], 0x78);
        CHECK_EQ(z[1], 0x01);
        std::vector<std::uint8_t> back;
        int blocks = 0;
        CHECK(inflate_stored(z, back, blocks));
        CHECK(back == raw);
        CHECK_EQ(blocks, n == 0 ? 1 : static_cast<int>((n + 65534) / 65535));
        CHECK_EQ(z.size(), 2 + 5 * static_cast<std::size_t>(blocks) + n + 4);
    }
}

TESTE(png_lido_de_volta) {
    lab::Image img(7, 3, 0);
    img.fill_rect(2, 1, 3, 1, 4);
    const std::vector<lab::Rgb> pal = {{255, 255, 255}, {0, 0, 0}, {1, 2, 3}, {4, 5, 6}, {200, 100, 50}};
    const auto png = lab::encode_png(img, pal);
    const std::uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    CHECK(std::memcmp(png.data(), sig, 8) == 0);
    std::size_t pos = 8;
    std::vector<std::string> tipos;
    std::vector<std::uint8_t> idat;
    while (pos + 12 <= png.size()) {
        const std::uint32_t len = u32_be(png, pos);
        const std::string tipo(png.begin() + static_cast<long>(pos + 4), png.begin() + static_cast<long>(pos + 8));
        const std::uint32_t crc = u32_be(png, pos + 8 + len);
        CHECK_EQ(lab::crc32(png.data() + pos + 4, len + 4), crc);
        if (tipo == "IHDR") {
            CHECK_EQ(len, 13u);
            CHECK_EQ(u32_be(png, pos + 8), 7u);
            CHECK_EQ(u32_be(png, pos + 12), 3u);
            CHECK_EQ(png[pos + 16], 8);  // bits por amostra
            CHECK_EQ(png[pos + 17], 3);  // paleta
        }
        if (tipo == "PLTE") CHECK_EQ(len, 15u);
        if (tipo == "IDAT") idat.insert(idat.end(), png.begin() + static_cast<long>(pos + 8), png.begin() + static_cast<long>(pos + 8 + len));
        tipos.push_back(tipo);
        pos += 12 + len;
    }
    CHECK_EQ(pos, png.size());
    CHECK((tipos == std::vector<std::string>{"IHDR", "PLTE", "IDAT", "IEND"}));
    std::vector<std::uint8_t> raw;
    int blocks = 0;
    CHECK(inflate_stored(idat, raw, blocks));
    CHECK_EQ(raw.size(), 3u * 8u);  // 3 linhas de (1 byte de filtro + 7 pixels)
    CHECK_EQ(raw[0], 0);
    CHECK_EQ(raw[8], 0);
    CHECK_EQ(raw[8 + 1 + 2], 4);  // linha 1, coluna 2
    CHECK_EQ(raw[8 + 1 + 5], 0);
}

TESTE(tamanho_da_imagem_do_labirinto) {
    const lab::Maze m = gerar(lab::Generator::Prim, 9, 4, 1);
    lab::DrawOptions d;
    d.cell = 10;
    d.wall = 2;
    const lab::Image img = lab::draw_maze(m, d);
    CHECK_EQ(img.width, 9 * 12 + 2);
    CHECK_EQ(img.height, 4 * 12 + 2);
    // Canto de cima à esquerda é parede; o meio da primeira célula é corredor.
    CHECK_EQ(img.pixels[0], lab::kWall);
    CHECK_EQ(img.pixels[static_cast<std::size_t>(5 * img.width + 5)], lab::kFloor);
    bool lancou = false;
    try {
        lab::Image grande(100000, 100000);
    } catch (const std::invalid_argument&) {
        lancou = true;
    }
    CHECK(lancou);
}
