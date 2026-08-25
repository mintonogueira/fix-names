# Interfaces do programa

## Paridade funcional

Todas as interfaces constroem a mesma estrutura `RenamerOptions` e chamam
`run_renamer()`. Não existe uma implementação de renomeação separada em GTK ou
Qt. Por isso, uma mesma configuração produz a mesma ordem de transformações,
os mesmos conflitos e o mesmo resumo.

| Função | CLI | ncurses | GTK 4 | Qt 6 |
| --- | :---: | :---: | :---: | :---: |
| maiúsculas/minúsculas/iniciais | sim | sim | sim | sim |
| remoção de acentos | sim | sim | sim | sim |
| espaços e sublinhados | sim | sim | sim | sim |
| localizar/substituir | sim | sim | sim | sim |
| inserir/sobrescrever | sim | sim | sim | sim |
| recursão opcional | sim | sim | sim | sim |
| proteção de extensão | sim | sim | sim | sim |
| lista de exclusões | sim | sim | sim | sim |
| pré-visualização | `--dry-run` | sim | sim | sim |
| confirmação antes de aplicar | sim | sim | sim | sim |
| progresso real | sim | sim | sim | sim |
| relatório detalhado | sim | sim | sim | sim |

## CLI

O executável `fix-names` escolhe o modo pelo número e tipo de argumentos:

- sem argumentos: abre ncurses;
- com flags sem `--interactive`: executa a CLI;
- com `--interactive`: abre ncurses com a configuração inicial recebida.

A barra possui 30 células e mostra `percentual (concluídos/total)`. O relatório
só é impresso depois da barra para impedir sobreposição visual.

## ncurses

Abra com:

```bash
fix-names
```

Navegação principal:

- `↑` e `↓`: mover a seleção;
- `Enter`: editar a opção ou executar a ação selecionada;
- `Q`: sair;
- o navegador permite entrar em diretórios, voltar ao pai e escolher o caminho.

O menu registra quinze linhas:

1. pasta-alvo;
2. capitalização;
3. remoção de acentos;
4. substituição de espaços;
5. substituição de sublinhados;
6. localizar/substituir;
7. inserir/sobrescrever;
8. aplicação recursiva;
9. alteração de extensões;
10. adicionar item à exclusão;
11. limpar exclusões;
12. pré-visualizar;
13. aplicar alterações;
14. ajuda;
15. sair.

Durante uma operação, a tela exibe título, percentual, contador real e barra.
Depois, abre uma tela rolável com mensagens e resumo.

## GTK 4

Abra com:

```bash
fix-names-gtk [CAMINHO_INICIAL]
```

A janela usa widgets GTK 4 e o tema nativo do ambiente. Seus componentes são:

- campo de pasta e botão `Navegar...`;
- seletor de capitalização;
- caixas para acentos, espaços, sublinhados e localizar/substituir;
- seletor `Desativado`, `Inserir` ou `Sobrescrever` com texto, posição e
  contagem a partir do fim;
- caixas de recursão e inclusão de extensões;
- lista de exclusões com adicionar/remover;
- botões `Pré-visualizar` e `Aplicar alterações`;
- barra percentual;
- relatório monoespaçado somente para leitura.

Ao aplicar, uma caixa de confirmação avisa que destinos existentes não serão
sobrescritos. Durante simulação ou aplicação, todos os controles mutáveis são
desabilitados e o fechamento da janela é recusado. A barra e o relatório
continuam sendo redesenhados.

## Qt 6 Widgets

Abra com:

```bash
fix-names-qt [CAMINHO_INICIAL]
```

A interface Qt oferece o mesmo conjunto de controles da GTK. Usa tema, paleta,
fontes e diálogo de arquivos fornecidos pelo Qt/desktop. A posição da função
inserir/sobrescrever aceita valores de `0` a `1.000.000` na interface visual;
o núcleo ainda valida e ajusta a posição ao comprimento real do nome.

Durante a operação, o cursor de espera é ativado, eventos de entrada do usuário
são excluídos, os controles são bloqueados e `closeEvent()` ignora pedidos de
fechamento até a conclusão.

## Seletor automático

O script `fix-names-gui` procura os executáveis na mesma pasta em que ele está,
evitando misturar `/usr/bin` com instalações antigas de `/usr/local/bin`.

Seleção padrão:

- KDE, Plasma ou LXQt detectado em `XDG_CURRENT_DESKTOP`/`DESKTOP_SESSION`: Qt;
- outros ambientes: GTK;
- se a escolha preferida não existir: tenta a outra GUI;
- se nenhuma existir: encerra com erro.

Escolha explícita:

```bash
fix-names-gui --gtk [CAMINHO_INICIAL]
fix-names-gui --qt [CAMINHO_INICIAL]
```

## Integração com o menu

`data/fix-names.desktop` instala a aplicação na categoria Utilitários/Ferramentas
de arquivos, aceita diretórios e fornece ações separadas para GTK e Qt. O menu
chama caminhos absolutos em `/usr/bin` para impedir que um binário antigo em
`/usr/local/bin` seja aberto no lugar do pacote atual.

## Idioma

A escolha de idioma é automática e igual em todas as interfaces:

1. `LC_ALL`;
2. `LC_MESSAGES`;
3. `LANG`;
4. português se o valor começa com `pt`; inglês nos demais casos.
