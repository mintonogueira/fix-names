# fix-names 2.1.0

`fix-names` é um renomeador em massa nativo para Linux, escrito em C++17 e
projetado para trabalhar sem privilégios administrativos. O mesmo núcleo é
usado por quatro entradas independentes:

- `fix-names`: CLI e interface ncurses;
- `fix-names-gtk`: GUI GTK 4, indicada para GNOME, XFCE, Cinnamon e MATE;
- `fix-names-qt`: GUI Qt 6 Widgets, indicada para KDE Plasma e LXQt;
- `fix-names-gui`: escolhe automaticamente GTK ou Qt conforme o ambiente.

As interfaces não aplicam estilos próprios. GTK e Qt carregam o tema, a
paleta, as fontes e as caixas de arquivo configuradas no sistema.

## Garantias de segurança

- O aplicativo recusa imediatamente qualquer execução com `EUID == 0`.
- O diretório raiz `/` é bloqueado como alvo.
- Links simbólicos são renomeados, mas seus destinos nunca são seguidos.
- Destinos existentes nunca são sobrescritos.
- A operação usa `renameat2(RENAME_NOREPLACE)`, de forma atômica.
- Trocas e cadeias de nomes usam uma fase temporária e reversão em caso de erro.
- O próprio executável é incluído automaticamente na lista de proteção.
- O diretório-base escolhido é preservado; somente seu conteúdo é processado.
- Extensões de arquivos e links ficam excluídas das alterações por padrão.
- A aplicação recursiva fica desativada por padrão.
- Nenhuma transformação é ativada implicitamente.

## Transformações e ordem

Quando várias funções são combinadas, o núcleo usa sempre esta ordem:

1. remoção opcional de acentos;
2. localizar e substituir;
3. inserir ou sobrescrever;
4. substituição de espaços e, separadamente, de sublinhados;
5. conversão de maiúsculas/minúsculas.

A extensão protegida é separada antes dessas etapas e recolocada, byte por
byte, somente no final. Por exemplo, `.TAR.GZ`, `.JpG` e `.DÁT` permanecem
exatamente iguais.

## Flags da CLI

```text
-u, --uppercase               tudo maiúsculo
-l, --lowercase               tudo minúsculo
-c, --capitalize              inicial de cada palavra em maiúscula
-a, --remove-accents          remove acentos latinos
-s, --spaces CARACTERE        substitui espaços por um caractere UTF-8
    --underscores CARACTERE   substitui _ por um caractere UTF-8
-f, --find TEXTO              texto literal a localizar
-p, --replace TEXTO           texto substituto
    --insert TEXTO            insere na posição indicada
    --overwrite TEXTO         sobrescreve na posição indicada
    --position N              posição em caracteres (padrão: 0)
    --from-end                conta a posição a partir do fim
-r, --recursive               inclui subpastas e seus arquivos
    --include-extension       permite alterar extensões
-x, --exclude CAMINHO         adiciona uma exclusão; pode ser repetida
    --exclude-from ARQUIVO    lê uma exclusão por linha
-n, --dry-run                 simula sem alterar arquivos
-y, --yes                     executa sem confirmação
-i, --interactive             abre ncurses
-h, --help                    mostra a ajuda
-V, --version                 mostra a versão
```

As três formas de capitalização são mutuamente exclusivas. `--find` e
`--replace` devem ser usados juntos. `--insert` e `--overwrite` também são
mutuamente exclusivos.

## Exemplos

Simular nomes minúsculos, sem acentos e com hífen no lugar de espaço:

```bash
fix-names --lowercase --remove-accents --spaces=- --dry-run "/caminho"
```

Aplicar recursivamente e preservar dois arquivos:

```bash
fix-names --uppercase --recursive \
  --exclude "não alterar.txt" \
  --exclude "subpasta/manter.jpg" \
  --yes "/caminho"
```

Localizar e substituir:

```bash
fix-names --find "antigo" --replace "novo" --dry-run "/caminho"
```

Inserir um prefixo, como no modo Inserir/Sobrescrever do Thunar:

```bash
fix-names --insert "2026-" --position 0 --yes "/caminho"
```

## Interfaces

```bash
fix-names              # ncurses
fix-names-gui          # GTK ou Qt automaticamente
fix-names-gui --gtk    # força GTK
fix-names-gui --qt     # força Qt
```

Ncurses, GTK e Qt oferecem navegador de pastas, controles para todas as
transformações, recursão opcional, proteção de extensões, lista de arquivos
excluídos, pré-visualização e confirmação antes da aplicação. CLI, ncurses,
GTK e Qt exibem uma barra de progresso baseada na quantidade real de itens,
com percentual e contador `concluídos/total`.

## Instalação no Arch Linux

```bash
chmod +x scripts/compilar_instalar_arch.sh
./scripts/compilar_instalar_arch.sh
```

O script compila, testa, cria e instala um pacote nativo semelhante a:

```text
pacotes/fix-names-2.1.0-1-x86_64.pkg.tar.zst
```

O arquivo permanece na pasta `pacotes` e pode ser reinstalado posteriormente:

```bash
sudo pacman -U pacotes/fix-names-2.1.0-1-x86_64.pkg.tar.zst
```

## Instalação no Debian

```bash
chmod +x scripts/compilar_instalar_debian.sh
./scripts/compilar_instalar_debian.sh
```

O script compila, testa, calcula automaticamente as dependências ELF, cria e
instala um pacote semelhante a:

```text
pacotes/fix-names_2.1.0-1_amd64.deb
```

Para reinstalar o arquivo já criado:

```bash
sudo apt install ./pacotes/fix-names_2.1.0-1_amd64.deb
```

Os dois instaladores devem ser executados por um usuário comum com `sudo`.
Eles instalam dependências, compilam, executam testes, validam GTK/Qt em uma
tela virtual, geram o pacote nativo e só então pedem ao gerenciador da
distribuição que instale esse pacote em `/usr`.

## Compilação manual

```bash
make -j"$(getconf _NPROCESSORS_ONLN)" all
make test
sudo make PREFIX=/usr install
```

## Estrutura do projeto

```text
src/core.*                    núcleo e segurança
src/cli_main.cpp              flags e CLI
src/ncurses_ui.*              interface textual e navegador
src/gtk_main.cpp              GUI GTK 4 independente
src/qt_main.cpp               GUI Qt 6 independente
tests/test_core.cpp           testes puros e testes reais de arquivos
assets/fix-names.png          ícone fornecido para o projeto
data/fix-names-gui            seletor automático de GUI
data/fix-names.desktop        entrada do menu de aplicativos
scripts/compilar_instalar_*   instaladores Arch e Debian
packaging/arch/PKGBUILD.in    modelo validado para o makepkg
packaging/debian/control.in   metadados do pacote .deb
```
