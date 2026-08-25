#!/bin/sh

# ==============================================================================
# INSTALADOR PRINCIPAL DO FIX-NAMES
# ==============================================================================
#
# Este arquivo é o ponto de entrada mais simples para a instalação. Ele:
#
#   1. impede execução como root, conforme a regra de segurança do projeto;
#   2. confirma que os dois geradores autossuficientes estão na raiz;
#   3. identifica a distribuição por /etc/os-release;
#   4. encaminha o processo ao gerador de pacote Debian ou Arch Linux.
#
# O instalador específico compilará CLI/ncurses, GTK e Qt, executará os testes,
# criará o pacote binário nativo e o instalará pelo gerenciador da distribuição.
# Nenhuma senha é armazenada: sudo solicita autenticação quando necessário.
# ==============================================================================

set -eu

DIRETORIO_PROJETO=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

erro()
{
    printf 'ERRO: %s\n' "$1" >&2
    exit 1
}

[ "$(id -u)" -ne 0 ] ||
    erro 'não execute este instalador como root; use um usuário comum com sudo.'

[ "$#" -eq 0 ] ||
    erro 'este instalador não recebe parâmetros.'

[ "$(uname -s 2>/dev/null || printf desconhecido)" = Linux ] ||
    erro 'o fix-names desta entrega é destinado ao Linux.'

# Os dois geradores ficam na raiz e incorporam suas próprias verificações de
# estrutura, SHA-256, sintaxe e modelos. O instalador principal só precisa
# garantir que o ponto de entrada correspondente existe; a validação completa
# será executada pelo próprio gerador antes do primeiro uso de sudo.
[ -f "$DIRETORIO_PROJETO/compilar_instalar_debian.sh" ] ||
    erro 'compilar_instalar_debian.sh não foi encontrado na raiz do pacote.'
[ -f "$DIRETORIO_PROJETO/compilar_instalar_arch.sh" ] ||
    erro 'compilar_instalar_arch.sh não foi encontrado na raiz do pacote.'

# /etc/os-release é a identificação padronizada das distribuições Linux. As
# variáveis são carregadas somente desse arquivo fixo do sistema, nunca de um
# caminho fornecido pelo usuário. ID_LIKE cobre derivadas compatíveis.
[ -r /etc/os-release ] ||
    erro '/etc/os-release não foi encontrado; distribuição não identificada.'

ID=
ID_LIKE=
# shellcheck disable=SC1091
. /etc/os-release

IDENTIFICADORES=" ${ID-} ${ID_LIKE-} "

case $IDENTIFICADORES in
    *' debian '*|*' ubuntu '*)
        printf '%s\n' 'Distribuição compatível com Debian detectada.'
        exec sh "$DIRETORIO_PROJETO/compilar_instalar_debian.sh"
        ;;
    *' arch '*|*' archlinux '*)
        printf '%s\n' 'Arch Linux ou derivada compatível detectada.'
        exec sh "$DIRETORIO_PROJETO/compilar_instalar_arch.sh"
        ;;
    *)
        erro "distribuição não suportada automaticamente (ID=${ID-}; ID_LIKE=${ID_LIKE-})."
        ;;
esac
