#!/bin/sh

# ==============================================================================
# FIX-NAMES — COMPILAÇÃO, EMPACOTAMENTO E INSTALAÇÃO NO ARCH LINUX
# ==============================================================================
#
# Este script executa, em ordem, todo o ciclo nativo do Arch Linux:
#
#   1. confirma que está no Arch Linux ou em um derivado compatível;
#   2. valida por conta própria a integridade de todos os arquivos do projeto;
#   3. instala as dependências oficiais necessárias;
#   4. compila CLI/ncurses, GTK 4 e Qt 6 como usuário comum;
#   5. executa os testes automatizados e os autotestes das duas GUIs;
#   6. cria um tarball-fonte local e um PKGBUILD com SHA-256 verificado;
#   7. chama makepkg para produzir o pacote binário nativo;
#   8. entrega o arquivo final em pacotes/archlinux/;
#   9. valida o pacote com o próprio pacman;
#  10. instala exatamente esse pacote com pacman -U e confirma sua versão.
#
# O script deve ser executado por usuário comum. makepkg proíbe e este projeto
# também rejeita compilação integral como root. sudo fica restrito à instalação
# das dependências e à transação final do pacman.
#
# Destino persistente do pacote:
#
#   pacotes/archlinux/fix-names-VERSAO-1-ARQUITETURA.pkg.tar.zst
# ==============================================================================

set -eu
umask 022

# O script fica diretamente na raiz do pacote completo. A pasta deste arquivo é
# também a raiz do projeto, eliminando a dependência da antiga subpasta scripts/.
# Portanto, ele pode ser chamado de qualquer diretório com:
#
#   /caminho/fix-names-2.1.6/compilar_instalar_arch.sh
DIRETORIO_PROJETO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRETORIO_DESTINO=$DIRETORIO_PROJETO/pacotes/archlinux

TOTAL_ETAPAS=10
DIRETORIO_TEMPORARIO=
ARQUIVO_DISPLAY=
LOG_XVFB=
PID_XVFB=
BACKUP_LEGADO=

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

# Encerra o Xvfb e remove somente temporários com prefixos conhecidos. O pacote
# gerado em pacotes/archlinux fica fora dessa árvore e nunca é apagado aqui.
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
        "${TMPDIR:-/tmp}"/fix-names-archlinux.*)
            [ ! -d "$DIRETORIO_TEMPORARIO" ] ||
                rm -rf -- "$DIRETORIO_TEMPORARIO"
            ;;
    esac
}
trap limpar_temporarios 0 1 2 15

# Barra POSIX de trinta células. Ela representa as dez grandes etapas; pacman,
# make e makepkg continuam mostrando os detalhes internos de cada requisição.
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

# Versões antigas dos instaladores copiavam diretamente para /usr/local. Como
# esse diretório costuma vir antes de /usr/bin no PATH, o pacman podia registrar
# o pacote novo enquanto o comando executado continuava sendo o arquivo antigo.
#
# Todos os caminhos encontrados são movidos para um backup recuperável dentro
# de pacotes/archlinux/. Os quatro comandos antigos recebem links para /usr/bin,
# inclusive para atender terminais que ainda tenham /usr/local/bin memorizado.
migrar_instalacao_legada()
{
    CAMINHOS_LEGADOS='
/usr/local/bin/fix-names
/usr/local/bin/fix-names-gui
/usr/local/bin/fix-names-gtk
/usr/local/bin/fix-names-qt
/usr/local/share/applications/fix-names.desktop
/usr/local/share/pixmaps/fix-names.png
/usr/local/share/man/man1/fix-names.1
/usr/local/share/man/man1/fix-names.1.gz
/usr/local/share/doc/fix-names
'
    CAMINHOS_ENCONTRADOS=

    for CAMINHO_LEGADO in $CAMINHOS_LEGADOS; do
        [ -e "$CAMINHO_LEGADO" ] || [ -L "$CAMINHO_LEGADO" ] || continue
        case $CAMINHO_LEGADO in
            /usr/local/bin/*)
                NOME_COMANDO=${CAMINHO_LEGADO##*/}
                ALVO_REAL=$(readlink -f -- "$CAMINHO_LEGADO" 2>/dev/null || :)
                [ "$ALVO_REAL" != "/usr/bin/$NOME_COMANDO" ] || continue
                ;;
        esac
        CAMINHOS_ENCONTRADOS="${CAMINHOS_ENCONTRADOS}
$CAMINHO_LEGADO"
    done

    [ -n "$CAMINHOS_ENCONTRADOS" ] || return 0

    BACKUP_LEGADO=$(mktemp -d "$DIRETORIO_DESTINO/backup-legado.XXXXXX") ||
        erro 'não foi possível criar o backup da instalação antiga.'
    printf 'Instalação antiga detectada; preservando-a em: %s\n' \
        "$BACKUP_LEGADO"

    for CAMINHO_LEGADO in $CAMINHOS_ENCONTRADOS; do
        CAMINHO_RELATIVO=${CAMINHO_LEGADO#/}
        DESTINO_BACKUP=$BACKUP_LEGADO/$CAMINHO_RELATIVO
        sudo mkdir -p -- "$(dirname -- "$DESTINO_BACKUP")"
        sudo mv -- "$CAMINHO_LEGADO" "$DESTINO_BACKUP"
    done

    for NOME_COMANDO in fix-names fix-names-gui fix-names-gtk fix-names-qt; do
        if [ -e "$BACKUP_LEGADO/usr/local/bin/$NOME_COMANDO" ] ||
           [ -L "$BACKUP_LEGADO/usr/local/bin/$NOME_COMANDO" ]; then
            sudo ln -s -- "/usr/bin/$NOME_COMANDO" \
                "/usr/local/bin/$NOME_COMANDO"
        fi
    done

    sudo chown -R "$(id -u):$(id -g)" "$BACKUP_LEGADO"
}

# Valida localmente todo o conjunto necessário. A versão 2.1.3 chamava um
# arquivo externo em scripts/verificar_projeto.sh; se a subpasta não estivesse
# ao lado do instalador, a execução terminava antes de compilar. Esta função
# incorpora a mesma proteção e torna este gerador de pacote autossuficiente.
validar_projeto()
{
    ARQUIVOS_OBRIGATORIOS='
Makefile
README.md
DOCUMENTACAO.md
CHANGELOG.md
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
docs/MANUAL_DO_USUARIO.md
docs/REFERENCIA_CLI.md
docs/INTERFACES.md
docs/ARQUITETURA_E_SEGURANCA.md
docs/REFERENCIA_DO_NUCLEO.md
docs/COMPILACAO_E_EMPACOTAMENTO.md
docs/DESENVOLVIMENTO_E_TESTES.md
docs/SOLUCAO_DE_PROBLEMAS.md
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

    # O manifesto detecta corrupção ou extração incompleta antes que pacman seja
    # chamado. SHA256SUMS fica fora de sua própria lista para evitar circularidade.
    command -v sha256sum >/dev/null 2>&1 ||
        erro 'sha256sum não foi encontrado; instale coreutils.'
    (
        cd "$DIRETORIO_PROJETO"
        sha256sum -c SHA256SUMS
    ) >/dev/null ||
        erro 'a verificação SHA-256 falhou; extraia novamente o pacote completo.'

    # Todos os pontos de entrada devem obedecer ao Shell POSIX. A checagem ocorre
    # antes de qualquer ação administrativa ou criação de temporários.
    for SCRIPT_POSIX in \
        instalar.sh \
        compilar_instalar_arch.sh \
        compilar_instalar_debian.sh \
        data/fix-names-gui
    do
        sh -n "$DIRETORIO_PROJETO/$SCRIPT_POSIX" ||
            erro "sintaxe Shell inválida: $SCRIPT_POSIX"
    done

    # Os marcadores são necessários para o script gerar um PKGBUILD específico
    # da versão e com a soma real do tarball-fonte local.
    grep -q '@VERSION@' "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ||
        erro 'marcador @VERSION@ ausente do PKGBUILD.in.'
    grep -q '@SOURCE_SHA256@' "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ||
        erro 'marcador @SOURCE_SHA256@ ausente do PKGBUILD.in.'

    # A assinatura binária impede que um arquivo truncado seja instalado como
    # ícone do aplicativo.
    ASSINATURA_PNG=$(dd if="$DIRETORIO_PROJETO/assets/fix-names.png" \
        bs=8 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
    [ "$ASSINATURA_PNG" = '89504e470d0a1a0a' ] ||
        erro 'assets/fix-names.png não possui uma assinatura PNG válida.'

    printf 'Estrutura completa verificada: %d arquivos obrigatórios.\n' \
        "$TOTAL_ARQUIVOS"
}

[ "$#" -eq 0 ] || erro 'este script não recebe parâmetros.'
[ "$(id -u)" -ne 0 ] ||
    erro 'não execute este script como root; use um usuário comum com sudo.'
[ "$(uname -s 2>/dev/null || printf desconhecido)" = Linux ] ||
    erro 'este empacotador é destinado ao Linux.'
[ -r /etc/os-release ] ||
    erro '/etc/os-release não foi encontrado; distribuição não identificada.'

# ID identifica o Arch; ID_LIKE cobre derivados compatíveis com pacman/makepkg.
ID=
ID_LIKE=
# shellcheck disable=SC1091
. /etc/os-release
IDENTIFICADORES=" ${ID-} ${ID_LIKE-} "
case $IDENTIFICADORES in
    *' arch '*|*' archlinux '*) : ;;
    *) erro "este script exige Arch Linux ou derivado compatível (ID=${ID-})." ;;
esac

command -v pacman >/dev/null 2>&1 ||
    erro 'pacman não foi encontrado.'
command -v makepkg >/dev/null 2>&1 ||
    erro 'makepkg não foi encontrado; instale base-devel.'
command -v bsdtar >/dev/null 2>&1 ||
    erro 'bsdtar não foi encontrado; instale libarchive.'
command -v sudo >/dev/null 2>&1 ||
    erro 'sudo não está instalado ou não está disponível no PATH.'

# O próprio instalador confere estrutura, hashes e modelos. Nenhum arquivo da
# antiga subpasta scripts/ é necessário para esta etapa.
validar_projeto

[ -f "$DIRETORIO_PROJETO/Makefile" ] ||
    erro "Makefile não encontrado em $DIRETORIO_PROJETO"
[ -f "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" ] ||
    erro 'modelo packaging/arch/PKGBUILD.in não encontrado.'

# Núcleo, PKGBUILD, nome do arquivo e verificação final compartilham a mesma
# versão para eliminar divergências entre o executável e o pacote instalado.
VERSAO=$(sed -n 's/.*VERSION = "\([^"]*\)".*/\1/p' \
    "$DIRETORIO_PROJETO/src/core.hpp")
case $VERSAO in
    ''|*[!0-9.]*) erro 'não foi possível identificar uma versão numérica válida.' ;;
esac
VERSAO_PACOTE=${VERSAO}-1

mostrar_etapa 1 'Validando a autorização administrativa'
sudo -v

mostrar_etapa 2 'Instalando dependências oficiais do Arch Linux'

# --needed evita reinstalações desnecessárias. --noconfirm torna a preparação e
# a instalação finais automáticas depois que sudo autenticar o usuário.
sudo pacman -S --needed --noconfirm \
    base-devel \
    pkgconf \
    ncurses \
    gtk4 \
    qt6-base \
    desktop-file-utils \
    xorg-server-xvfb

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

# timeout converte qualquer janela que não complete o autoteste e o fechamento
# em falha objetiva, antes que makepkg produza um pacote defeituoso.
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

mostrar_etapa 6 'Criando o tarball-fonte local e o PKGBUILD verificado'
DIRETORIO_TEMPORARIO=$(mktemp -d "${TMPDIR:-/tmp}/fix-names-archlinux.XXXXXX") ||
    erro 'não foi possível criar o diretório temporário do makepkg.'
RAIZ_FONTE=$DIRETORIO_TEMPORARIO/fix-names-$VERSAO
ARQUIVO_FONTE=$DIRETORIO_TEMPORARIO/fix-names-$VERSAO.tar.gz
mkdir -p -- "$RAIZ_FONTE" "$DIRETORIO_DESTINO"

# Dois processos tar fazem uma cópia lógica do projeto para o tarball-fonte.
# Resultados de compilação, pacotes já criados e metadados Git são excluídos,
# impedindo artefatos locais de contaminarem o pacote reproduzível.
(
    cd "$DIRETORIO_PROJETO"
    tar --exclude='./build' \
        --exclude='./pacotes' \
        --exclude='./.git' \
        --exclude='./*.tar.gz' \
        -cf - .
) | (
    cd "$RAIZ_FONTE"
    tar -xf -
)

tar -C "$DIRETORIO_TEMPORARIO" -czf "$ARQUIVO_FONTE" \
    "fix-names-$VERSAO"
SOMA_FONTE=$(sha256sum "$ARQUIVO_FONTE" | awk '{print $1}')
case $SOMA_FONTE in
    ''|*[!0-9a-f]*) erro 'não foi possível calcular o SHA-256 do tarball-fonte.' ;;
esac
[ "${#SOMA_FONTE}" -eq 64 ] ||
    erro 'a soma SHA-256 do tarball-fonte não possui 64 caracteres.'

sed -e "s/@VERSION@/$VERSAO/g" \
    -e "s/@SOURCE_SHA256@/$SOMA_FONTE/g" \
    "$DIRETORIO_PROJETO/packaging/arch/PKGBUILD.in" \
    > "$DIRETORIO_TEMPORARIO/PKGBUILD"

mostrar_etapa 7 'Gerando o pacote com makepkg em pacotes/archlinux'

# --cleanbuild evita reutilizar src/pkg antigos, --force permite reconstruir a
# mesma versão e --noconfirm mantém o fluxo automático. PKGDEST faz o makepkg
# colocar o produto diretamente na pasta de destino pedida pelo projeto.
(
    cd "$DIRETORIO_TEMPORARIO"
    PKGDEST="$DIRETORIO_DESTINO" MAKEFLAGS="-j$JOBS" \
        makepkg --clean --cleanbuild --force --noconfirm
)

mostrar_etapa 8 'Identificando e preservando o pacote binário principal'

# A lista vem do próprio makepkg. O filtro seleciona o pacote fix-names normal
# e ignora um eventual pacote de depuração habilitado no makepkg.conf local.
LISTA_PACOTES=$(cd "$DIRETORIO_TEMPORARIO" && \
    PKGDEST="$DIRETORIO_DESTINO" makepkg --packagelist)
PACOTE_ARCH=
for CANDIDATO in $LISTA_PACOTES; do
    NOME_CANDIDATO=$(basename -- "$CANDIDATO")
    case $NOME_CANDIDATO in
        fix-names-"$VERSAO"-*.pkg.tar.zst)
            [ -z "$PACOTE_ARCH" ] ||
                erro 'makepkg informou mais de um pacote principal inesperado.'
            PACOTE_ARCH=$CANDIDATO
            ;;
    esac
done

[ -n "$PACOTE_ARCH" ] ||
    erro 'makepkg não informou um pacote fix-names .pkg.tar.zst.'
[ -s "$PACOTE_ARCH" ] || erro "o pacote não foi criado: $PACOTE_ARCH"

# Garante que mesmo uma configuração incomum do makepkg não redirecione o
# pacote para fora da pasta pacotes/archlinux definida por este script.
case $PACOTE_ARCH in
    "$DIRETORIO_DESTINO"/*) : ;;
    *) erro "makepkg devolveu um destino inesperado: $PACOTE_ARCH" ;;
esac

mostrar_etapa 9 'Validando nome, versão e conteúdo com o pacman'
IDENTIDADE_PACOTE=$(pacman -Qp "$PACOTE_ARCH")
case $IDENTIDADE_PACOTE in
    "fix-names $VERSAO_PACOTE") : ;;
    *) erro "identidade interna inesperada: $IDENTIDADE_PACOTE" ;;
esac
pacman -Qlp "$PACOTE_ARCH" >/dev/null

# Extrai o pacote em uma pasta temporária e consulta o próprio binário. Assim,
# a versão declarada pelo PKGBUILD não pode divergir do programa compilado.
CONTEUDO_VALIDADO=$DIRETORIO_TEMPORARIO/conteudo-validado
mkdir -p -- "$CONTEUDO_VALIDADO"
bsdtar -xf "$PACOTE_ARCH" -C "$CONTEUDO_VALIDADO"
for NOME_COMANDO in fix-names fix-names-gui fix-names-gtk fix-names-qt; do
    [ -x "$CONTEUDO_VALIDADO/usr/bin/$NOME_COMANDO" ] ||
        erro "executável ausente no pacote: /usr/bin/$NOME_COMANDO"
done
VERSAO_NO_PACOTE=$("$CONTEUDO_VALIDADO/usr/bin/fix-names" --version)
[ "$VERSAO_NO_PACOTE" = "fix-names $VERSAO" ] ||
    erro "o binário dentro do pacote informa '$VERSAO_NO_PACOTE'."

mostrar_etapa 10 'Instalando o pacote validado automaticamente com pacman -U'

# Não se usa --needed aqui: ao reconstruir a mesma revisão, o pacman precisa
# reinstalar o arquivo local em vez de ignorá-lo como já presente.
sudo pacman -U --noconfirm "$PACOTE_ARCH"

migrar_instalacao_legada
sudo update-desktop-database /usr/share/applications >/dev/null 2>&1 || :
[ ! -d /usr/local/share/applications ] ||
    sudo update-desktop-database /usr/local/share/applications \
        >/dev/null 2>&1 || :

VERSAO_INSTALADA=$(pacman -Q fix-names 2>/dev/null | awk '{print $2}' || :)
[ "$VERSAO_INSTALADA" = "$VERSAO_PACOTE" ] ||
    erro "a confirmação pós-instalação devolveu '$VERSAO_INSTALADA'."

[ -x /usr/bin/fix-names ] ||
    erro 'o pacote foi registrado, mas /usr/bin/fix-names não existe.'
DONO_EXECUTAVEL=$(pacman -Qoq /usr/bin/fix-names 2>/dev/null || :)
[ "$DONO_EXECUTAVEL" = fix-names ] ||
    erro "o pacman não reconheceu /usr/bin/fix-names como parte do pacote."

VERSAO_EXECUTAVEL=$(/usr/bin/fix-names --version)
[ "$VERSAO_EXECUTAVEL" = "fix-names $VERSAO" ] ||
    erro "/usr/bin/fix-names informa '$VERSAO_EXECUTAVEL'."

hash -r 2>/dev/null || :
CAMINHO_RESOLVIDO=$(command -v fix-names 2>/dev/null || :)
[ -n "$CAMINHO_RESOLVIDO" ] ||
    erro 'o comando fix-names não foi encontrado no PATH depois da instalação.'
CAMINHO_REAL=$(readlink -f -- "$CAMINHO_RESOLVIDO" 2>/dev/null || :)
[ "$CAMINHO_REAL" = /usr/bin/fix-names ] ||
    erro "o comando fix-names ainda resolve para '$CAMINHO_RESOLVIDO'."
VERSAO_RESOLVIDA=$(fix-names --version)
[ "$VERSAO_RESOLVIDA" = "fix-names $VERSAO" ] ||
    erro "o comando resolvido informa '$VERSAO_RESOLVIDA'."

printf '\n%s\n' 'Compilação, empacotamento e instalação concluídos com sucesso.'
printf 'Pacote Arch Linux preservado em: %s\n' "$PACOTE_ARCH"
printf 'Versão registrada pelo pacman: %s\n' "$VERSAO_INSTALADA"
printf 'Versão do executável: %s\n' "$VERSAO_EXECUTAVEL"
printf 'Comando resolvido para: %s -> %s\n' \
    "$CAMINHO_RESOLVIDO" "$CAMINHO_REAL"
[ -z "$BACKUP_LEGADO" ] ||
    printf 'Backup da instalação antiga: %s\n' "$BACKUP_LEGADO"
printf '%s\n' 'CLI/ncurses: fix-names'
printf '%s\n' 'GUI automática: fix-names-gui'
printf '%s\n' 'GUI GTK:       fix-names-gtk'
printf '%s\n' 'GUI Qt:        fix-names-qt'
