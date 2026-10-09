#include <cstdio>

#include "teste.h"

int main() {
    // Saída linha a linha: se um teste derrubar o processo, as mensagens anteriores não se perdem.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::size_t com_falha = 0;
    for (const teste::Caso& caso : teste::casos()) {
        const int antes = teste::falhas();
        caso.funcao();
        if (teste::falhas() != antes) {
            ++com_falha;
            std::printf("teste %s: FALHOU\n", caso.nome);
        }
    }
    std::printf("unidade: %zu testes, %d verificações, %d falhas\n", teste::casos().size(), teste::verificacoes(),
                teste::falhas());
    return com_falha == 0 ? 0 : 1;
}
