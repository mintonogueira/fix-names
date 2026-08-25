# Solução de problemas

## O programa recusa execução como root

Mensagem típica:

```text
ERRO: o fix-names nunca pode ser executado como root.
```

Isso é uma proteção do projeto. Saia da sessão root e use uma conta comum. Nos
instaladores, execute `./instalar.sh`, sem colocar `sudo` na frente; o próprio
script chamará `sudo` apenas quando necessário.

## Nenhuma transformação foi ativada

Ative ao menos uma transformação. Opções de escopo como `--recursive`,
`--dry-run` ou `--include-extension` não são transformações por si só.

```bash
fix-names --lowercase --dry-run .
```

## O substituto de espaço/sublinhado foi recusado

O argumento deve conter exatamente um caractere UTF-8 e não pode ser `/`.

```bash
fix-names --spaces=- --dry-run .
```

`--spaces=--` é inválido porque contém dois caracteres.

## A extensão não mudou

Esse é o padrão seguro. Use `--include-extension` somente se quiser que a
transformação alcance tudo a partir do primeiro ponto de extensão.

## O conteúdo das subpastas não mudou

Sem `--recursive`, subpastas podem ser renomeadas, mas não são percorridas.

```bash
fix-names --lowercase --recursive --dry-run .
```

## Foram relatados conflitos

O programa não sobrescreve nem escolhe um dos itens. Revise as linhas
`[CONFLITO]`, ajuste a transformação/exclusões ou renomeie manualmente um dos
nomes envolvidos. Execute novamente a simulação.

## `renameat2` ou operação não suportada

O núcleo exige Linux com `renameat2(RENAME_NOREPLACE)`. Ele falha de propósito
se não puder garantir a ausência de sobrescrita. Não substitua a chamada por
`rename()` sem rever o modelo de segurança.

## A versão do Debian não atualizou

A versão `2.1.6` corrige esse fluxo. Verifique:

```bash
dpkg-query -W -f='${Version}\n' fix-names
/usr/bin/fix-names --version
readlink -f "$(command -v fix-names)"
```

Resultados esperados:

```text
2.1.6-1
fix-names 2.1.6
/usr/bin/fix-names
```

Se `command -v` ainda apontar para outro caminho, abra um terminal novo ou
execute `hash -r`. O instalador move instalações legadas de `/usr/local` para
um `backup-legado.*` na pasta `pacotes/debian/`.

## Erro de compilação Qt envolvendo `tr()`

A falha das versões anteriores foi corrigida em `2.1.5`: mensagens internas da
classe Qt chamam explicitamente `fixnames::tr()`, sem colisão com
`QObject::tr()`. Extraia a versão `2.1.6` em uma pasta nova e não misture fontes
de versões diferentes.

## Arch: `copy relocation` envolvendo `QWidget`

Na versão `2.1.6`, o teste real do gerador Arch pode chegar à vinculação de
`fix-names-qt` e terminar com uma mensagem semelhante a:

```text
copy relocation against protected symbol `_ZTI7QWidget@@Qt_6`
```

Esse erro é diferente da colisão com `tr()` corrigida em `2.1.5`. Ele ocorre
na combinação das flags LTO/PIC/PIE usadas durante a compilação e a vinculação
contra o Qt 6 do Arch. Como o pacote não termina de ser criado, nada deve ser
instalado por essa execução. Preserve a saída completa e aguarde uma versão que
uniformize essas flags em todos os objetos compartilhados; não desative as
proteções do linker nem force a instalação de um pacote incompleto.

## O script procura `scripts/verificar_projeto.sh`

Essa estrutura pertence a uma entrega antiga. Na versão `2.1.6`, os geradores
ficam na raiz e são autossuficientes. Se a mensagem ainda aparecer, a pasta
mistura arquivos de versões diferentes. Extraia o pacote completo em uma pasta
vazia.

## Falha na verificação SHA-256

Execute:

```bash
sha256sum -c SHA256SUMS
```

Qualquer linha diferente de `OK` indica arquivo ausente, alterado ou corrompido.
Extraia novamente a entrega completa. Não copie somente os scripts para outra
pasta, pois eles validam o projeto ao qual pertencem.

## Falha nos autotestes GTK/Qt

Os empacotadores usam Xvfb e limite de 20 segundos. Procure antes da mensagem
final a saída de compilação ou do servidor virtual. Confirme as dependências.

Debian:

```bash
dpkg -s libgtk-4-dev qt6-base-dev qt6-qpa-plugins xvfb
```

Arch Linux:

```bash
pacman -Q gtk4 qt6-base xorg-server-xvfb
```

## A GUI automática escolheu o toolkit inesperado

Force a interface:

```bash
fix-names-gui --gtk
fix-names-gui --qt
```

O seletor usa `XDG_CURRENT_DESKTOP` e `DESKTOP_SESSION`; KDE/Plasma/LXQt
preferem Qt, e os demais ambientes preferem GTK.

## Diagnóstico mínimo para relatar uma falha

Registre sem usar root:

```bash
fix-names --version
command -v fix-names
readlink -f "$(command -v fix-names)"
printf 'LANG=%s\n' "${LANG-}"
uname -a
```

Para uma falha de pacote, inclua também a saída completa do gerador da
distribuição e o nome do arquivo produzido em `pacotes/debian/` ou
`pacotes/archlinux/`.
