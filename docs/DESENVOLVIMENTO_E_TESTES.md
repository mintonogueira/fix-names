# Desenvolvimento e testes

## Estrutura dos fontes

| Caminho | Responsabilidade |
| --- | --- |
| `src/core.hpp` | tipos e contrato público do núcleo |
| `src/core.cpp` | Unicode, transformações, travessia e renomeação segura |
| `src/cli_main.cpp` | getopt, confirmação, barra CLI e ponto de entrada |
| `src/ncurses_ui.*` | TUI, navegador, formulários, progresso e relatórios |
| `src/gtk_main.cpp` | GUI GTK 4 |
| `src/qt_main.cpp` | GUI Qt 6 Widgets |
| `tests/test_core.cpp` | testes puros e de sistema de arquivos |
| `data/fix-names-gui` | seleção automática de GUI |
| `data/fix-names.desktop` | integração com menu |
| `data/fix-names.1` | página de manual |
| `packaging/*` | modelos dos pacotes nativos |

## Princípios de manutenção

- Regras de renomeação pertencem ao núcleo, não às interfaces.
- Uma nova opção deve ser adicionada a `RenamerOptions`, validada em
  `validate_options()` e aplicada em `transform_name()`.
- Todas as interfaces precisam expor a mesma capacidade ou documentar
  explicitamente a diferença.
- Toda mudança que altere nomes deve ganhar teste puro e, quando aplicável,
  teste real de sistema de arquivos.
- Nenhum caminho amplo deve ser removido por scripts de teste ou pacote.
- Os scripts Shell permanecem compatíveis com `/bin/sh` e incluem comentários
  sobre o passo a passo e a lógica.

## Testes automatizados do núcleo

Execute:

```bash
make test
```

Os testes puros cobrem:

- progresso no início, meio e máximo de `size_t` sem estouro;
- maiúsculas, minúsculas e inicial de palavras;
- acentos, espaços e sublinhados;
- preservação de extensão composta;
- busca/substituição de todas as ocorrências;
- inserir e sobrescrever por posição Unicode;
- alteração explícita de extensão;
- rejeição de substitutos com mais de um caractere.

Os testes reais criam somente uma árvore com prefixo
`/tmp/fix-names-tests-XXXXXX` e verificam recursão, exclusões, arquivos ocultos,
links simbólicos, extensões, conflitos, simulação e progresso monotônico.

Se o teste normal for executado como root, as operações de arquivo são
ignoradas e o bloqueio de root é testado. O macro interno
`FIX_NAMES_TEST_ALLOW_ROOT` existe apenas para ambientes isolados de teste.

## Autotestes gráficos

Os geradores Debian e Arch iniciam Xvfb em uma tela livre e executam:

```bash
./build/fix-names-gtk --self-test
./build/fix-names-qt --self-test
```

Cada GUI configura minúsculas e executa uma simulação no caminho atual. Assim,
o teste percorre janela, opções, núcleo, progresso, relatório e fechamento.
`timeout 20s` transforma travamento ou janela não encerrada em falha de pacote.

`--self-test` é um argumento interno de validação e não uma função cotidiana.

## Compilação rigorosa

O Makefile habilita C++17 e:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow
```

O objeto `core.o` é compartilhado pelos três executáveis, reduzindo o risco de
uma interface ser ligada a uma regra diferente.

## Critérios antes de uma nova entrega

1. Atualizar `VERSION` em `src/core.hpp` quando houver mudança de versão.
2. Atualizar página de manual, README, documentação e histórico.
3. Executar `make clean`, `make all` e `make test`.
4. Executar autotestes GTK e Qt.
5. Verificar sintaxe de todos os scripts com `sh -n`.
6. Gerar novamente `SHA256SUMS` sem listar o próprio manifesto.
7. Extrair o arquivo final em uma pasta nova.
8. Rodar `sha256sum -c SHA256SUMS` na árvore extraída.
9. Executar o gerador nativo na distribuição correspondente.
10. Conferir versão do gerenciador, binário em `/usr/bin` e resolução no PATH.

## Estado de validação conhecido

Na versão `2.1.6`, o programa e o fluxo Debian foram confirmados pelo usuário.
O teste real no Arch expôs uma inconsistência PIC/PIE durante o LTO do Qt. A
versão `2.1.7` corrige essa regressão, adiciona validação ELF com `readelf` e
executa o fluxo completo em uma automação baseada em Arch Linux.
