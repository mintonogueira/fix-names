# Compilação, instalação e empacotamento

## Dependências de compilação

O projeto usa compilador C++17, `make`, `pkg-config`/`pkgconf`, ncurses wide
character, GTK 4 e Qt 6 Widgets. Os scripts também instalam utilitários de
empacotamento, banco de atalhos e Xvfb para os autotestes gráficos.

## Makefile

| Variável | Padrão | Uso |
| --- | --- | --- |
| `CXX` | `c++` | compilador |
| `PREFIX` | `/usr/local` | prefixo de instalação manual |
| `DESTDIR` | vazio | raiz temporária de pacote |
| `BUILD_DIR` | `build` | saída da compilação |
| `CXXFLAGS` | `-O2` + avisos | opções C++17 fornecidas pelo ambiente |
| `PIC_CXXFLAGS` | `-fPIC` | PIC obrigatório em todas as unidades C++ |
| `PIE_LDFLAGS` | `-fPIC -pie` | vínculo PIE de todos os executáveis |

Alvos:

```bash
make all                  # confere dependências e compila três binários
make test                 # compila e executa test-core
make PREFIX=/usr install  # instala os artefatos
make clean                # remove apenas resultados conhecidos de build/
```

Binários produzidos:

```text
build/fix-names
build/fix-names-gtk
build/fix-names-qt
build/test-core
```

## Arquivos instalados

Com `PREFIX=/usr`, a instalação contém:

```text
/usr/bin/fix-names
/usr/bin/fix-names-gui
/usr/bin/fix-names-gtk
/usr/bin/fix-names-qt
/usr/share/applications/fix-names.desktop
/usr/share/pixmaps/fix-names.png
/usr/share/man/man1/fix-names.1[.gz]
/usr/share/doc/fix-names/README.md
/usr/share/doc/fix-names/DOCUMENTACAO.md
/usr/share/doc/fix-names/CHANGELOG.md
/usr/share/doc/fix-names/VERSOES.md
/usr/share/doc/fix-names/docs/*.md
```

## Instalador principal

`instalar.sh` lê `/etc/os-release` e encaminha:

- `ID`/`ID_LIKE` Debian ou Ubuntu: `compilar_instalar_debian.sh`;
- `ID`/`ID_LIKE` Arch/Arch Linux: `compilar_instalar_arch.sh`;
- demais sistemas: encerra sem tentar adaptar silenciosamente.

Ele não recebe parâmetros e deve ser executado como usuário comum.

## Empacotador Debian

Dependências instaladas automaticamente:

```text
build-essential dpkg-dev pkg-config libncurses-dev libgtk-4-dev
qt6-base-dev qt6-qpa-plugins desktop-file-utils xvfb
```

Fluxo de dez etapas:

1. validar `sudo`;
2. atualizar o APT e instalar dependências;
3. limpar e compilar todas as interfaces;
4. executar testes do núcleo;
5. executar GTK e Qt em Xvfb com limite de 20 segundos;
6. montar uma raiz temporária com `DESTDIR`;
7. calcular dependências ELF com `dpkg-shlibdeps` e metadados;
8. criar o `.deb` em `pacotes/debian/`;
9. validar nome, versão, arquitetura, conteúdo e versão do binário interno;
10. forçar instalação do pacote local com APT e conferir o resultado.

Formato esperado:

```text
pacotes/debian/fix-names_2.1.7-1_ARQUITETURA.deb
```

O pacote inclui dependências ELF calculadas e acrescenta
`qt6-qpa-plugins`, carregado dinamicamente pelo Qt. O APT usa `--reinstall`
para atualizar os arquivos mesmo quando a mesma revisão já está registrada.

Após instalar, o script confere separadamente:

- `dpkg-query` registra `2.1.7-1`;
- `/usr/bin/fix-names --version` informa `2.1.7`;
- `/usr/bin/fix-names` pertence ao pacote;
- `command -v fix-names` resolve, após `readlink -f`, para `/usr/bin/fix-names`.

## Empacotador Arch Linux

> **Correção da versão 2.1.7:** todas as unidades são compiladas com `-fPIC`,
> os executáveis são vinculados como PIE e o `PKGBUILD` mantém LTO ativo. Isso
> corrige a `copy relocation` contra o símbolo protegido de `QWidget` observada
> na versão `2.1.6`.

Dependências instaladas automaticamente:

```text
base-devel pkgconf ncurses gtk4 qt6-base desktop-file-utils xorg-server-xvfb
```

Fluxo de dez etapas:

1. validar `sudo`;
2. instalar dependências com pacman;
3. limpar, compilar e validar PIE/ausência de `TEXTREL`;
4. executar testes do núcleo;
5. executar autotestes GTK e Qt em Xvfb;
6. criar tarball-fonte local e renderizar `PKGBUILD` com SHA-256 real;
7. gerar o pacote com `makepkg` e `PKGDEST`;
8. identificar e preservar o pacote principal;
9. validar identidade/conteúdo com pacman, versão e formato ELF dos binários;
10. instalar com `pacman -U` e conferir banco, proprietário, binário e PATH.

Formato esperado:

```text
pacotes/archlinux/fix-names-2.1.7-1-ARQUITETURA.pkg.tar.zst
```

A instalação local não usa `--needed`; isso permite reinstalar uma compilação
da mesma revisão. O `makepkg` ignora resultados antigos por `--cleanbuild`.
O script usa `readelf` antes do empacotamento e após extrair o pacote: CLI,
GTK e Qt devem ser ELF do tipo `DYN` (PIE) e não podem conter `TEXTREL`.

## Migração de instalações antigas

Os dois geradores procuram arquivos antigos do projeto em `/usr/local` que
poderiam ocultar `/usr/bin`. Eles não apagam esses arquivos: movem-nos para:

```text
pacotes/debian/backup-legado.XXXXXX/
pacotes/archlinux/backup-legado.XXXXXX/
```

Quando havia um comando antigo em `/usr/local/bin`, um link de compatibilidade
para o novo `/usr/bin` é criado. O backup volta a pertencer ao usuário que
iniciou o script.

## Integridade da entrega

Cada empacotador é autossuficiente e valida:

- presença e conteúdo não vazio dos componentes obrigatórios;
- `SHA256SUMS`;
- sintaxe POSIX dos scripts;
- marcadores do `control.in` ou `PKGBUILD.in`;
- assinatura PNG do ícone.

Os diretórios temporários são criados com prefixos exclusivos e só são
removidos quando o caminho corresponde ao padrão esperado.
