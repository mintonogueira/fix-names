# ==============================================================================
# Makefile do fix-names
# ==============================================================================
#
# Os quatro executáveis compartilham build/core.o:
#   fix-names      -> CLI + ncurses
#   fix-names-gtk  -> interface GTK 4
#   fix-names-qt   -> interface Qt 6 Widgets
#   test-core      -> testes automatizados do núcleo
#
# PREFIX pode ser trocado no momento da instalação. Exemplo:
#   make PREFIX=/usr install
# ==============================================================================

CXX ?= c++
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
DESTDIR ?=
BUILD_DIR ?= build

CPPFLAGS ?=
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDFLAGS ?=

# O Arch Linux acrescenta LTO a CXXFLAGS e LDFLAGS pelo makepkg. Com LTO, o
# GCC combina as opções PIC/PIE de todas as unidades; misturar core.o sem PIC
# com qt_main.o usando -fPIC pode produzir uma copy relocation proibida contra
# símbolos protegidos do Qt. Por isso, -fPIC faz parte de TODA compilação e o
# vínculo de TODOS os executáveis é explicitamente PIE.
PIC_CXXFLAGS := -fPIC
PIE_LDFLAGS := -fPIC -pie

NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || printf '%s' '-lncursesw')
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4 2>/dev/null)
QT_CFLAGS := $(shell $(PKG_CONFIG) --cflags Qt6Widgets 2>/dev/null)
QT_LIBS := $(shell $(PKG_CONFIG) --libs Qt6Widgets 2>/dev/null)

COMMON_DEPS = src/core.hpp
ALL_BINARIES = $(BUILD_DIR)/fix-names $(BUILD_DIR)/fix-names-gtk $(BUILD_DIR)/fix-names-qt

# A documentação faz parte do produto instalado, não apenas do tarball-fonte.
# Manter esta lista explícita garante que os dois pacotes nativos recebam os
# mesmos capítulos e evita que um arquivo temporário seja incluído por um glob
# amplo. Os caminhos relativos são preservados sob share/doc/fix-names/.
DOC_FILES = README.md DOCUMENTACAO.md CHANGELOG.md VERSOES.md \
	docs/MANUAL_DO_USUARIO.md docs/REFERENCIA_CLI.md docs/INTERFACES.md \
	docs/ARQUITETURA_E_SEGURANCA.md docs/REFERENCIA_DO_NUCLEO.md \
	docs/COMPILACAO_E_EMPACOTAMENTO.md \
	docs/DESENVOLVIMENTO_E_TESTES.md docs/SOLUCAO_DE_PROBLEMAS.md

.PHONY: all binaries check-dependencies test install clean

all: check-dependencies
	$(MAKE) binaries

binaries: $(ALL_BINARIES)

check-dependencies:
	@command -v "$(CXX)" >/dev/null 2>&1 || { echo 'ERRO: compilador C++ não encontrado.' >&2; exit 1; }
	@command -v "$(PKG_CONFIG)" >/dev/null 2>&1 || { echo 'ERRO: pkg-config não encontrado.' >&2; exit 1; }
	@$(PKG_CONFIG) --exists ncursesw || { echo 'ERRO: arquivos de desenvolvimento ncursesw ausentes.' >&2; exit 1; }
	@$(PKG_CONFIG) --exists gtk4 || { echo 'ERRO: arquivos de desenvolvimento GTK 4 ausentes.' >&2; exit 1; }
	@$(PKG_CONFIG) --exists Qt6Widgets || { echo 'ERRO: arquivos de desenvolvimento Qt 6 Widgets ausentes.' >&2; exit 1; }

$(BUILD_DIR):
	mkdir -p -- "$@"

$(BUILD_DIR)/core.o: src/core.cpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/ncurses_ui.o: src/ncurses_ui.cpp src/ncurses_ui.hpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) $(NCURSES_CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/cli_main.o: src/cli_main.cpp src/ncurses_ui.hpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) $(NCURSES_CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/gtk_main.o: src/gtk_main.cpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) $(GTK_CFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/qt_main.o: src/qt_main.cpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) $(QT_CFLAGS) -Isrc \
		-DFIX_NAMES_ICON_PATH='"$(PREFIX)/share/pixmaps/fix-names.png"' \
		-MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/fix-names: $(BUILD_DIR)/core.o $(BUILD_DIR)/ncurses_ui.o $(BUILD_DIR)/cli_main.o
	$(CXX) $(LDFLAGS) $(PIE_LDFLAGS) $^ $(NCURSES_LIBS) -o "$@"

$(BUILD_DIR)/fix-names-gtk: $(BUILD_DIR)/core.o $(BUILD_DIR)/gtk_main.o
	$(CXX) $(LDFLAGS) $(PIE_LDFLAGS) $^ $(GTK_LIBS) -o "$@"

$(BUILD_DIR)/fix-names-qt: $(BUILD_DIR)/core.o $(BUILD_DIR)/qt_main.o
	$(CXX) $(LDFLAGS) $(PIE_LDFLAGS) $^ $(QT_LIBS) -o "$@"

$(BUILD_DIR)/test-core.o: tests/test_core.cpp $(COMMON_DEPS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PIC_CXXFLAGS) -Isrc -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/test-core: $(BUILD_DIR)/core.o $(BUILD_DIR)/test-core.o
	$(CXX) $(LDFLAGS) $(PIE_LDFLAGS) $^ -o "$@"

test: $(BUILD_DIR)/test-core
	"$(BUILD_DIR)/test-core"

install: all
	install -D -m 0755 "$(BUILD_DIR)/fix-names" "$(DESTDIR)$(PREFIX)/bin/fix-names"
	install -D -m 0755 "$(BUILD_DIR)/fix-names-gtk" "$(DESTDIR)$(PREFIX)/bin/fix-names-gtk"
	install -D -m 0755 "$(BUILD_DIR)/fix-names-qt" "$(DESTDIR)$(PREFIX)/bin/fix-names-qt"
	install -D -m 0755 data/fix-names-gui "$(DESTDIR)$(PREFIX)/bin/fix-names-gui"
	install -D -m 0644 assets/fix-names.png "$(DESTDIR)$(PREFIX)/share/pixmaps/fix-names.png"
	install -D -m 0644 data/fix-names.desktop "$(DESTDIR)$(PREFIX)/share/applications/fix-names.desktop"
	install -D -m 0644 data/fix-names.1 "$(DESTDIR)$(PREFIX)/share/man/man1/fix-names.1"
	@for document in $(DOC_FILES); do \
		install -D -m 0644 "$$document" \
			"$(DESTDIR)$(PREFIX)/share/doc/fix-names/$$document"; \
	done

clean:
	@test -n "$(BUILD_DIR)" && test "$(BUILD_DIR)" != "/"
	rm -f -- "$(BUILD_DIR)/core.o" "$(BUILD_DIR)/ncurses_ui.o" \
		"$(BUILD_DIR)/cli_main.o" "$(BUILD_DIR)/gtk_main.o" \
		"$(BUILD_DIR)/qt_main.o" "$(BUILD_DIR)/test-core.o" \
		"$(BUILD_DIR)/fix-names" "$(BUILD_DIR)/fix-names-gtk" \
		"$(BUILD_DIR)/fix-names-qt" "$(BUILD_DIR)/test-core"
	rm -f -- "$(BUILD_DIR)/core.d" "$(BUILD_DIR)/ncurses_ui.d" \
		"$(BUILD_DIR)/cli_main.d" "$(BUILD_DIR)/gtk_main.d" \
		"$(BUILD_DIR)/qt_main.d" "$(BUILD_DIR)/test-core.d"
	@rmdir -- "$(BUILD_DIR)" 2>/dev/null || :

-include $(BUILD_DIR)/*.d
