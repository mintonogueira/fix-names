# Referência do núcleo C++

Esta referência descreve a API pública definida em `src/core.hpp`. Ela é
voltada à manutenção do projeto e à criação de novas interfaces.

## Namespace e versão

Toda a API pertence a `namespace fixnames`. A constante:

```cpp
inline constexpr const char *VERSION = "2.1.6";
```

é a fonte usada pelo programa e pelos scripts para identificar a versão.

## Enumerações

### `Language`

| Valor | Significado |
| --- | --- |
| `English` | mensagens em inglês |
| `PortugueseBrazil` | mensagens em português do Brasil |

### `CaseMode`

| Valor | Significado |
| --- | --- |
| `None` | não altera capitalização |
| `Uppercase` | converte para maiúsculas |
| `Lowercase` | converte para minúsculas |
| `CapitalizeWords` | inicial de palavra maiúscula; restante minúsculo |

### `InsertMode`

| Valor | Significado |
| --- | --- |
| `None` | função desativada |
| `Insert` | insere sem remover caracteres existentes |
| `Overwrite` | insere e remove a mesma quantidade de caracteres seguintes |

## `RenamerOptions`

| Campo | Tipo | Padrão | Função |
| --- | --- | --- | --- |
| `target` | `std::filesystem::path` | vazio | caminho a processar |
| `case_mode` | `CaseMode` | `None` | modo de capitalização |
| `remove_accents` | `bool` | `false` | remove diacríticos |
| `replace_spaces` | `bool` | `false` | ativa substituição de espaços |
| `space_replacement` | `std::string` | vazio | um caractere UTF-8 substituto |
| `replace_underscores` | `bool` | `false` | ativa substituição de `_` |
| `underscore_replacement` | `std::string` | vazio | um caractere UTF-8 substituto |
| `find_replace_enabled` | `bool` | `false` | ativa busca literal |
| `find_text` | `std::string` | vazio | trecho procurado |
| `replacement_text` | `std::string` | vazio | trecho substituto |
| `insert_mode` | `InsertMode` | `None` | inserir ou sobrescrever |
| `insert_text` | `std::string` | vazio | texto da operação posicional |
| `insert_position` | `std::size_t` | `0` | posição em pontos Unicode |
| `position_from_end` | `bool` | `false` | conta posição pela direita |
| `include_extension` | `bool` | `false` | processa também extensões |
| `recursive` | `bool` | `false` | percorre subpastas |
| `dry_run` | `bool` | `false` | cria plano sem renomear |
| `exclusions` | vetor de caminhos | vazio | itens/árvores protegidos |

## Resultado

`RunStats` mantém seis contadores: `planned`, `renamed`, `unchanged`,
`excluded`, `conflicts` e `errors`.

`RunResult` reúne:

- `success`: verdadeiro somente quando não há erros nem conflitos;
- `stats`: os contadores da requisição;
- `messages`: relatório ordenado da operação.

## Callbacks

```cpp
using LogCallback = std::function<void(const std::string &)>;
using ProgressCallback = std::function<void(std::size_t, std::size_t)>;
```

O callback de log recebe cada mensagem quando ela é adicionada ao resultado.
O callback de progresso recebe quantidade concluída e total. Uma interface
deve usar `progress_percentage()` para manter o mesmo cálculo das demais.

## Funções públicas

### `int progress_percentage(completed, total) noexcept`

Retorna percentual inteiro de `0` a `100`, limita `completed` a `total` e
retorna `100` quando `total == 0`. O algoritmo evita estouro aritmético.

### `Language detect_language()`

Consulta `LC_ALL`, `LC_MESSAGES` e `LANG`, nessa ordem. Prefixo `pt` seleciona
português; qualquer outro valor seleciona inglês.

### `std::string tr(language, pt_br, english)`

Escolhe uma das duas mensagens sem dependência de toolkit.

### `bool running_as_root()`

Retorna `true` quando `geteuid() == 0`.

### `bool validate_options(options, error, language)`

Valida sem percorrer o sistema de arquivos. Exige alvo e ao menos uma
transformação; valida substitutos de um caractere, textos vazios, `/` e NUL.
Em falha, grava a explicação localizada em `error`.

### `std::string transform_name(name, is_directory, options)`

Função pura que aplica a ordem oficial de transformações. Não verifica colisão
com o sistema de arquivos. `is_directory` informa se a proteção de extensão
deve ser desconsiderada.

### `RunResult run_renamer(options, language, log, progress)`

Valida novamente as opções, recusa root e `/`, normaliza exclusões, conta os
itens, simula ou executa o lote e devolve relatório/estatísticas. Nunca segue
links nem sobrescreve destinos.

### `std::string display_path(path)`

Converte um caminho para relatório escapando bytes de controle. O caminho real
do sistema de arquivos não é alterado.

## Contrato para uma nova interface

1. Detectar idioma com `detect_language()`.
2. Recusar `running_as_root()` antes de criar a interface.
3. Preencher `RenamerOptions` sem ativar transformações implícitas.
4. Oferecer simulação por `dry_run=true`.
5. Validar ou deixar `run_renamer()` produzir a mensagem localizada.
6. Atualizar progresso exclusivamente pelos valores do callback.
7. Impedir uma segunda operação enquanto a primeira estiver ativa.
8. Apresentar todas as mensagens e o resumo final de `RunResult`.
