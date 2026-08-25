# fix-names 2.1.7

`fix-names` é um renomeador em massa nativo para Linux, escrito em C++17 e
projetado para alterar nomes com pré-visualização, detecção de conflitos e sem
sobrescrever arquivos existentes. O programa trabalha sem privilégios
administrativos e compartilha o mesmo núcleo entre quatro entradas:

- `fix-names`: CLI e interface ncurses;
- `fix-names-gtk`: GUI GTK 4, indicada para GNOME, XFCE, Cinnamon e MATE;
- `fix-names-qt`: GUI Qt 6 Widgets, indicada para KDE Plasma e LXQt;
- `fix-names-gui`: escolhe automaticamente GTK ou Qt conforme o ambiente.

As interfaces não aplicam estilos próprios. GTK e Qt carregam o tema, a
paleta, as fontes e as caixas de arquivo configuradas no sistema.

## Estado do projeto

| Item | Estado |
| --- | --- |
| Versão atual | `2.1.7` |
| Debian | programa, pacote `.deb`, instalação e atualização testados |
| Arch Linux | correção PIC/PIE para LTO incluída; pacote validado automaticamente em ambiente Arch |
| Linguagem | C++17 e Shell POSIX |
| Interfaces | CLI/ncurses, GTK 4, Qt 6 e seletor automático |
| Licença | [GNU GPL v3](LICENSE) |

No Arch Linux, a versão `2.1.7` uniformiza `-fPIC` em todas as unidades C++ e
vincula os executáveis explicitamente como PIE. Isso corrige a falha da
`2.1.6`, na qual o LTO do `makepkg` combinava `core.o` sem PIC com a unidade Qt
e produzia `copy relocation` contra o símbolo protegido de `QWidget`. O script
Arch agora também rejeita binários que não sejam PIE ou contenham `TEXTREL`.

## Versões publicadas

O histórico preserva uma revisão independente para cada versão, de `2.0.0` a
`2.1.7`. Os links, hashes dos pacotes-fonte originais e o estado de cada
entrega estão em [`VERSOES.md`](VERSOES.md). O histórico funcional resumido
continua em [`CHANGELOG.md`](CHANGELOG.md).

## Documentação completa

Este README é uma apresentação rápida. A documentação oficial da versão
`2.1.7` começa em [`DOCUMENTACAO.md`](DOCUMENTACAO.md) e está dividida por
assunto para facilitar a consulta:

- [`docs/MANUAL_DO_USUARIO.md`](docs/MANUAL_DO_USUARIO.md): instalação,
  primeiros passos, transformações e fluxos recomendados;
- [`docs/REFERENCIA_CLI.md`](docs/REFERENCIA_CLI.md): sintaxe, todas as flags,
  combinações, códigos de saída e exemplos;
- [`docs/INTERFACES.md`](docs/INTERFACES.md): CLI, ncurses, GTK 4, Qt 6,
  seletor gráfico, barra de progresso e equivalência de controles;
- [`docs/ARQUITETURA_E_SEGURANCA.md`](docs/ARQUITETURA_E_SEGURANCA.md): desenho
  interno, algoritmo de renomeação, conflitos, reversão e limites;
- [`docs/REFERENCIA_DO_NUCLEO.md`](docs/REFERENCIA_DO_NUCLEO.md): tipos e API
  pública de `src/core.hpp` para manutenção do código;
- [`docs/COMPILACAO_E_EMPACOTAMENTO.md`](docs/COMPILACAO_E_EMPACOTAMENTO.md):
  Makefile, dependências, `.deb`, `.pkg.tar.zst` e arquivos instalados;
- [`docs/DESENVOLVIMENTO_E_TESTES.md`](docs/DESENVOLVIMENTO_E_TESTES.md):
  organização dos fontes, testes, autotestes gráficos e critérios de entrega;
- [`docs/SOLUCAO_DE_PROBLEMAS.md`](docs/SOLUCAO_DE_PROBLEMAS.md): diagnóstico
  das falhas conhecidas e comandos de verificação.

O histórico consolidado está em [`CHANGELOG.md`](CHANGELOG.md). A página de
manual instalada pode ser consultada com `man fix-names`.

## Correção da versão 2.1.7

- compila `core.o`, ncurses, GTK, Qt e testes com `-fPIC` consistente;
- vincula todos os executáveis explicitamente com `-fPIC -pie`;
- mantém o LTO do `makepkg` ativo no pacote Arch, em vez de ocultar a
  incompatibilidade com `!lto`;
- valida com `readelf` os binários compilados e os extraídos do pacote;
- falha antes da instalação se algum ELF não for PIE ou contiver `TEXTREL`;
- executa em automação o ciclo completo: compilar, testar, criar o
  `.pkg.tar.zst`, instalar com pacman e confirmar a versão.

## Correções preservadas da versão 2.1.6

- reinstala explicitamente o pacote local mesmo se a mesma revisão já estiver
  registrada pelo gerenciador da distribuição;
- compara a versão declarada pelo pacote com a versão realmente embutida no
  executável instalado em `/usr/bin/fix-names`;
- detecta instalações manuais antigas em `/usr/local` que ocultariam o pacote
  novo e as move para um backup recuperável dentro da pasta de destino;
- confirma ao final tanto a versão registrada como o caminho resolvido pelo
  comando `fix-names`;
- o seletor gráfico e o arquivo `.desktop` usam os executáveis da mesma raiz,
  impedindo a mistura entre versões diferentes.

## Correção preservada da versão 2.1.5

A interface Qt agora qualifica as mensagens do núcleo como `fixnames::tr()`.
Isso evita a colisão com a função estática `QMainWindow::tr(const char *, ...)`
herdada pelo Qt. Na versão anterior, o compilador encontrava a função do Qt
dentro de `MainWindow` e tentava converter o enum `fixnames::Language` em
`const char *`, interrompendo a criação do executável `fix-names-qt`.

Todo o restante do programa foi preservado, inclusive o progresso real nas
quatro interfaces e os scripts separados que criam, validam, instalam e mantêm
os pacotes em `pacotes/debian/` e `pacotes/archlinux/`.

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
pacotes/archlinux/fix-names-2.1.7-1-x86_64.pkg.tar.zst
```

O arquivo permanece na pasta `pacotes` e pode ser reinstalado posteriormente:

```bash
sudo pacman -U pacotes/archlinux/fix-names-2.1.7-1-x86_64.pkg.tar.zst
```

## Instalação no Debian

```bash
chmod +x compilar_instalar_debian.sh
./compilar_instalar_debian.sh
```

O script compila, testa, calcula automaticamente as dependências ELF, cria e
instala automaticamente um pacote semelhante a:

```text
pacotes/debian/fix-names_2.1.7-1_amd64.deb
```

Para reinstalar o arquivo já criado:

```bash
sudo apt install --reinstall ./pacotes/debian/fix-names_2.1.7-1_amd64.deb
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
.github/workflows/archlinux.yml  valida o pacote em Arch Linux
```
