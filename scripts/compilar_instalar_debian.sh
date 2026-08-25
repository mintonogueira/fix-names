#!/bin/sh

# ==============================================================================
# GERADOR DO PACOTE .DEB E INSTALADOR DO FIX-NAMES PARA DEBIAN
# ==============================================================================
#
# Este script deve ser executado por um usuário comum. Ele usa sudo somente
# para instalar dependências e para instalar o pacote .deb final. Compilação,
# testes, montagem do pacote e validação das interfaces são feitos sem root,
# respeitando a proteção absoluta existente dentro do próprio fix-names.
#
# Resultado persistente:
#   pacotes/fix-names_VERSAO-1_ARQUITETURA.deb
#
# O percentual exibido abaixo representa as oito etapas completas do processo.
# APT, make e dpkg também continuam mostrando seus próprios detalhes.
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

# Remove somente o diretório criado por este processo. A verificação explícita
# do prefixo evita que uma variável vazia ou inesperada se torne alvo amplo.
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
        "${TMPDIR:-/tmp}"/fix-names-deb.*)
            [ ! -d "$DIRETORIO_TEMPORARIO" ] ||
                rm -rf -- "$DIRETORIO_TEMPORARIO"
            ;;
    esac
}
trap limpar_temporarios 0 1 2 15

# Mostra uma barra simples e portátil, sem depender de programas externos. O
# número da etapa é convertido em percentual e em trinta células preenchidas.
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
command -v apt-get >/dev/null 2>&1 ||
    erro 'apt-get não foi encontrado; este instalador é específico para Debian.'
command -v sudo >/dev/null 2>&1 ||
    erro 'sudo não está instalado ou não está disponível no PATH.'

# Antes de instalar dependências ou pedir autenticação, confere se o pacote
# extraído contém todos os fontes e recursos necessários. Isso evita que uma
# cópia incompleta avance até a compilação e falhe com uma mensagem obscura.
[ -f "$DIRETORIO_SCRIPT/verificar_projeto.sh" ] ||
    erro 'scripts/verificar_projeto.sh não foi encontrado; pacote incompleto.'
sh "$DIRETORIO_SCRIPT/verificar_projeto.sh"

[ -f "$DIRETORIO_PROJETO/Makefile" ] ||
    erro "Makefile não encontrado em $DIRETORIO_PROJETO"
[ -f "$DIRETORIO_PROJETO/packaging/debian/control.in" ] ||
    erro 'modelo packaging/debian/control.in não encontrado.'

VERSAO=$(sed -n 's/.*VERSION = "\([^"]*\)".*/\1/p' \
    "$DIRETORIO_PROJETO/src/core.hpp")
case $VERSAO in
    ''|*[!0-9.]*) erro 'não foi possível identificar uma versão numérica válida.' ;;
esac

mostrar_etapa 1 'Validando a autorização administrativa'
sudo -v

mostrar_etapa 2 'Instalando dependências de compilação e empacotamento'
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    dpkg-dev \
    pkg-config \
    libncurses-dev \
    libgtk-4-dev \
    qt6-base-dev \
    qt6-qpa-plugins \
    desktop-file-utils \
    xvfb

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
# GDK_BACKEND=x11 obriga o GTK a usar a tela virtual criada acima, mesmo se o
# usuário estiver em uma sessão Wayland. timeout evita que uma regressão no
# encerramento automático deixe o instalador preso indefinidamente.
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

mostrar_etapa 6 'Montando a estrutura e calculando dependências do pacote .deb'
DIRETORIO_TEMPORARIO=$(mktemp -d "${TMPDIR:-/tmp}/fix-names-deb.XXXXXX") ||
    erro 'não foi possível criar o diretório temporário de empacotamento.'
RAIZ_PACOTE=$DIRETORIO_TEMPORARIO/raiz
mkdir -p -- "$RAIZ_PACOTE/DEBIAN" "$DIRETORIO_TEMPORARIO/debian" \
    "$DIRETORIO_PACOTES"
(
    cd "$DIRETORIO_PROJETO"
    make PREFIX=/usr DESTDIR="$RAIZ_PACOTE" install
)

# Páginas de manual de pacotes Debian são comprimidas de forma reproduzível:
# -n impede que data e nome do arquivo entrem no cabeçalho do gzip.
gzip -9n "$RAIZ_PACOTE/usr/share/man/man1/fix-names.1"

# dpkg-shlibdeps examina os ELF realmente compilados e deriva as versões
# mínimas das bibliotecas. Ele exige um arquivo debian/control com parágrafo de
# fonte, criado aqui somente para a análise. O control binário definitivo é
# gerado pelo modelo logo abaixo.
{
    printf '%s\n' 'Source: fix-names'
    printf '%s\n' 'Section: utils'
    printf '%s\n' 'Priority: optional'
    printf '%s\n' 'Maintainer: fix-names Project <fix-names@localhost>'
    printf '%s\n\n' 'Standards-Version: 4.6.2'
    printf '%s\n' 'Package: fix-names'
    printf '%s\n' 'Architecture: any'
    printf '%s\n' 'Description: renomeador em massa seguro'
} > "$DIRETORIO_TEMPORARIO/debian/control"
LOG_SHLIBS=$DIRETORIO_TEMPORARIO/dpkg-shlibdeps.log
if ! SAIDA_SHLIBS=$(cd "$DIRETORIO_TEMPORARIO" && dpkg-shlibdeps -O \
    -e"$RAIZ_PACOTE/usr/bin/fix-names" \
    -e"$RAIZ_PACOTE/usr/bin/fix-names-gtk" \
    -e"$RAIZ_PACOTE/usr/bin/fix-names-qt" 2>"$LOG_SHLIBS"); then
    sed -n '1,120p' "$LOG_SHLIBS" >&2
    erro 'dpkg-shlibdeps não conseguiu calcular as dependências.'
fi
DEPENDENCIAS=$(printf '%s\n' "$SAIDA_SHLIBS" |
    sed -n 's/^shlibs:Depends=//p')
[ -n "$DEPENDENCIAS" ] ||
    erro 'dpkg-shlibdeps não devolveu as dependências do pacote.'
case $DEPENDENCIAS in
    *qt6-qpa-plugins*) : ;;
    *) DEPENDENCIAS="$DEPENDENCIAS, qt6-qpa-plugins" ;;
esac

ARQUITETURA=$(dpkg --print-architecture)
TAMANHO_INSTALADO=$(du -sk "$RAIZ_PACOTE/usr" | awk '{print $1}')
sed -e "s/@VERSION@/$VERSAO/g" \
    -e "s/@ARCHITECTURE@/$ARQUITETURA/g" \
    -e "s/@DEPENDS@/$DEPENDENCIAS/g" \
    -e "s/@INSTALLED_SIZE@/$TAMANHO_INSTALADO/g" \
    "$DIRETORIO_PROJETO/packaging/debian/control.in" \
    > "$RAIZ_PACOTE/DEBIAN/control"
(
    cd "$RAIZ_PACOTE"
    find usr -type f -exec md5sum '{}' + | sort > DEBIAN/md5sums
)

mostrar_etapa 7 'Gerando e verificando o arquivo binário .deb'
PACOTE_DEB=$DIRETORIO_PACOTES/fix-names_${VERSAO}-1_${ARQUITETURA}.deb
dpkg-deb --build --root-owner-group "$RAIZ_PACOTE" "$PACOTE_DEB"
dpkg-deb --info "$PACOTE_DEB" >/dev/null
dpkg-deb --contents "$PACOTE_DEB" >/dev/null

mostrar_etapa 8 'Instalando o pacote gerado com o APT'
sudo apt-get install -y "$PACOTE_DEB"

printf '\n%s\n' 'Instalação concluída com sucesso.'
printf 'Pacote Debian gerado: %s\n' "$PACOTE_DEB"
printf '%s\n' 'CLI/ncurses: fix-names'
printf '%s\n' 'GUI automática: fix-names-gui'
printf '%s\n' 'GUI GTK:       fix-names-gtk'
printf '%s\n' 'GUI Qt:        fix-names-qt'
