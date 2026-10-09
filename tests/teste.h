// Mini framework de testes: TESTE(nome), CHECK(cond) e CHECK_EQ(obtido, esperado).
#pragma once

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace teste {

struct Caso {
    const char* nome;
    void (*funcao)();
};

inline std::vector<Caso>& casos() {
    static std::vector<Caso> lista;
    return lista;
}

inline int& falhas() {
    static int n = 0;
    return n;
}

inline int& verificacoes() {
    static int n = 0;
    return n;
}

struct Registro {
    Registro(const char* nome, void (*funcao)()) { casos().push_back({nome, funcao}); }
};

template <class T>
std::string mostrar(const T& valor) {
    std::ostringstream o;
    o << valor;
    return o.str();
}

inline std::string mostrar(const std::string& s) { return "\"" + s + "\""; }
inline std::string mostrar(const char* s) { return "\"" + std::string(s) + "\""; }
inline std::string mostrar(bool b) { return b ? "true" : "false"; }

inline void verificar(bool ok, const char* texto, const char* arquivo, int linha) {
    ++verificacoes();
    if (!ok) {
        ++falhas();
        std::printf("  FALHOU %s:%d: %s\n", arquivo, linha, texto);
    }
}

template <class A, class B>
void comparar(const A& obtido, const B& esperado, const char* ta, const char* tb, const char* arquivo, int linha) {
    ++verificacoes();
    if (!(obtido == esperado)) {
        ++falhas();
        std::printf("  FALHOU %s:%d: %s == %s\n    obtido:   %s\n    esperado: %s\n", arquivo, linha, ta, tb,
                    mostrar(obtido).c_str(), mostrar(esperado).c_str());
    }
}

}  // namespace teste

#define TESTE(nome)                                                      \
    static void nome();                                                  \
    static const ::teste::Registro registro_##nome(#nome, nome);         \
    static void nome()

#define CHECK(cond) ::teste::verificar((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(obtido, esperado) ::teste::comparar((obtido), (esperado), #obtido, #esperado, __FILE__, __LINE__)
