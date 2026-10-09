#!/bin/sh
# Testes de integração da linha de comando: determinismo, todos os geradores e
# resolvedores, PNGs conferidos pelo sips (macOS) e pelo zlib do Python, quadros
# da geração e erros de uso.
# Uso: sh tests/integracao.sh caminho/para/labirinto
set -u

BIN=${1:?informe o executável}
case $BIN in
    /*) ;;
    *) BIN="$(pwd)/$BIN" ;;
esac
RAIZ=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/labirintos-testes.XXXXXX")
trap 'rm -rf "$TMP"' EXIT INT TERM

total=0
falhas=0
falhou() {
    falhas=$((falhas + 1))
    echo "FALHOU: $1"
}
conta() { total=$((total + 1)); }

# 1. Mesma semente, mesma saída (desenho, solução e estatísticas).
conta
"$BIN" -l 25 -a 12 -g wilson -s 99 --estatisticas >"$TMP/a.txt" 2>&1
"$BIN" -l 25 -a 12 -g wilson -s 99 --estatisticas >"$TMP/b.txt" 2>&1
cmp -s "$TMP/a.txt" "$TMP/b.txt" || falhou "a mesma semente deveria repetir a saída"
grep -q "labirinto perfeito" "$TMP/a.txt" || falhou "estatísticas sem a linha de labirinto perfeito"

# 2. Todos os geradores com todos os resolvedores.
for g in backtracker prim kruskal wilson eller aldous-broder divisao; do
    for r in bfs dfs astar dijkstra becos mao-direita; do
        conta
        if ! "$BIN" -l 9 -a 6 -g "$g" -r "$r" -s 5 --trancar 0.3 --ascii >"$TMP/s.txt" 2>&1 ||
            ! grep -q "^solução ($r): [0-9]* passos" "$TMP/s.txt"; then
            falhou "$g com $r: $(tail -1 "$TMP/s.txt")"
        fi
    done
done

# 3. PNG com a solução: tamanho conferido pelo sips (se existir) e conteúdo pelo Python.
conta
"$BIN" -l 30 -a 20 -s 7 -g kruskal --trancar 0.5 -r astar --pesos --png "$TMP/lab.png" --celula 10 --sem-desenho >/dev/null 2>&1 ||
    falhou "não gerou o PNG"
if command -v sips >/dev/null 2>&1; then
    conta
    largura=$(sips -g pixelWidth "$TMP/lab.png" 2>/dev/null | awk '/pixelWidth/ {print $2}')
    altura=$(sips -g pixelHeight "$TMP/lab.png" 2>/dev/null | awk '/pixelHeight/ {print $2}')
    # 30 colunas de (10 + 2) pixels mais a parede final: 362; 20 linhas: 242.
    [ "$largura" = 362 ] && [ "$altura" = 242 ] || falhou "sips leu ${largura}x${altura}, esperava 362x242"
else
    echo "aviso: sips não encontrado, pulando a conferência pelo sips"
fi
if command -v python3 >/dev/null 2>&1; then
    conta
    python3 "$RAIZ/tests/confere_png.py" "$TMP/lab.png" || falhou "PNG reprovado pelo zlib do Python"
else
    echo "aviso: python3 não encontrado, pulando a conferência pelo zlib"
fi

# 4. Quadros da geração: 12x8 = 96 células; Prim abre 95 passagens; a cada 10
#    passos dá 9 quadros, mais o quadro final.
conta
"$BIN" -l 12 -a 8 -g prim -s 3 --quadros "$TMP/quadros" --a-cada 10 --celula 6 -r nenhum --sem-desenho >/dev/null 2>&1 ||
    falhou "não gerou os quadros"
quadros=$(ls "$TMP/quadros" | grep -c '^quadro-[0-9]*\.png$')
[ "$quadros" = 10 ] || falhou "esperava 10 quadros, gerou $quadros"
if command -v python3 >/dev/null 2>&1; then
    conta
    python3 "$RAIZ/tests/confere_png.py" "$TMP"/quadros/*.png || falhou "quadro reprovado pelo zlib do Python"
fi

# 5. Erros de uso saem com código 2; --listar mostra tudo.
conta
for args in "--gerador nada" "--largura 0" "--trancar 2" "--celula 1" "--opcao-inexistente" "--semente"; do
    # shellcheck disable=SC2086
    "$BIN" $args >/dev/null 2>&1
    codigo=$?
    [ "$codigo" = 2 ] || falhou "'$args' deveria sair com código 2, saiu com $codigo"
done
conta
n=$("$BIN" --listar | grep -c '^  [a-z]')
[ "$n" = 13 ] || falhou "--listar mostrou $n itens, esperava 13"

echo "integração: $((total - falhas)) de $total verificações passaram"
[ "$falhas" -eq 0 ]
