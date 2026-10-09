#include "png.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace lab {

namespace {

// Tabela com o CRC de cada byte possível, calculada uma vez.
const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    return table;
}

void put_u32_be(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

// Bloco PNG: tamanho (4 bytes), tipo (4 letras), dados, CRC do tipo + dados.
void put_chunk(std::vector<std::uint8_t>& out, const char type[5], const std::vector<std::uint8_t>& data) {
    put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_u32_be(out, crc32(out.data() + start, out.size() - start));
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t previous) {
    const auto& table = crc_table();
    std::uint32_t c = previous ^ 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t size) {
    const std::uint32_t mod = 65521;  // maior primo menor que 2^16
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    // Até 5552 bytes dá para somar sem estourar 32 bits antes de tirar o módulo.
    while (size > 0) {
        const std::size_t block = size < 5552 ? size : 5552;
        for (std::size_t i = 0; i < block; ++i) {
            a += data[i];
            b += a;
        }
        a %= mod;
        b %= mod;
        data += block;
        size -= block;
    }
    return (b << 16) | a;
}

std::vector<std::uint8_t> zlib_stored(const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> out;
    out.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    // CMF = 0x78: método 8 (deflate), janela de 32 KiB. FLG = 0x01: sem
    // dicionário, nível 0, e (CMF * 256 + FLG) divisível por 31.
    out.push_back(0x78);
    out.push_back(0x01);
    std::size_t pos = 0;
    do {
        const std::size_t len = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
        const bool final = pos + len == raw.size();
        // Cabeçalho do bloco: bit BFINAL e BTYPE = 00 (stored); o resto do byte é preenchimento.
        out.push_back(final ? 0x01 : 0x00);
        out.push_back(static_cast<std::uint8_t>(len & 0xFF));  // LEN em little-endian
        out.push_back(static_cast<std::uint8_t>(len >> 8));
        out.push_back(static_cast<std::uint8_t>(~len & 0xFF));  // NLEN = complemento de LEN
        out.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFF));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                   raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    put_u32_be(out, adler32(raw.data(), raw.size()));  // Adler-32 em big-endian
    return out;
}

Image::Image(int w, int h, std::uint8_t fill) : width(w), height(h) {
    if (w <= 0 || h <= 0 || static_cast<long long>(w) * h > 64LL * 1024 * 1024) {
        throw std::invalid_argument("tamanho de imagem inválido ou grande demais");
    }
    pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill);
}

void Image::fill_rect(int x, int y, int w, int h, std::uint8_t color) {
    for (int yy = std::max(0, y); yy < std::min(height, y + h); ++yy) {
        for (int xx = std::max(0, x); xx < std::min(width, x + w); ++xx) {
            pixels[static_cast<std::size_t>(yy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(xx)] = color;
        }
    }
}

std::vector<std::uint8_t> encode_png(const Image& image, const std::vector<Rgb>& palette) {
    if (palette.empty() || palette.size() > 256) throw std::invalid_argument("a paleta precisa de 1 a 256 cores");
    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

    std::vector<std::uint8_t> ihdr;
    put_u32_be(ihdr, static_cast<std::uint32_t>(image.width));
    put_u32_be(ihdr, static_cast<std::uint32_t>(image.height));
    ihdr.push_back(8);  // bits por amostra
    ihdr.push_back(3);  // tipo de cor 3: paleta
    ihdr.push_back(0);  // compressão: deflate
    ihdr.push_back(0);  // filtro: método 0
    ihdr.push_back(0);  // sem entrelaçamento
    put_chunk(png, "IHDR", ihdr);

    std::vector<std::uint8_t> plte;
    for (const Rgb& c : palette) plte.insert(plte.end(), c.begin(), c.end());
    put_chunk(png, "PLTE", plte);

    // Cada linha começa com o byte do filtro (0 = nenhum) seguido dos pixels.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(image.width + 1) * static_cast<std::size_t>(image.height));
    for (int y = 0; y < image.height; ++y) {
        raw.push_back(0);
        const auto row = image.pixels.begin() + static_cast<std::ptrdiff_t>(y) * image.width;
        raw.insert(raw.end(), row, row + image.width);
    }
    put_chunk(png, "IDAT", zlib_stored(raw));
    put_chunk(png, "IEND", {});
    return png;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

}  // namespace lab
