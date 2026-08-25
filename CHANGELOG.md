# Histórico do fix-names

> As versões `2.0.0` a `2.1.6` estão preservadas no histórico do repositório.
> Consulte [`VERSOES.md`](VERSOES.md) para referências e hashes dos arquivos
> originais.

## 2.1.6 — atualização e resolução de versão

- Força reinstalação do pacote local no Debian e no Arch.
- Confere versão registrada, versão do binário em `/usr/bin` e comando resolvido
  pelo `PATH` como verificações independentes.
- Detecta instalações antigas em `/usr/local`, preserva-as em backup e impede
  que ocultem os executáveis do pacote.
- Faz o seletor gráfico procurar GTK/Qt na mesma pasta do próprio seletor.
- Usa caminhos absolutos `/usr/bin` no arquivo `.desktop`.
- Situação conhecida posterior à entrega: o gerador Arch chegou à etapa de
  vinculação Qt, mas falhou com uma relocação protegida de `QWidget` ao usar as
  flags LTO do `makepkg`. O Debian foi testado e aprovado.

## 2.1.5 — correção da interface Qt

- Qualifica chamadas internas como `fixnames::tr()` para impedir colisão com
  `QObject::tr()`/`QMainWindow::tr()`.
- Mantém a validação de versão do binário dentro dos pacotes.

## 2.1.4 — empacotadores autossuficientes

- Move os geradores Debian e Arch para a raiz da entrega.
- Incorpora a validação que antes dependia de
  `scripts/verificar_projeto.sh`.
- Preserva destinos separados em `pacotes/debian/` e
  `pacotes/archlinux/`.

## 2.1.3 — destinos de pacote separados

- Reescreve os geradores nativos em fluxos separados.
- Adiciona barra de dez etapas aos instaladores.
- Valida identidade, versão, arquitetura e conteúdo antes de instalar.

## 2.1.2 — progresso e bloqueio de GUI

- Centraliza cálculo percentual protegido contra estouro.
- Bloqueia todos os controles GTK/Qt durante operações.
- Recusa fechamento das janelas enquanto callbacks ainda estão ativos.
- Amplia os autotestes gráficos para executar uma simulação real.

## 2.1.1 — integridade da entrega

- Adiciona manifesto, somas SHA-256 e verificação estrutural.
- Reúne núcleo C++17, CLI/ncurses, GTK, Qt, ícone, testes e empacotadores.

## 2.1.0 — arquitetura completa em C++17

- Introduz núcleo compartilhado entre quatro interfaces.
- Adiciona transformação por flags, exclusões, recursão opcional e proteção de
  extensões.
- Implementa renomeação atômica sem sobrescrita e detecção de colisões.

## 2.0.0 — base multinterface

- Consolida o núcleo C++17 e as interfaces CLI/ncurses, GTK 4 e Qt 6.
- Inclui ícone, entrada de menu, página de manual, testes e scripts iniciais
  para Debian e Arch Linux.
