#!/bin/sh

# ==============================================================================
# COMPILADOR E INSTALADOR DO FIX-NAMES PARA ARCH LINUX
# ==============================================================================
#
# Este script deve ser executado por um usuário comum. Ele usa sudo somente
# para instalar dependências e copiar os arquivos finais para /usr/local.
# A compilação e os testes nunca são executados como root.
#
# Etapas:
#   1. valida sistema, projeto e privilégios;
#   2. instala compilador, ncurses, GTK 4, Qt 6 e ferramentas de teste;
#   3. compila todos os binários com avisos rigorosos;
#   4. executa testes do núcleo e inicialização real das duas GUIs em Xvfb;
#   5. instala binários, lançador, ícone, menu e página de manual.
# ==============================================================================

set -eu

DIRETORIO_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRETORIO_PROJETO=$(CDPATH= cd -- "$DIRETORIO_SCRIPT/.." && pwd)

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

[ "$(id -u)" -ne 0 ] ||
    erro 'não execute este instalador como root; use um usuário comum com sudo.'
command -v pacman >/dev/null 2>&1 ||
    erro 'pacman não foi encontrado; este instalador é específico para Arch Linux.'
command -v sudo >/dev/null 2>&1 ||
    erro 'sudo não está instalado ou não está disponível no PATH.'
[ -f "$DIRETORIO_PROJETO/Makefile" ] ||
    erro "Makefile não encontrado em $DIRETORIO_PROJETO"

printf '%s\n' '[1/6] Validando a autorização administrativa...'
sudo -v

printf '%s\n' '[2/6] Instalando somente as dependências necessárias...'
# --needed impede reinstalações desnecessárias. Nenhuma opção de confirmação é
# desativada: o pacman continua mostrando a transação antes de aplicá-la.
sudo pacman -S --needed \
    base-devel \
    pkgconf \
    ncurses \
    gtk4 \
    qt6-base \
    desktop-file-utils \
    xorg-server-xvfb

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')
case $JOBS in
    ''|*[!0-9]*) JOBS=1 ;;
esac

printf '%s\n' '[3/6] Limpando uma compilação anterior e gerando os binários...'
(
    cd "$DIRETORIO_PROJETO"
    make clean
    make -j "$JOBS" all
)

printf '%s\n' '[4/6] Executando os testes automatizados do núcleo...'
(
    cd "$DIRETORIO_PROJETO"
    make test
)

printf '%s\n' '[5/6] Validando a inicialização das interfaces GTK e Qt...'
# O Arch fornece Xvfb, mas não o auxiliar xvfb-run do Debian. -displayfd pede
# ao próprio servidor que escolha uma tela livre, evitando colisões com :99.
ARQUIVO_DISPLAY=$(mktemp "${TMPDIR:-/tmp}/fix-names-display.XXXXXX") ||
    erro 'não foi possível criar o arquivo temporário do teste gráfico.'
LOG_XVFB=$(mktemp "${TMPDIR:-/tmp}/fix-names-xvfb.XXXXXX") ||
    erro 'não foi possível criar o log temporário do teste gráfico.'
Xvfb -displayfd 5 -screen 0 1024x768x24 -ac \
    5>"$ARQUIVO_DISPLAY" >"$LOG_XVFB" 2>&1 &
PID_XVFB=$!
limpar_teste_grafico()
{
    kill "$PID_XVFB" 2>/dev/null || :
    wait "$PID_XVFB" 2>/dev/null || :
    rm -f -- "$ARQUIVO_DISPLAY" "$LOG_XVFB"
}
trap limpar_teste_grafico 0
TENTATIVAS=0
while [ ! -s "$ARQUIVO_DISPLAY" ] && kill -0 "$PID_XVFB" 2>/dev/null; do
    TENTATIVAS=$((TENTATIVAS + 1))
    [ "$TENTATIVAS" -lt 10 ] || break
    sleep 1
done
[ -s "$ARQUIVO_DISPLAY" ] || {
    sed -n '1,80p' "$LOG_XVFB" >&2
    erro 'Xvfb não iniciou corretamente.'
}
NUMERO_DISPLAY=$(sed -n '1p' "$ARQUIVO_DISPLAY")
STATUS_GUI=0
(
    cd "$DIRETORIO_PROJETO"
    DISPLAY=":$NUMERO_DISPLAY" ./build/fix-names-gtk --self-test
) || STATUS_GUI=$?
if [ "$STATUS_GUI" -eq 0 ]; then
    (
        cd "$DIRETORIO_PROJETO"
        DISPLAY=":$NUMERO_DISPLAY" QT_QPA_PLATFORM=xcb \
            ./build/fix-names-qt --self-test
    ) || STATUS_GUI=$?
fi
limpar_teste_grafico
trap - 0
[ "$STATUS_GUI" -eq 0 ] || erro 'uma das interfaces gráficas falhou no teste.'

printf '%s\n' '[6/6] Instalando o fix-names em /usr/local...'
(
    cd "$DIRETORIO_PROJETO"
    sudo make PREFIX=/usr/local install
)

if command -v update-desktop-database >/dev/null 2>&1; then
    sudo update-desktop-database /usr/local/share/applications
fi

printf '\n%s\n' 'Instalação concluída com sucesso.'
printf '%s\n' 'CLI/ncurses: fix-names'
printf '%s\n' 'GUI automática: fix-names-gui'
printf '%s\n' 'GUI GTK:       fix-names-gtk'
printf '%s\n' 'GUI Qt:        fix-names-qt'
printf 'Binários gerados também em: %s/build\n' "$DIRETORIO_PROJETO"
