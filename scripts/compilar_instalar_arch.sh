#!/bin/sh

# ==============================================================================
# GERADOR DO PACOTE .PKG.TAR.ZST E INSTALADOR DO FIX-NAMES PARA ARCH LINUX
# ==============================================================================
#
# O script é executado por um usuário comum. sudo aparece apenas na instalação
# das dependências e na transação final do pacman. makepkg, compilação, testes e
# interfaces nunca são executados como root.
#
# Resultado persistente:
#   pacotes/fix-names-VERSAO-1-ARQUITETURA.pkg.tar.zst
# ==============================================================================

set -eu

DIRETORIO_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRETORIO_PROJETO=$(CDPATH= cd -- "$DIRETORIO_SCRIPT/.." && pwd)
DIRETORIO_PACOTES=$DIRETORIO_PROJETO/pacotes
TOTAL_ETAPAS=8
DIRETORIO_TEMPORARIO=
ARQUIVO_DISPLAY=
LOG_XVFB=
PID_XVFB=

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

limpar_temporarios()
{
    case ${PID_XVFB-} in
        ''|*[!0-9]*) : ;;
        *)
            kill "$PID_XVFB" 2>/dev/null || :
            wait "$PID_XVFB" 2>/dev/null || :
            ;;
    esac
    case ${ARQUIVO_DISPLAY-} in
        "${TMPDIR:-/tmp}"/fix-names-display.*)
            rm -f -- "$ARQUIVO_DISPLAY"
            ;;
    esac
    case ${LOG_XVFB-} in
        "${TMPDIR:-/tmp}"/fix-names-xvfb.*)
            rm -f -- "$LOG_XVFB"
            ;;
    esac
    case ${DIRETORIO_TEMPORARIO-} in
        "${TMPDIR:-/tmp}"/fix-names-arch.*)
            [ ! -d "$DIRETORIO_TEMPORARIO" ] ||
                rm -rf -- "$DIRETORIO_TEMPORARIO"
            ;;
    esac
}
trap limpar_temporarios 0 1 2 15

mostrar_etapa()
{
    NUMERO_ETAPA=$1
    DESCRICAO_ETAPA=$2
    PERCENTUAL=$((NUMERO_ETAPA * 100 / TOTAL_ETAPAS))
    PREENCHIDAS=$((PERCENTUAL * 30 / 100))
    INDICE=0
    BARRA=
    while [ "$INDICE" -lt 30 ]; do
        if [ "$INDICE" -lt "$PREENCHIDAS" ]; then
            BARRA=${BARRA}#
        else
            BARRA=${BARRA}-
        fi
        INDICE=$((INDICE + 1))
    done
    printf '\n[%s] %3d%% — %s\n' "$BARRA" "$PERCENTUAL" "$DESCRICAO_ETAPA"
}

[ "$(id -u)" -ne 0 ] ||
    erro 'não execute este instalador como root; use um usuário comum com sudo.'
command -v pacman >/dev/null 2>&1 ||
    erro 'pacman não foi encontrado; este instalador é específico para Arch Linux.'
command -v makepkg >/dev/null 2>&1 ||
    erro 'makepkg não foi encontrado; instale o grupo base-devel.'
command -v sudo >/dev/null 2>&1 ||
    erro 'sudo não está instalado ou não está disponível no PATH.'

# Verifica toda a estrutura antes de instalar dependências. Além de detectar
# fontes ausentes, esta etapa valida os scripts POSIX, o modelo de pacote e a
# assinatura do ícone PNG incluído na distribuição.
[ -f "$DIRETORIO_SCRIPT/verificar_projeto.sh" ] ||
    erro 'scripts/verificar_projeto.sh não foi encontrado; pacote incompleto.'
sh "$DIRETORIO_SCRIPT/verificar_projeto.sh"

[ -f "$DIRETORIO_PROJETO/Makefile" ] ||
    erro "Makefile não encontrado em $DIRETORIO_PROJETO"
[ -f "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ] ||
    erro 'modelo packaging/arch/PKGBUILD.in não encontrado.'

VERSAO=$(sed -n 's/.*VERSION = "\([^"]*\)".*/\1/p' \
    "$DIRETORIO_PROJETO/src/core.hpp")
case $VERSAO in
    ''|*[!0-9.]*) erro 'não foi possível identificar uma versão numérica válida.' ;;
esac

mostrar_etapa 1 'Validando a autorização administrativa'
sudo -v

mostrar_etapa 2 'Instalando dependências de compilação e empacotamento'
# --needed evita reinstalar pacotes atuais. A confirmação da transação do
# pacman permanece ativa para o usuário conferir o que será instalado.
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

mostrar_etapa 3 'Compilando CLI, ncurses, GTK e Qt'
(
    cd "$DIRETORIO_PROJETO"
    make clean
    make -j "$JOBS" PREFIX=/usr all
)

mostrar_etapa 4 'Executando os testes automatizados do núcleo'
(
    cd "$DIRETORIO_PROJETO"
    make PREFIX=/usr test
)

mostrar_etapa 5 'Validando a abertura real das interfaces GTK e Qt'
ARQUIVO_DISPLAY=$(mktemp "${TMPDIR:-/tmp}/fix-names-display.XXXXXX") ||
    erro 'não foi possível criar o arquivo temporário do teste gráfico.'
LOG_XVFB=$(mktemp "${TMPDIR:-/tmp}/fix-names-xvfb.XXXXXX") ||
    erro 'não foi possível criar o log temporário do teste gráfico.'
Xvfb -displayfd 5 -screen 0 1024x768x24 -ac \
    5>"$ARQUIVO_DISPLAY" >"$LOG_XVFB" 2>&1 &
PID_XVFB=$!
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
# O backend X11 garante que o teste não tente reutilizar uma sessão Wayland do
# usuário. O limite de vinte segundos transforma qualquer travamento de abertura
# ou encerramento em falha clara, antes que um pacote defeituoso seja criado.
(
    cd "$DIRETORIO_PROJETO"
    DISPLAY=":$NUMERO_DISPLAY" GDK_BACKEND=x11 \
        timeout 20s ./build/fix-names-gtk --self-test
) || STATUS_GUI=$?
if [ "$STATUS_GUI" -eq 0 ]; then
    (
        cd "$DIRETORIO_PROJETO"
        DISPLAY=":$NUMERO_DISPLAY" QT_QPA_PLATFORM=xcb \
            timeout 20s ./build/fix-names-qt --self-test
    ) || STATUS_GUI=$?
fi
kill "$PID_XVFB" 2>/dev/null || :
wait "$PID_XVFB" 2>/dev/null || :
rm -f -- "$ARQUIVO_DISPLAY" "$LOG_XVFB"
PID_XVFB=
ARQUIVO_DISPLAY=
LOG_XVFB=
[ "$STATUS_GUI" -eq 0 ] || erro 'uma das interfaces gráficas falhou no teste.'

mostrar_etapa 6 'Preparando os fontes e o PKGBUILD verificado por SHA-256'
DIRETORIO_TEMPORARIO=$(mktemp -d "${TMPDIR:-/tmp}/fix-names-arch.XXXXXX") ||
    erro 'não foi possível criar o diretório temporário do makepkg.'
RAIZ_FONTE=$DIRETORIO_TEMPORARIO/fix-names-$VERSAO
ARQUIVO_FONTE=$DIRETORIO_TEMPORARIO/fix-names-$VERSAO.tar.gz
mkdir -p -- "$RAIZ_FONTE" "$DIRETORIO_PACOTES"

# A cópia lógica é feita por dois processos tar: somente fontes da entrega são
# transportados, enquanto build, pacotes anteriores e metadados Git ficam fora.
(
    cd "$DIRETORIO_PROJETO"
    tar --exclude='./build' --exclude='./pacotes' --exclude='./.git' \
        --exclude='./*.tar.gz' -cf - .
) | (
    cd "$RAIZ_FONTE"
    tar -xf -
)
tar -C "$DIRETORIO_TEMPORARIO" -czf "$ARQUIVO_FONTE" \
    "fix-names-$VERSAO"
SOMA_FONTE=$(sha256sum "$ARQUIVO_FONTE" | awk '{print $1}')
sed -e "s/@VERSION@/$VERSAO/g" \
    -e "s/@SOURCE_SHA256@/$SOMA_FONTE/g" \
    "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" \
    > "$DIRETORIO_TEMPORARIO/PKGBUILD"

mostrar_etapa 7 'Gerando e verificando o pacote binário do pacman'
PACOTE_ARCH=$(cd "$DIRETORIO_TEMPORARIO" && \
    PKGDEST="$DIRETORIO_PACOTES" makepkg --packagelist | sed -n '1p')
[ -n "$PACOTE_ARCH" ] || erro 'makepkg não informou o nome do pacote final.'
(
    cd "$DIRETORIO_TEMPORARIO"
    PKGDEST="$DIRETORIO_PACOTES" MAKEFLAGS="-j$JOBS" \
        makepkg --clean --cleanbuild --force
)
[ -f "$PACOTE_ARCH" ] || erro "o pacote esperado não foi criado: $PACOTE_ARCH"
tar -tf "$PACOTE_ARCH" >/dev/null

mostrar_etapa 8 'Instalando o pacote gerado com o pacman'
sudo pacman -U "$PACOTE_ARCH"

printf '\n%s\n' 'Instalação concluída com sucesso.'
printf 'Pacote Arch Linux gerado: %s\n' "$PACOTE_ARCH"
printf '%s\n' 'CLI/ncurses: fix-names'
printf '%s\n' 'GUI automática: fix-names-gui'
printf '%s\n' 'GUI GTK:       fix-names-gtk'
printf '%s\n' 'GUI Qt:        fix-names-qt'
