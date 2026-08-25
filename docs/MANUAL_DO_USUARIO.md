# Manual do usuário

## 1. O que o fix-names faz

O **fix-names** renomeia arquivos, links simbólicos e pastas em lote. Ele não
edita conteúdo, permissões ou donos; sua operação é restrita ao nome das
entradas do sistema de arquivos.

Quando o alvo é uma pasta, o nome dessa pasta é preservado e o programa trata
seu conteúdo. Com recursão ativada, ele entra nas subpastas, processa os itens
mais profundos primeiro e só depois renomeia as próprias subpastas.

## 2. Instalação rápida

Extraia a entrega completa e entre na pasta do projeto:

```bash
tar -xzf fix-names-2.1.6-completo-documentado.tar.gz
cd fix-names-2.1.6
```

Execute como usuário comum. Não use `sudo` na frente do instalador:

```bash
./instalar.sh
```

O instalador detecta Debian ou Arch Linux, chama o gerador específico, instala
as dependências, compila, testa, cria o pacote nativo e o instala. `sudo` é
solicitado apenas nas etapas administrativas.

Para escolher diretamente a distribuição:

```bash
./compilar_instalar_debian.sh
./compilar_instalar_arch.sh
```

Os pacotes gerados permanecem em:

```text
pacotes/debian/
pacotes/archlinux/
```

## 3. Primeira utilização segura

Use primeiro a simulação. Este comando mostra o plano sem alterar arquivos:

```bash
fix-names --lowercase --remove-accents --spaces=- --dry-run \
  "/caminho/para/a/pasta"
```

Revise as linhas `[SIMULAÇÃO]` e o resumo. Para aplicar a mesma regra, retire
`--dry-run`; o programa pedirá confirmação:

```bash
fix-names --lowercase --remove-accents --spaces=- \
  "/caminho/para/a/pasta"
```

Use `--yes` somente quando a regra já tiver sido conferida:

```bash
fix-names --lowercase --remove-accents --spaces=- --yes \
  "/caminho/para/a/pasta"
```

## 4. Transformações

### Capitalização

- **Maiúsculas:** converte o texto processado para maiúsculas.
- **Minúsculas:** converte o texto processado para minúsculas.
- **Inicial de cada palavra:** coloca o primeiro caractere alfanumérico de cada
  palavra em maiúscula e os demais em minúscula.

Somente um desses modos pode estar ativo por vez.

### Remoção de acentos

Remove marcas combinantes e converte letras latinas precompostas para formas
sem diacríticos. Ligações conhecidas também são desdobradas, por exemplo
`æ -> ae`, `œ -> oe`, `ß -> ss` e `þ -> th`.

Os caracteres latinos convertidos pela tabela de remoção são produzidos em
minúsculas. Combine a função com um modo de capitalização quando quiser
uniformizar o resultado inteiro.

### Espaços e sublinhados

As duas funções são independentes. Cada substituto precisa ser exatamente um
caractere UTF-8 válido e não pode ser `/`.

```bash
fix-names --spaces=- --underscores=- --dry-run .
```

Somente o espaço ASCII comum (`U+0020`) e o sublinhado ASCII (`_`) são
substituídos. Outros espaços Unicode não são tratados como espaço comum.

### Localizar e substituir

A busca é literal, diferencia maiúsculas de minúsculas e substitui todas as
ocorrências. Não há interpretação de expressão regular.

```bash
fix-names --find "versao-antiga" --replace "versao-nova" --dry-run .
```

O texto de busca não pode ser vazio. O substituto pode ser vazio, permitindo
remover o trecho encontrado:

```bash
fix-names --find "-copia" --replace '' --dry-run .
```

### Inserir e sobrescrever

A posição é contada em caracteres Unicode, não em bytes UTF-8.

```bash
fix-names --insert "2026-" --position 0 --dry-run .
```

No modo **Inserir**, o texto existente é deslocado. No modo **Sobrescrever**,
a quantidade de caracteres ocupada pelo texto informado substitui caracteres
existentes a partir da posição.

Sem `--from-end`, uma posição além do comprimento é limitada ao fim. Com
`--from-end`, posição `0` significa o fim do nome, posição `1` significa antes
do último caractere, e uma posição maior que o comprimento volta ao início.

## 5. Extensões

Extensões são protegidas por padrão. O primeiro ponto que não seja o ponto
inicial do nome começa a extensão protegida.

| Nome | Base processada | Extensão protegida |
| --- | --- | --- |
| `foto.JpG` | `foto` | `.JpG` |
| `arquivo.TAR.GZ` | `arquivo` | `.TAR.GZ` |
| `.config.JpG` | `.config` | `.JpG` |
| `.bashrc` | `.bashrc` | nenhuma |

Use `--include-extension` ou o controle equivalente para processar o nome
inteiro, incluindo a extensão.

## 6. Recursão

Sem recursão, somente os itens imediatamente dentro da pasta escolhida são
tratados. Subpastas podem ter o próprio nome alterado, mas o conteúdo interno
fica intacto.

Com recursão, o conteúdo de todas as subpastas acessíveis é processado:

```bash
fix-names --lowercase --recursive --dry-run "/caminho"
```

Links para diretórios não são seguidos e, portanto, não introduzem ciclos de
travessia.

## 7. Exclusões

Uma exclusão pode ser absoluta ou relativa à pasta-alvo. Excluir uma pasta
protege também todo o conteúdo abaixo dela.

```bash
fix-names --lowercase --recursive \
  --exclude "manter.txt" \
  --exclude "subpasta/protegida" \
  --dry-run "/caminho"
```

Para listas maiores, use um arquivo com uma exclusão por linha:

```text
# Comentários começam com # na primeira coluna
manter.txt
subpasta/protegida
imagens/original.png
```

```bash
fix-names --lowercase --exclude-from exclusoes.txt --dry-run "/caminho"
```

Linhas vazias e linhas cujo primeiro caractere é `#` são ignoradas.

## 8. Relatório final

O resumo contém:

- `planejados`: nomes diferentes calculados antes da resolução final;
- `renomeados`: alterações concluídas em uma execução real;
- `inalterados`: itens cujo resultado é igual ao nome original;
- `ignorados`: itens protegidos pela lista ou pelo próprio programa;
- `conflitos`: itens preservados porque o destino é ambíguo ou já existe;
- `erros`: falhas de acesso, validação ou operação.

Uma simulação mantém `renomeados=0`, mesmo quando há itens planejados.

## 9. Regras de uso responsável

- Faça uma pré-visualização antes de cada regra nova.
- Comece sem recursão e só a habilite depois de conferir o primeiro nível.
- Mantenha extensões protegidas, salvo quando houver motivo explícito.
- Não altere o diretório enquanto uma operação estiver em andamento.
- Não execute o aplicativo nem seus instaladores integralmente como `root`.
- Mantenha backup dos dados importantes; o programa protege contra colisões,
  mas não substitui uma política de backup.
