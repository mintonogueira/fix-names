# Documentação oficial do fix-names 2.1.6

Esta documentação descreve a versão funcional `2.1.6` do **fix-names**, um
renomeador em massa nativo para Linux. O texto foi confrontado com os fontes,
os testes, o Makefile e os empacotadores da própria versão.

## Escopo do projeto

O programa altera nomes de entradas do sistema de arquivos sem modificar o
conteúdo dos arquivos. As mesmas regras são oferecidas por quatro pontos de
entrada:

| Comando | Interface | Uso principal |
| --- | --- | --- |
| `fix-names` | CLI ou ncurses | automação em Shell e uso no terminal |
| `fix-names-gtk` | GTK 4 | GNOME, XFCE, Cinnamon, MATE e outros ambientes GTK |
| `fix-names-qt` | Qt 6 Widgets | KDE Plasma, LXQt e outros ambientes Qt |
| `fix-names-gui` | seletor POSIX | escolhe GTK ou Qt conforme o desktop |

O núcleo compartilhado pode:

- converter para maiúsculas, minúsculas ou inicial de cada palavra;
- remover acentos e sinais diacríticos latinos;
- substituir espaços e sublinhados por um caractere UTF-8 escolhido;
- localizar e substituir texto literal;
- inserir ou sobrescrever texto por posição em caracteres Unicode;
- trabalhar apenas no nível atual ou recursivamente;
- proteger extensões ou incluí-las nas transformações;
- excluir arquivos, links, pastas e árvores inteiras;
- simular a operação antes de alterar nomes;
- relatar progresso real, conflitos, erros e estatísticas finais.

## Índice

1. [Manual do usuário](docs/MANUAL_DO_USUARIO.md)
2. [Referência da linha de comando](docs/REFERENCIA_CLI.md)
3. [Interfaces e equivalência de controles](docs/INTERFACES.md)
4. [Arquitetura e segurança](docs/ARQUITETURA_E_SEGURANCA.md)
5. [Referência do núcleo C++](docs/REFERENCIA_DO_NUCLEO.md)
6. [Compilação e empacotamento](docs/COMPILACAO_E_EMPACOTAMENTO.md)
7. [Desenvolvimento e testes](docs/DESENVOLVIMENTO_E_TESTES.md)
8. [Solução de problemas](docs/SOLUCAO_DE_PROBLEMAS.md)
9. [Histórico de versões](CHANGELOG.md)
10. [Referências e hashes das versões publicadas](VERSOES.md)

## Regras fundamentais

Estas regras valem igualmente para CLI, ncurses, GTK e Qt:

1. o programa nunca deve ser executado como `root`;
2. nenhuma transformação é habilitada por padrão;
3. a extensão fica protegida por padrão;
4. a recursão fica desativada por padrão;
5. o diretório-base escolhido não tem o próprio nome alterado;
6. links simbólicos podem ter o nome alterado, mas nunca são seguidos;
7. um destino existente nunca é sobrescrito;
8. colisões preservam todos os nomes envolvidos e tornam a execução falha;
9. o diretório raiz `/` não pode ser usado como alvo;
10. a pré-visualização percorre a árvore e produz o plano sem renomear nada.

## Ordem das transformações

Quando várias funções são habilitadas, a ordem é fixa:

1. remover acentos;
2. localizar e substituir;
3. inserir ou sobrescrever;
4. substituir espaços;
5. substituir sublinhados;
6. converter a capitalização.

Em arquivos e links, a extensão protegida é separada antes da primeira etapa e
recolocada byte por byte depois da última. Em diretórios, o nome inteiro é
processado porque diretórios não possuem uma extensão protegida pelo núcleo.

## Estado desta versão

- Programa e instalador Debian: testados e confirmados como funcionais.
- Atualização/reinstalação Debian: corrigida e confirmada na versão `2.1.6`.
- A colisão entre `fixnames::tr()` e `QMainWindow::tr()` foi corrigida na
  versão `2.1.5`.
- Barra de progresso: ativa nas quatro interfaces.
- Empacotador Arch Linux: implementado e validado estruturalmente, mas o teste
  real da versão `2.1.6` falhou na vinculação de `fix-names-qt` com LTO. O
  suporte Arch permanece pendente de uma nova correção e nova validação.

## Plataforma e compatibilidade

O projeto é específico para Linux. A segurança de renomeação depende de
`renameat2(RENAME_NOREPLACE)`, e a proteção automática do executável consulta
`/proc/self/exe`. Os empacotadores oficiais desta entrega são destinados a
Debian/derivados compatíveis e Arch Linux/derivados compatíveis.

## Idiomas

As mensagens e interfaces são exibidas em português do Brasil quando
`LC_ALL`, `LC_MESSAGES` ou `LANG` começa com `pt` (nessa ordem de prioridade).
Em qualquer outro caso, o programa usa inglês.
