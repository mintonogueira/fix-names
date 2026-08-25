# Referência da linha de comando

## Sintaxe

```text
fix-names
fix-names [OPÇÕES] [CAMINHO]
```

Sem argumentos, o comando abre a interface ncurses. Com flags, funciona como
ferramenta de linha de comando. Se o caminho for omitido, usa o diretório atual
`.`. Somente um caminho pode ser informado por execução.

## Transformações

| Forma curta | Forma longa | Argumento | Efeito |
| --- | --- | --- | --- |
| `-u` | `--uppercase` | — | converte para maiúsculas |
| `-l` | `--lowercase` | — | converte para minúsculas |
| `-c` | `--capitalize` | — | inicial de cada palavra em maiúscula |
| `-a` | `--remove-accents` | — | remove diacríticos latinos |
| `-s` | `--spaces` | caractere | substitui espaços ASCII |
| — | `--underscores` | caractere | substitui `_` |
| `-f` | `--find` | texto | texto literal a localizar |
| `-p` | `--replace` | texto | substituto para todas as ocorrências |
| — | `--insert` | texto | insere texto na posição |
| — | `--overwrite` | texto | sobrescreve a partir da posição |
| — | `--position` | inteiro | posição em caracteres; padrão `0` |
| — | `--from-end` | — | conta a posição a partir do fim |

`--uppercase`, `--lowercase` e `--capitalize` são mutuamente exclusivos.
`--find` e `--replace` devem aparecer juntos. `--insert` e `--overwrite` são
mutuamente exclusivos.

## Escopo e segurança

| Forma curta | Forma longa | Argumento | Efeito |
| --- | --- | --- | --- |
| `-r` | `--recursive` | — | inclui subpastas e seus conteúdos |
| — | `--include-extension` | — | permite transformar extensões |
| `-x` | `--exclude` | caminho | adiciona uma exclusão; repetível |
| — | `--exclude-from` | arquivo | lê uma exclusão por linha |
| `-n` | `--dry-run` | — | simula sem renomear |
| `-y` | `--yes` | — | aplica sem confirmação interativa |
| `-i` | `--interactive` | — | abre ncurses com as opções já lidas |
| `-h` | `--help` | — | mostra a ajuda no idioma detectado |
| `-V` | `--version` | — | mostra a versão incorporada |
| — | `--` | — | encerra a interpretação de opções |

O programa verifica `EUID == 0` antes inclusive de `--help` e `--version`.
Consulte esses comandos com uma conta comum.

## Formas de passar argumentos

Use `=` quando o argumento puder começar com hífen:

```bash
fix-names --spaces=- --lowercase --dry-run .
```

Use `--` antes de um caminho cujo nome começa com hífen:

```bash
fix-names --lowercase --dry-run -- ./-pasta
```

Aspas preservam espaços e caracteres interpretados pelo Shell:

```bash
fix-names --find "Nome antigo" --replace "Nome novo" --dry-run "/Minha pasta"
```

## Interação e confirmação

- `--dry-run` executa imediatamente porque não altera nomes.
- Uma aplicação sem `--yes` pede confirmação em português ou inglês.
- Qualquer resposta diferente das formas afirmativas reconhecidas cancela a
  operação com sucesso, sem alteração.
- `--interactive` transfere o caminho e as transformações já configuradas para
  a interface ncurses, onde ainda podem ser editadas.

## Barra e fluxos de saída

A barra percentual é escrita em `stderr`; o relatório detalhado e o resumo são
escritos em `stdout` depois que a barra termina. Isso permite redirecionar o
relatório sem perder mensagens de progresso no terminal:

```bash
fix-names --lowercase --dry-run . > relatorio.txt
```

Para capturar tudo:

```bash
fix-names --lowercase --dry-run . > relatorio-completo.txt 2>&1
```

## Códigos de saída

| Código | Significado |
| --- | --- |
| `0` | sucesso, ajuda/versão exibida ou operação cancelada pelo usuário |
| `1` | erro operacional, conflito ou resultado do núcleo sem sucesso |
| `2` | erro de sintaxe, combinação inválida ou validação feita pela CLI |
| `77` | execução recusada porque o usuário efetivo é `root` |

## Exemplos completos

Converter apenas o nível atual:

```bash
fix-names --lowercase --dry-run "/dados/fotos"
```

Converter toda a árvore, sem tocar extensões:

```bash
fix-names --lowercase --remove-accents --spaces=- --recursive \
  --dry-run "/dados/fotos"
```

Alterar também as extensões:

```bash
fix-names --lowercase --include-extension --dry-run "/dados/fotos"
```

Substituir prefixos e aplicar sem pergunta:

```bash
fix-names --find "IMG_" --replace "foto-" --yes "/dados/fotos"
```

Inserir antes do último caractere da base:

```bash
fix-names --insert "-X" --position 1 --from-end --dry-run .
```

Iniciar ncurses já com recursão e minúsculas ativadas:

```bash
fix-names --interactive --lowercase --recursive "/dados/fotos"
```
