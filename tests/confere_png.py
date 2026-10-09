"""Confere PNGs gerados pelo labirinto usando o zlib do Python como referência.

Uso: python3 tests/confere_png.py arquivo.png [mais arquivos...]
Verifica a assinatura, o CRC de cada bloco, o cabeçalho IHDR, a paleta, o fluxo
zlib (descompactado pelo próprio zlib do Python) e o tamanho das linhas.
"""
import struct
import sys
import zlib


def confere(caminho):
    dados = open(caminho, "rb").read()
    if dados[:8] != b"\x89PNG\r\n\x1a\n":
        return "assinatura inválida"
    pos = 8
    blocos = []
    while pos < len(dados):
        (tamanho,) = struct.unpack(">I", dados[pos:pos + 4])
        tipo = dados[pos + 4:pos + 8]
        corpo = dados[pos + 8:pos + 8 + tamanho]
        (crc,) = struct.unpack(">I", dados[pos + 8 + tamanho:pos + 12 + tamanho])
        if zlib.crc32(tipo + corpo) & 0xFFFFFFFF != crc:
            return "CRC errado no bloco " + tipo.decode("ascii", "replace")
        blocos.append((tipo, corpo))
        pos += 12 + tamanho
    tipos = [t for t, _ in blocos]
    if tipos[0] != b"IHDR" or tipos[-1] != b"IEND" or b"IDAT" not in tipos:
        return "ordem de blocos inválida: %r" % tipos
    largura, altura, bits, cor, _, _, _ = struct.unpack(">IIBBBBB", blocos[0][1])
    if bits != 8 or cor != 3:
        return "esperava 8 bits com paleta"
    paleta = next(c for t, c in blocos if t == b"PLTE")
    if len(paleta) % 3 != 0:
        return "paleta com tamanho inválido"
    cores = len(paleta) // 3
    bruto = zlib.decompress(b"".join(c for t, c in blocos if t == b"IDAT"))
    if len(bruto) != altura * (largura + 1):
        return "tamanho dos dados não bate com %dx%d" % (largura, altura)
    for y in range(altura):
        linha = bruto[y * (largura + 1):(y + 1) * (largura + 1)]
        if linha[0] != 0:
            return "linha %d com filtro %d" % (y, linha[0])
        if max(linha[1:]) >= cores:
            return "índice de cor fora da paleta na linha %d" % y
    return None


def main(arquivos):
    falhas = 0
    for caminho in arquivos:
        erro = confere(caminho)
        if erro:
            falhas += 1
            print("%s: %s" % (caminho, erro))
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
