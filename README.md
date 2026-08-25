# fix-names 2.1.4

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
com percentual e contador `concluídos/total`. Enquanto uma requisição está em
andamento, as duas GUIs bloqueiam todos os controles e recusam o fechamento da
janela; isso impede uma segunda operação e mantém o estado da barra válido até
o relatório final.

## Instalação automática

O caminho mais simples é executar o instalador principal na raiz do projeto:

```bash
chmod +x instalar.sh
./instalar.sh
```

Ele detecta Debian ou Arch Linux e chama o empacotador apropriado que está na
raiz do projeto. Cada empacotador confere por conta própria se a entrega contém
todos os arquivos obrigatórios. O processo sempre cria o pacote binário nativo
antes de instalá-lo. Execute como usuário comum; `sudo` será usado apenas para
dependências e para a instalação final pelo gerenciador da distribuição.

O arquivo `SHA256SUMS` cobre todos os componentes da entrega. A verificação é
executada automaticamente antes da compilação e também pode ser repetida com:

```bash
sha256sum -c SHA256SUMS
```

## Instalação no Arch Linux

```bash
chmod +x compilar_instalar_arch.sh
./compilar_instalar_arch.sh
```

O script compila, testa, cria e instala automaticamente um pacote nativo
semelhante a:

```text
pacotes/archlinux/fix-names-2.1.4-1-x86_64.pkg.tar.zst
```

O arquivo permanece na pasta `pacotes` e pode ser reinstalado posteriormente:

```bash
sudo pacman -U pacotes/archlinux/fix-names-2.1.4-1-x86_64.pkg.tar.zst
```

## Instalação no Debian

```bash
chmod +x compilar_instalar_debian.sh
./compilar_instalar_debian.sh
```

O script compila, testa, calcula automaticamente as dependências ELF, cria e
instala automaticamente um pacote semelhante a:

```text
pacotes/debian/fix-names_2.1.4-1_amd64.deb
```

Para reinstalar o arquivo já criado:

```bash
sudo apt install ./pacotes/debian/fix-names_2.1.4-1_amd64.deb
```

Os dois instaladores devem ser executados por um usuário comum com `sudo`.
Eles instalam dependências, compilam, executam testes, validam GTK/Qt em uma
tela virtual, geram o pacote nativo na pasta exclusiva da distribuição e só
então pedem ao gerenciador que instale exatamente o arquivo validado em `/usr`.

Os destinos permanecem disponíveis depois da instalação:

```text
pacotes/debian/       pacotes .deb
pacotes/archlinux/    pacotes .pkg.tar.zst
```

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
compilar_instalar_debian.sh   gerador e instalador Debian autossuficiente
compilar_instalar_arch.sh     gerador e instalador Arch autossuficiente
packaging/arch/PKGBUILD.in    modelo validado para o makepkg
packaging/debian/control.in   metadados do pacote .deb
instalar.sh                   detecta a distribuição e inicia a instalação
CONTEUDO_DO_PACOTE.txt        manifesto legível da entrega
SHA256SUMS                    integridade de todos os componentes
```
