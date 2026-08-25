#!/bin/sh

# ==============================================================================
# FIX-NAMES — COMPILAÇÃO, EMPACOTAMENTO .DEB E INSTALAÇÃO NO DEBIAN
# ==============================================================================
#
# Este script executa, em ordem, todo o ciclo necessário no Debian:
#
#   1. confirma que está em um sistema Debian ou derivado compatível;
#   2. valida por conta própria a integridade de todos os arquivos do projeto;
#   3. instala as dependências de compilação e empacotamento;
#   4. compila CLI/ncurses, GTK 4 e Qt 6 como usuário comum;
#   5. executa os testes automatizados e os autotestes das duas GUIs;
#   6. monta uma raiz de pacote temporária sem escrever diretamente em /usr;
#   7. calcula as dependências ELF reais com dpkg-shlibdeps;
#   8. cria e valida o pacote binário .deb;
#   9. entrega o arquivo final em pacotes/debian/;
#  10. instala exatamente esse .deb com o APT e confirma a versão instalada.
#
# O script inteiro deve ser iniciado por um usuário comum. sudo é empregado
# somente para instalar dependências e para a transação final do APT. O código
# nunca é compilado, testado ou empacotado como root.
#
# Destino persistente do pacote:
#
#   pacotes/debian/fix-names_VERSAO-1_ARQUITETURA.deb
#
# O diretório temporário é removido automaticamente ao terminar. O pacote
# colocado em pacotes/debian/ não pertence à área temporária e é preservado.
# ==============================================================================

set -eu
umask 022

# O script fica na raiz do pacote completo. Portanto, a pasta que contém este
# arquivo é também a raiz do projeto. Essa decisão elimina a dependência que a
# versão 2.1.3 tinha de uma subpasta scripts/ e permite executar exatamente:
#
#   ./compilar_instalar_debian.sh
#
# O caminho absoluto continua tornando a execução independente do diretório de
# trabalho atual do terminal.
DIRETORIO_PROJETO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRETORIO_DESTINO=$DIRETORIO_PROJETO/pacotes/debian

TOTAL_ETAPAS=10
DIRETORIO_TEMPORARIO=
ARQUIVO_DISPLAY=
LOG_XVFB=
PID_XVFB=

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

# Encerra o Xvfb, se ele ainda estiver ativo, e remove somente caminhos criados
# por mktemp com os prefixos exclusivos deste script. As verificações por case
# impedem que uma variável vazia ou inesperada seja usada como alvo de remoção.
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
        "${TMPDIR:-/tmp}"/fix-names-debian.*)
            [ ! -d "$DIRETORIO_TEMPORARIO" ] ||
                rm -rf -- "$DIRETORIO_TEMPORARIO"
            ;;
    esac
}
trap limpar_temporarios 0 1 2 15

# Desenha uma barra POSIX de trinta células. O percentual indica o avanço das
# dez fases do instalador; make, APT e dpkg continuam exibindo seus próprios
# detalhes dentro da fase correspondente.
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

# Faz dentro deste próprio script a verificação que antes dependia do arquivo
# externo scripts/verificar_projeto.sh. Assim, uma subpasta ausente nunca causa
# a falha mostrada na versão 2.1.3. Cada componente necessário à compilação e à
# criação do .deb precisa existir, ser um arquivo regular e não estar vazio.
validar_projeto()
{
    ARQUIVOS_OBRIGATORIOS='
Makefile
README.md
instalar.sh
SHA256SUMS
CONTEUDO_DO_PACOTE.txt
compilar_instalar_arch.sh
compilar_instalar_debian.sh
assets/fix-names.png
data/fix-names-gui
data/fix-names.1
data/fix-names.desktop
packaging/arch/PKGBUILD.in
packaging/debian/control.in
src/core.cpp
src/core.hpp
src/cli_main.cpp
src/ncurses_ui.cpp
src/ncurses_ui.hpp
src/gtk_main.cpp
src/qt_main.cpp
tests/test_core.cpp
'

    TOTAL_ARQUIVOS=0
    for CAMINHO_RELATIVO in $ARQUIVOS_OBRIGATORIOS; do
        CAMINHO_COMPLETO=$DIRETORIO_PROJETO/$CAMINHO_RELATIVO
        [ -f "$CAMINHO_COMPLETO" ] ||
            erro "arquivo obrigatório ausente: $CAMINHO_RELATIVO"
        [ -s "$CAMINHO_COMPLETO" ] ||
            erro "arquivo obrigatório vazio: $CAMINHO_RELATIVO"
        TOTAL_ARQUIVOS=$((TOTAL_ARQUIVOS + 1))
    done

    # SHA256SUMS é gerado somente depois que a árvore final está pronta. A
    # conferência detecta qualquer arquivo truncado, trocado ou corrompido. O
    # manifesto não lista a si próprio para evitar uma dependência circular.
    command -v sha256sum >/dev/null 2>&1 ||
        erro 'sha256sum não foi encontrado; instale coreutils.'
    (
        cd "$DIRETORIO_PROJETO"
        sha256sum -c SHA256SUMS
    ) >/dev/null ||
        erro 'a verificação SHA-256 falhou; extraia novamente o pacote completo.'

    # Verifica a sintaxe dos três pontos de entrada Shell antes de sudo, APT ou
    # qualquer alteração administrativa. Todos foram escritos para /bin/sh.
    for SCRIPT_POSIX in \
        instalar.sh \
        compilar_instalar_arch.sh \
        compilar_instalar_debian.sh \
        data/fix-names-gui
    do
        sh -n "$DIRETORIO_PROJETO/$SCRIPT_POSIX" ||
            erro "sintaxe Shell inválida: $SCRIPT_POSIX"
    done

    # Confere os marcadores que serão substituídos na montagem dos metadados.
    # Um modelo parcialmente editado produziria um pacote inválido.
    grep -q '@VERSION@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
        erro 'marcador @VERSION@ ausente do control.in.'
    grep -q '@ARCHITECTURE@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
        erro 'marcador @ARCHITECTURE@ ausente do control.in.'
    grep -q '@DEPENDS@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
        erro 'marcador @DEPENDS@ ausente do control.in.'
    grep -q '@INSTALLED_SIZE@' "$DIRETORIO_PROJETO/packaging/debian/control.in" ||
        erro 'marcador @INSTALLED_SIZE@ ausente do control.in.'

    # A assinatura de oito bytes confirma que o ícone é realmente um PNG, não
    # apenas um arquivo com essa extensão.
    ASSINATURA_PNG=$(dd if="$DIRETORIO_PROJETO/assets/fix-names.png" \
        bs=8 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
    [ "$ASSINATURA_PNG" = '89504e470d0a1a0a' ] ||
        erro 'assets/fix-names.png não possui uma assinatura PNG válida.'

    printf 'Estrutura completa verificada: %d arquivos obrigatórios.\n' \
        "$TOTAL_ARQUIVOS"
}

# Impede parâmetros silenciosamente ignorados e execução integral como root.
# A restrição mantém os artefatos de compilação pertencentes ao usuário e está
# de acordo com a proteção do próprio aplicativo contra EUID 0.
[ "$#" -eq 0 ] || erro 'este script não recebe parâmetros.'
[ "$(id -u)" -ne 0 ] ||
    erro 'não execute este script como root; use um usuário comum com sudo.'

[ "$(uname -s 2>/dev/null || printf desconhecido)" = Linux ] ||
    erro 'este empacotador é destinado ao Linux.'
[ -r /etc/os-release ] ||
    erro '/etc/os-release não foi encontrado; distribuição não identificada.'

# /etc/os-release é um arquivo fixo e administrado pelo sistema. ID cobre o
# Debian; ID_LIKE permite derivados que mantenham compatibilidade com APT/dpkg.
ID=
ID_LIKE=
# shellcheck disable=SC1091
. /etc/os-release
IDENTIFICADORES=" ${ID-} ${ID_LIKE-} "
case $IDENTIFICADORES in
    *' debian '*) : ;;
    *) erro "este script exige Debian ou derivado compatível (ID=${ID-})." ;;
esac

command -v apt-get >/dev/null 2>&1 ||
    erro 'apt-get não foi encontrado.'
command -v dpkg >/dev/null 2>&1 ||
    erro 'dpkg não foi encontrado.'
command -v sudo >/dev/null 2>&1 ||
    erro 'sudo não está instalado ou não está disponível no PATH.'

# A validação é uma função local e não depende de scripts auxiliares. Ela roda
# antes do primeiro sudo para impedir mudanças no sistema se o pacote estiver
# incompleto ou corrompido.
validar_projeto

[ -f "$DIRETORIO_PROJETO/Makefile" ] ||
    erro "Makefile não encontrado em $DIRETORIO_PROJETO"
[ -f "$DIRETORIO_PROJETO/packaging/debian/control.in" ] ||
    erro 'modelo packaging/debian/control.in não encontrado.'

# A única fonte de versão é a constante do núcleo. O mesmo valor será usado no
# nome do arquivo, nos metadados Debian e na confirmação após a instalação.
VERSAO=$(sed -n 's/.*VERSION = "\([^"]*\)".*/\1/p' \
    "$DIRETORIO_PROJETO/src/core.hpp")
case $VERSAO in
    ''|*[!0-9.]*) erro 'não foi possível identificar uma versão numérica válida.' ;;
esac
VERSAO_PACOTE=${VERSAO}-1

mostrar_etapa 1 'Validando a autorização administrativa'
sudo -v

mostrar_etapa 2 'Instalando dependências do Debian'
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

# Usa todos os processadores disponíveis quando getconf fornecer um inteiro
# positivo. Um valor inválido ou zero recua com segurança para um único job.
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')
case $JOBS in
    ''|*[!0-9]*|0) JOBS=1 ;;
esac

mostrar_etapa 3 'Limpando resultados anteriores e compilando o programa completo'
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

mostrar_etapa 5 'Executando os autotestes GTK e Qt em uma tela virtual'
ARQUIVO_DISPLAY=$(mktemp "${TMPDIR:-/tmp}/fix-names-display.XXXXXX") ||
    erro 'não foi possível criar o arquivo temporário do teste gráfico.'
LOG_XVFB=$(mktemp "${TMPDIR:-/tmp}/fix-names-xvfb.XXXXXX") ||
    erro 'não foi possível criar o log temporário do teste gráfico.'

# -displayfd pede ao Xvfb uma tela livre, evitando colisão com displays em uso.
# Os backends X11 fazem GTK e Qt usarem exatamente essa tela virtual.
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
case $NUMERO_DISPLAY in
    ''|*[!0-9]*) erro 'Xvfb devolveu um número de display inválido.' ;;
esac

STATUS_GUI=0
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
[ "$STATUS_GUI" -eq 0 ] ||
    erro "uma interface gráfica falhou no autoteste (status $STATUS_GUI)."

mostrar_etapa 6 'Montando a raiz temporária do pacote Debian'
DIRETORIO_TEMPORARIO=$(mktemp -d "${TMPDIR:-/tmp}/fix-names-debian.XXXXXX") ||
    erro 'não foi possível criar o diretório temporário de empacotamento.'
RAIZ_PACOTE=$DIRETORIO_TEMPORARIO/raiz
mkdir -p -- "$RAIZ_PACOTE/DEBIAN" "$DIRETORIO_TEMPORARIO/debian" \
    "$DIRETORIO_DESTINO"

# DESTDIR redireciona a instalação do Makefile para a raiz temporária. Nada é
# copiado ao sistema real nesta etapa; /usr dentro de RAIZ_PACOTE é apenas a
# futura árvore interna do .deb.
(
    cd "$DIRETORIO_PROJETO"
    make PREFIX=/usr DESTDIR="$RAIZ_PACOTE" install
)

# -n elimina data e nome original do cabeçalho gzip, tornando a página de
# manual reproduzível entre duas compilações do mesmo conteúdo.
gzip -9n "$RAIZ_PACOTE/usr/share/man/man1/fix-names.1"

mostrar_etapa 7 'Calculando dependências ELF e escrevendo os metadados'

# dpkg-shlibdeps necessita de um parágrafo-fonte auxiliar para analisar os ELF.
# O control definitivo do pacote binário é produzido depois com control.in.
{
    printf '%s\n' 'Source: fix-names'
    printf '%s\n' 'Section: utils'
    printf '%s\n' 'Priority: optional'
    printf '%s\n' 'Maintainer: fix-names Project <fix-names@localhost>'
    printf '%s\n\n' 'Standards-Version: 4.7.0'
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

# O plugin xcb é carregado dinamicamente pelo Qt e pode não aparecer na tabela
# ELF; por isso sua dependência de execução é acrescentada explicitamente.
case $DEPENDENCIAS in
    *qt6-qpa-plugins*) : ;;
    *) DEPENDENCIAS="$DEPENDENCIAS, qt6-qpa-plugins" ;;
esac

ARQUITETURA=$(dpkg --print-architecture)
case $ARQUITETURA in
    ''|*[!a-zA-Z0-9_-]*) erro 'dpkg devolveu uma arquitetura inválida.' ;;
esac

TAMANHO_INSTALADO=$(du -sk "$RAIZ_PACOTE/usr" | awk '{print $1}')
case $TAMANHO_INSTALADO in
    ''|*[!0-9]*) erro 'não foi possível calcular Installed-Size.' ;;
esac

sed -e "s/@VERSION@/$VERSAO/g" \
    -e "s/@ARCHITECTURE@/$ARQUITETURA/g" \
    -e "s/@DEPENDS@/$DEPENDENCIAS/g" \
    -e "s/@INSTALLED_SIZE@/$TAMANHO_INSTALADO/g" \
    "$DIRETORIO_PROJETO/packaging/debian/control.in" \
    > "$RAIZ_PACOTE/DEBIAN/control"

# md5sums é um índice tradicional do Debian para verificar os arquivos comuns
# instalados. Os caminhos são gravados relativos à raiz do pacote.
(
    cd "$RAIZ_PACOTE"
    find usr -type f -exec md5sum '{}' + | sort > DEBIAN/md5sums
)

mostrar_etapa 8 'Gerando o .deb diretamente na pasta pacotes/debian'
PACOTE_DEB=$DIRETORIO_DESTINO/fix-names_${VERSAO_PACOTE}_${ARQUITETURA}.deb
dpkg-deb --build --root-owner-group "$RAIZ_PACOTE" "$PACOTE_DEB"
[ -s "$PACOTE_DEB" ] || erro "o pacote não foi criado: $PACOTE_DEB"

mostrar_etapa 9 'Validando nome, versão, arquitetura e conteúdo do .deb'
NOME_VALIDADO=$(dpkg-deb -f "$PACOTE_DEB" Package)
VERSAO_VALIDADA=$(dpkg-deb -f "$PACOTE_DEB" Version)
ARQUITETURA_VALIDADA=$(dpkg-deb -f "$PACOTE_DEB" Architecture)

[ "$NOME_VALIDADO" = fix-names ] ||
    erro "nome interno inesperado no .deb: $NOME_VALIDADO"
[ "$VERSAO_VALIDADA" = "$VERSAO_PACOTE" ] ||
    erro "versão interna inesperada no .deb: $VERSAO_VALIDADA"
[ "$ARQUITETURA_VALIDADA" = "$ARQUITETURA" ] ||
    erro "arquitetura interna inesperada no .deb: $ARQUITETURA_VALIDADA"
dpkg-deb --contents "$PACOTE_DEB" >/dev/null

mostrar_etapa 10 'Instalando o pacote validado automaticamente com o APT'

# Um caminho absoluto contendo / faz o APT tratar o argumento como pacote local
# e ainda resolver qualquer dependência que não esteja instalada.
sudo apt-get install -y "$PACOTE_DEB"

VERSAO_INSTALADA=$(dpkg-query -W -f='${Version}' fix-names 2>/dev/null || :)
[ "$VERSAO_INSTALADA" = "$VERSAO_PACOTE" ] ||
    erro "a confirmação pós-instalação devolveu '$VERSAO_INSTALADA'."

printf '\n%s\n' 'Compilação, empacotamento e instalação concluídos com sucesso.'
printf 'Pacote Debian preservado em: %s\n' "$PACOTE_DEB"
printf 'Versão instalada: %s\n' "$VERSAO_INSTALADA"
printf '%s\n' 'CLI/ncurses: fix-names'
printf '%s\n' 'GUI automática: fix-names-gui'
printf '%s\n' 'GUI GTK:       fix-names-gtk'
printf '%s\n' 'GUI Qt:        fix-names-qt'
