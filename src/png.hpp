// Gravador de PNG feito à mão: assinatura, blocos IHDR, PLTE, IDAT e IEND,
// CRC-32 em cada bloco e os dados num fluxo zlib com blocos "stored" (sem
// compressão) e Adler-32 no final.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lab {

// CRC-32 (polinômio 0xEDB88320, o mesmo do PNG, do zip e do gzip).
std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t previous = 0);

// Adler-32 (o checksum do zlib): duas somas módulo 65521.
std::uint32_t adler32(const std::uint8_t* data, std::size_t size);

// Fluxo zlib: cabeçalho 78 01, blocos stored de até 65535 bytes, Adler-32.
std::vector<std::uint8_t> zlib_stored(const std::vector<std::uint8_t>& raw);

using Rgb = std::array<std::uint8_t, 3>;

// Imagem com paleta: cada pixel é um índice na paleta (até 256 cores).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // width * height índices, linha por linha

    Image() = default;
    Image(int w, int h, std::uint8_t fill = 0);
    void fill_rect(int x, int y, int w, int h, std::uint8_t color);
};

// PNG de 8 bits com paleta (tipo de cor 3).
std::vector<std::uint8_t> encode_png(const Image& image, const std::vector<Rgb>& palette);

// Grava bytes em um arquivo; devolve false se não conseguiu.
bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes);

}  // namespace lab
