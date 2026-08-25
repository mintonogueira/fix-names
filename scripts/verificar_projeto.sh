#!/bin/sh

# ==============================================================================
# VERIFICAÇÃO DA ESTRUTURA COMPLETA DO PROJETO FIX-NAMES
# ==============================================================================
#
# Este script não compila nem altera nada. Sua função é detectar imediatamente
# uma extração incompleta ou um arquivo essencial vazio antes que o instalador
# baixe dependências. Também valida:
#
#   - sintaxe dos scripts POSIX incluídos;
#   - presença dos marcadores dos modelos Debian e Arch;
#   - versão numérica declarada pelo núcleo;
#   - assinatura binária PNG do ícone oficial.
#
# Ele pode ser executado manualmente com:
#
#   sh scripts/verificar_projeto.sh
# ==============================================================================

set -eu

DIRETORIO_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRETORIO_PROJETO=$(CDPATH= cd -- "$DIRETORIO_SCRIPT/.." && pwd)

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

# A lista abaixo funciona como um manifesto mínimo obrigatório. Cada caminho é
# relativo à raiz extraída, não contém espaços e deve apontar para arquivo
# regular com pelo menos um byte.
ARQUIVOS_OBRIGATORIOS='
Makefile
README.md
instalar.sh
SHA256SUMS
assets/fix-names.png
data/fix-names-gui
data/fix-names.1
data/fix-names.desktop
packaging/arch/PKGBUILD.in
packaging/debian/control.in
scripts/compilar_instalar_arch.sh
scripts/compilar_instalar_debian.sh
scripts/verificar_projeto.sh
src/core.cpp
src/core.hpp
src/cli_main.cpp
src/ncurses_ui.cpp
src/ncurses_ui.hpp
src/gtk_main.cpp
src/qt_main.cpp
tests/test_core.cpp
'

TOTAL=0
for CAMINHO_RELATIVO in $ARQUIVOS_OBRIGATORIOS; do
    CAMINHO_COMPLETO=$DIRETORIO_PROJETO/$CAMINHO_RELATIVO
    [ -f "$CAMINHO_COMPLETO" ] ||
        erro "arquivo obrigatório ausente: $CAMINHO_RELATIVO"
    [ -s "$CAMINHO_COMPLETO" ] ||
        erro "arquivo obrigatório vazio: $CAMINHO_RELATIVO"
    TOTAL=$((TOTAL + 1))
done

# SHA256SUMS é criado somente depois que a entrega final está pronta. A opção
# -c recalcula cada arquivo listado e detecta truncamentos ou alterações. O
# próprio manifesto fica fora da lista para evitar uma dependência circular.
command -v sha256sum >/dev/null 2>&1 ||
    erro 'sha256sum não foi encontrado; instale coreutils.'
(
    cd "$DIRETORIO_PROJETO"
    sha256sum -c SHA256SUMS
) >/dev/null ||
    erro 'a verificação SHA-256 falhou; o pacote está corrompido ou foi alterado.'

# A verificação de sintaxe usa /bin/sh para garantir que os instaladores não
# dependam de recursos exclusivos do Bash. O seletor de GUI também é Shell
# POSIX e, por isso, faz parte desta conferência.
for SCRIPT_POSIX in \
    instalar.sh \
    data/fix-names-gui \
    scripts/compilar_instalar_arch.sh \
    scripts/compilar_instalar_debian.sh \
    scripts/verificar_projeto.sh
do
    sh -n "$DIRETORIO_PROJETO/$SCRIPT_POSIX" ||
        erro "sintaxe Shell inválida: $SCRIPT_POSIX"
done

# O número é lido da única constante de versão do núcleo. Os empacotadores
# usam a mesma origem, impedindo divergência entre executável, .deb e pacote do
# pacman.
VERSAO=$(sed -n 's/.*VERSION = "\([^"]*\)".*/\1/p' \
    "$DIRETORIO_PROJETO/src/core.hpp")
case $VERSAO in
    ''|*[!0-9.]*) erro 'versão ausente ou inválida em src/core.hpp.' ;;
esac

# Os modelos precisam manter seus marcadores até o momento do empacotamento.
# Se um modelo truncado ou já parcialmente processado for distribuído, o script
# correspondente não conseguiria produzir metadados coerentes.
grep -q '@VERSION@' "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ||
    erro 'marcador @VERSION@ ausente do PKGBUILD.in.'
grep -q '@SOURCE_SHA256@' "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ||
    erro 'marcador @SOURCE_SHA256@ ausente do PKGBUILD.in.'
grep -q '@VERSION@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
    erro 'marcador @VERSION@ ausente do control.in.'
grep -q '@DEPENDS@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
    erro 'marcador @DEPENDS@ ausente do control.in.'

# Um arquivo chamado .png pode ter sido truncado durante a cópia. Os oito
# primeiros bytes de todo PNG válido são fixos. dd, od e tr pertencem ao
# conjunto básico das duas distribuições suportadas.
ASSINATURA_PNG=$(dd if="$DIRETORIO_PROJETO/assets/fix-names.png" \
    bs=8 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
[ "$ASSINATURA_PNG" = '89504e470d0a1a0a' ] ||
    erro 'assets/fix-names.png não possui uma assinatura PNG válida.'

printf 'Estrutura completa verificada: %d arquivos obrigatórios; versão %s.\n' \
    "$TOTAL" "$VERSAO"
