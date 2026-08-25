/*
 * Interface ncurses do fix-names.
 *
 * A interface não contém lógica própria de renomeação. Ela apenas edita
 * RenamerOptions, chama run_renamer() e apresenta o relatório devolvido pelo
 * núcleo. O navegador de diretórios usa directory_iterator sem seguir links.
 */

#include "ncurses_ui.hpp"

#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <ncurses.h>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace fixnames {
namespace {

struct ScreenGuard {
    ScreenGuard()
    {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
    }
    ~ScreenGuard() { endwin(); }
};

struct BrowserItem {
    fs::path path;
    std::string label;
    bool directory = false;
};

void add_truncated(int row, int column, const std::string &text, int width)
{
    if (row < 0 || column < 0 || width <= 0)
        return;
    mvaddnstr(row, column, text.c_str(), width);
}

std::string bool_text(bool value, Language language)
{
    return value ? tr(language, "sim", "yes") : tr(language, "não", "no");
}

std::string case_text(CaseMode mode, Language language)
{
    switch (mode) {
    case CaseMode::Uppercase: return tr(language, "MAIÚSCULAS", "UPPERCASE");
    case CaseMode::Lowercase: return tr(language, "minúsculas", "lowercase");
    case CaseMode::CapitalizeWords:
        return tr(language, "Inicial De Cada Palavra", "Capitalize Each Word");
    case CaseMode::None:
    default: return tr(language, "desativado", "disabled");
    }
}

std::string insert_mode_text(InsertMode mode, Language language)
{
    switch (mode) {
    case InsertMode::Insert: return tr(language, "inserir", "insert");
    case InsertMode::Overwrite: return tr(language, "sobrescrever", "overwrite");
    case InsertMode::None:
    default: return tr(language, "desativado", "disabled");
    }
}

std::string prompt_text(const std::string &title, const std::string &initial,
                        Language language, bool allow_empty = true)
{
    for (;;) {
        erase();
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        (void)rows;
        add_truncated(1, 2, title, columns - 4);
        add_truncated(3, 2, tr(language,
                              "Enter confirma; campo vazio desativa/cancela conforme a opção.",
                              "Enter confirms; an empty field disables/cancels as applicable."),
                      columns - 4);
        move(5, 2);
        clrtoeol();
        addstr(initial.c_str());
        curs_set(1);
        echo();

        std::vector<char> buffer(4096, '\0');
        std::copy_n(initial.c_str(), std::min(initial.size(), buffer.size() - 1),
                    buffer.begin());
        move(5, 2);
        getnstr(buffer.data(), static_cast<int>(buffer.size() - 1));
        noecho();
        curs_set(0);
        std::string result(buffer.data());
        if (allow_empty || !result.empty())
            return result;
        beep();
    }
}

bool ask_yes_no(const std::string &question, Language language)
{
    erase();
    int rows, columns;
    getmaxyx(stdscr, rows, columns);
    (void)rows;
    add_truncated(2, 2, question, columns - 4);
    add_truncated(4, 2, tr(language, "Pressione S para sim ou N para não.",
                          "Press Y for yes or N for no."), columns - 4);
    refresh();
    for (;;) {
        const int key = getch();
        if ((language == Language::PortugueseBrazil && (key == 's' || key == 'S')) ||
            (language == Language::English && (key == 'y' || key == 'Y')))
            return true;
        if (key == 'n' || key == 'N' || key == 27)
            return false;
    }
}

int select_choice(const std::string &title, const std::vector<std::string> &items,
                  int selected, Language language)
{
    for (;;) {
        erase();
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        add_truncated(1, 2, title, columns - 4);
        add_truncated(2, 2, tr(language,
                              "Setas: mover | Enter: escolher | Esc: cancelar",
                              "Arrows: move | Enter: choose | Esc: cancel"),
                      columns - 4);
        const int visible = std::max(1, rows - 5);
        int offset = 0;
        if (selected >= visible)
            offset = selected - visible + 1;
        for (int line = 0; line < visible && offset + line < static_cast<int>(items.size()); ++line) {
            const int index = offset + line;
            if (index == selected)
                attron(A_REVERSE);
            add_truncated(4 + line, 2, items[static_cast<std::size_t>(index)], columns - 4);
            if (index == selected)
                attroff(A_REVERSE);
        }
        refresh();
        const int key = getch();
        if (key == KEY_UP && selected > 0)
            --selected;
        else if (key == KEY_DOWN && selected + 1 < static_cast<int>(items.size()))
            ++selected;
        else if (key == '\n' || key == KEY_ENTER)
            return selected;
        else if (key == 27)
            return -1;
    }
}

std::vector<BrowserItem> list_browser_items(const fs::path &directory,
                                            bool directories_only)
{
    std::vector<BrowserItem> items;
    std::error_code error;
    fs::directory_iterator iterator(directory,
                                    fs::directory_options::skip_permission_denied,
                                    error);
    if (error)
        return items;

    for (const fs::directory_entry &entry : iterator) {
        const fs::file_status status = entry.symlink_status(error);
        if (error) {
            error.clear();
            continue;
        }
        const bool is_dir = fs::is_directory(status) && !fs::is_symlink(status);
        if (directories_only && !is_dir)
            continue;
        BrowserItem item;
        item.path = entry.path();
        item.directory = is_dir;
        item.label = (is_dir ? "[D] " : "[F] ") + entry.path().filename().string();
        items.push_back(std::move(item));
    }
    std::sort(items.begin(), items.end(), [](const BrowserItem &left,
                                             const BrowserItem &right) {
        if (left.directory != right.directory)
            return left.directory > right.directory;
        return left.label < right.label;
    });
    return items;
}

fs::path browse_path(const fs::path &initial, bool directories_only,
                     Language language)
{
    std::error_code error;
    fs::path current = fs::absolute(initial, error);
    if (error || !fs::is_directory(current, error)) {
        current = fs::absolute(initial.parent_path().empty() ? fs::path(".")
                                                             : initial.parent_path(),
                               error);
    }
    if (error)
        current = "/";
    current = current.lexically_normal();
    int selected = 0;
    int offset = 0;

    for (;;) {
        std::vector<BrowserItem> items = list_browser_items(current,
                                                            directories_only);
        if (selected >= static_cast<int>(items.size()))
            selected = items.empty() ? 0 : static_cast<int>(items.size()) - 1;

        erase();
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        add_truncated(0, 1, tr(language, "Navegador: ", "Browser: ") +
                      display_path(current), columns - 2);
        add_truncated(1, 1,
                      directories_only
                          ? tr(language,
                               "Enter: abrir | Backspace: voltar | S: escolher pasta | Esc: cancelar",
                               "Enter: open | Backspace: parent | S: select folder | Esc: cancel")
                          : tr(language,
                               "Enter: abrir/escolher arquivo | S: escolher item | C: pasta atual | Esc: cancelar",
                               "Enter: open/select file | S: select item | C: current folder | Esc: cancel"),
                      columns - 2);

        const int visible = std::max(1, rows - 3);
        if (selected < offset)
            offset = selected;
        if (selected >= offset + visible)
            offset = selected - visible + 1;
        for (int line = 0; line < visible && offset + line < static_cast<int>(items.size()); ++line) {
            const int index = offset + line;
            if (index == selected)
                attron(A_REVERSE);
            add_truncated(3 + line, 2, items[static_cast<std::size_t>(index)].label,
                          columns - 4);
            if (index == selected)
                attroff(A_REVERSE);
        }
        if (items.empty())
            add_truncated(3, 2, tr(language, "(pasta vazia ou inacessível)",
                                  "(empty or inaccessible directory)"), columns - 4);
        refresh();

        const int key = getch();
        if (key == KEY_UP && selected > 0)
            --selected;
        else if (key == KEY_DOWN && selected + 1 < static_cast<int>(items.size()))
            ++selected;
        else if (key == KEY_NPAGE && !items.empty())
            selected = std::min(static_cast<int>(items.size()) - 1, selected + visible);
        else if (key == KEY_PPAGE)
            selected = std::max(0, selected - visible);
        else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            if (current != current.root_path()) {
                current = current.parent_path();
                selected = offset = 0;
            }
        } else if (key == '\n' || key == KEY_ENTER) {
            if (items.empty())
                continue;
            const BrowserItem &item = items[static_cast<std::size_t>(selected)];
            if (item.directory) {
                current = item.path;
                selected = offset = 0;
            } else if (!directories_only) {
                return item.path;
            }
        } else if (key == 's' || key == 'S') {
            if (directories_only)
                return current;
            if (!items.empty())
                return items[static_cast<std::size_t>(selected)].path;
        } else if (!directories_only && (key == 'c' || key == 'C')) {
            return current;
        } else if (key == 27) {
            return {};
        }
    }
}

void view_lines(const std::string &title, const std::vector<std::string> &lines,
                Language language)
{
    int offset = 0;
    for (;;) {
        erase();
        int rows, columns;
        getmaxyx(stdscr, rows, columns);
        add_truncated(0, 1, title, columns - 2);
        add_truncated(1, 1, tr(language,
                              "Setas/PgUp/PgDn: rolar | Esc/Enter: voltar",
                              "Arrows/PgUp/PgDn: scroll | Esc/Enter: return"),
                      columns - 2);
        const int visible = std::max(1, rows - 3);
        for (int line = 0; line < visible && offset + line < static_cast<int>(lines.size()); ++line)
            add_truncated(3 + line, 1, lines[static_cast<std::size_t>(offset + line)],
                          columns - 2);
        if (lines.empty())
            add_truncated(3, 1, tr(language, "(sem mensagens)", "(no messages)"),
                          columns - 2);
        refresh();
        const int key = getch();
        if (key == KEY_UP && offset > 0)
            --offset;
        else if (key == KEY_DOWN && offset + visible < static_cast<int>(lines.size()))
            ++offset;
        else if (key == KEY_PPAGE)
            offset = std::max(0, offset - visible);
        else if (key == KEY_NPAGE)
            offset = std::min(std::max(0, static_cast<int>(lines.size()) - visible),
                              offset + visible);
        else if (key == 27 || key == '\n' || key == KEY_ENTER)
            return;
    }
}

/* Executa o núcleo enquanto mantém uma barra percentual visível. O callback
 * é chamado pelo núcleo após cada item concluído, portanto a barra representa
 * tanto itens renomeados quanto inalterados, excluídos, simulados ou recusados
 * por conflito. */
RunResult run_with_progress(const RenamerOptions &options, Language language,
                            const std::string &title)
{
    return run_renamer(
        options, language, {},
        [&title, language](std::size_t completed, std::size_t total) {
            const int percentage = progress_percentage(completed, total);
            erase();
            int rows, columns;
            getmaxyx(stdscr, rows, columns);
            add_truncated(0, 1, title, columns - 2);
            add_truncated(2, 1,
                          tr(language, "Processando itens...", "Processing items..."),
                          columns - 2);

            const int width = std::max(1, std::min(50, columns - 4));
            const int filled = percentage * width / 100;
            std::string bar = "[";
            for (int index = 0; index < width; ++index)
                bar.push_back(index < filled ? '#' : '-');
            bar.push_back(']');
            add_truncated(std::min(4, std::max(0, rows - 2)), 1, bar, columns - 2);

            std::ostringstream status;
            status << percentage << "% (" << completed << '/' << total << ')';
            add_truncated(std::min(5, std::max(0, rows - 1)), 1,
                          status.str(), columns - 2);
            refresh();
        });
}

void configure_insert(RenamerOptions &options, Language language)
{
    const std::vector<std::string> choices = {
        tr(language, "Desativado", "Disabled"),
        tr(language, "Inserir", "Insert"),
        tr(language, "Sobrescrever", "Overwrite")
    };
    int selected = static_cast<int>(options.insert_mode);
    selected = select_choice(tr(language, "Inserir / sobrescrever",
                                "Insert / overwrite"), choices, selected, language);
    if (selected < 0)
        return;
    options.insert_mode = static_cast<InsertMode>(selected);
    if (options.insert_mode == InsertMode::None) {
        options.insert_text.clear();
        return;
    }
    options.insert_text = prompt_text(tr(language, "Texto:", "Text:"),
                                      options.insert_text, language, false);
    const std::string position = prompt_text(
        tr(language, "Posição numérica (0 = início):",
           "Numeric position (0 = beginning):"),
        std::to_string(options.insert_position), language, false);
    try {
        options.insert_position = static_cast<std::size_t>(std::stoull(position));
    } catch (...) {
        options.insert_position = 0;
    }
    options.position_from_end = ask_yes_no(
        tr(language, "Contar a posição a partir do fim?",
           "Count the position from the end?"), language);
}

std::vector<std::string> main_rows(const RenamerOptions &options,
                                   Language language)
{
    std::vector<std::string> rows;
    rows.push_back(tr(language, "Pasta: ", "Folder: ") + display_path(options.target));
    rows.push_back(tr(language, "Capitalização: ", "Letter case: ") +
                   case_text(options.case_mode, language));
    rows.push_back(tr(language, "Remover acentos: ", "Remove accents: ") +
                   bool_text(options.remove_accents, language));
    rows.push_back(tr(language, "Substituir espaços: ", "Replace spaces: ") +
                   (options.replace_spaces ? options.space_replacement
                                            : tr(language, "desativado", "disabled")));
    rows.push_back(tr(language, "Substituir sublinhados: ", "Replace underscores: ") +
                   (options.replace_underscores ? options.underscore_replacement
                                                : tr(language, "desativado", "disabled")));
    rows.push_back(tr(language, "Localizar/substituir: ", "Find/replace: ") +
                   (options.find_replace_enabled
                        ? options.find_text + " -> " + options.replacement_text
                        : tr(language, "desativado", "disabled")));
    rows.push_back(tr(language, "Inserir/sobrescrever: ", "Insert/overwrite: ") +
                   insert_mode_text(options.insert_mode, language));
    rows.push_back(tr(language, "Aplicação recursiva: ", "Recursive: ") +
                   bool_text(options.recursive, language));
    rows.push_back(tr(language, "Alterar extensões: ", "Change extensions: ") +
                   bool_text(options.include_extension, language));
    rows.push_back(tr(language, "Adicionar item à lista de exclusão", "Add excluded item"));
    rows.push_back(tr(language, "Limpar exclusões (", "Clear exclusions (") +
                   std::to_string(options.exclusions.size()) + ")");
    rows.push_back(tr(language, "Pré-visualizar (simulação)", "Preview (dry run)"));
    rows.push_back(tr(language, "Aplicar alterações", "Apply changes"));
    rows.push_back(tr(language, "Ajuda", "Help"));
    rows.push_back(tr(language, "Sair", "Exit"));
    return rows;
}

void show_help(Language language)
{
    const std::vector<std::string> lines = {
        tr(language, "Cada transformação precisa ser ativada nesta tela.",
                     "Every transformation must be enabled on this screen."),
        tr(language, "As extensões ficam protegidas enquanto 'Alterar extensões' = não.",
                     "Extensions stay protected while 'Change extensions' = no."),
        tr(language, "Recursão = não limita a ação ao conteúdo imediato da pasta.",
                     "Recursive = no limits processing to immediate folder contents."),
        tr(language, "Pastas excluídas protegem também todo o conteúdo abaixo delas.",
                     "Excluded folders also protect everything below them."),
        tr(language, "Pré-visualize antes de aplicar. Destinos existentes nunca são sobrescritos.",
                     "Preview before applying. Existing destinations are never overwritten."),
        tr(language, "Inserir/sobrescrever usa posição em caracteres, da esquerda ou direita.",
                     "Insert/overwrite uses a character position from the left or right."),
        tr(language, "O diretório-base escolhido não tem o próprio nome alterado.",
                     "The selected base directory itself is not renamed."),
        tr(language, "O fix-names recusa execução como root.",
                     "fix-names refuses to run as root.")
    };
    view_lines(tr(language, "Ajuda do fix-names", "fix-names help"), lines, language);
}

} // namespace

int run_ncurses_interface(RenamerOptions options, Language language)
{
#ifndef FIX_NAMES_TEST_ALLOW_ROOT
    if (running_as_root()) {
        std::cerr << tr(language,
                        "ERRO: o fix-names nunca pode ser executado como root.\n",
                        "ERROR: fix-names must never run as root.\n");
        return 77;
    }
#endif

    ScreenGuard screen;
    int selected = 0;
    int offset = 0;

    for (;;) {
        const std::vector<std::string> rows = main_rows(options, language);
        erase();
        int terminal_rows, columns;
        getmaxyx(stdscr, terminal_rows, columns);
        add_truncated(0, 1, "fix-names " + std::string(VERSION), columns - 2);
        add_truncated(1, 1, tr(language,
                              "Setas: mover | Enter: editar/executar | Q: sair",
                              "Arrows: move | Enter: edit/run | Q: quit"), columns - 2);
        const int visible = std::max(1, terminal_rows - 3);
        if (selected < offset)
            offset = selected;
        if (selected >= offset + visible)
            offset = selected - visible + 1;
        for (int line = 0; line < visible && offset + line < static_cast<int>(rows.size()); ++line) {
            const int index = offset + line;
            if (index == selected)
                attron(A_REVERSE);
            add_truncated(3 + line, 2, rows[static_cast<std::size_t>(index)], columns - 4);
            if (index == selected)
                attroff(A_REVERSE);
        }
        refresh();

        const int key = getch();
        if (key == KEY_UP && selected > 0) {
            --selected;
            continue;
        }
        if (key == KEY_DOWN && selected + 1 < static_cast<int>(rows.size())) {
            ++selected;
            continue;
        }
        if (key == 'q' || key == 'Q')
            return 0;
        if (key != '\n' && key != KEY_ENTER)
            continue;

        switch (selected) {
        case 0: {
            const fs::path chosen = browse_path(options.target, true, language);
            if (!chosen.empty())
                options.target = chosen;
            break;
        }
        case 1:
            options.case_mode = static_cast<CaseMode>(
                (static_cast<int>(options.case_mode) + 1) % 4);
            break;
        case 2:
            options.remove_accents = !options.remove_accents;
            break;
        case 3: {
            const std::string value = prompt_text(
                tr(language,
                   "Caractere que substituirá cada espaço (vazio = desativar):",
                   "Character replacing each space (empty = disable):"),
                options.replace_spaces ? options.space_replacement : "",
                language);
            options.replace_spaces = !value.empty();
            options.space_replacement = value;
            break;
        }
        case 4: {
            const std::string value = prompt_text(
                tr(language,
                   "Caractere que substituirá cada _ (vazio = desativar):",
                   "Character replacing each _ (empty = disable):"),
                options.replace_underscores ? options.underscore_replacement : "",
                language);
            options.replace_underscores = !value.empty();
            options.underscore_replacement = value;
            break;
        }
        case 5: {
            options.find_text = prompt_text(
                tr(language, "Texto a localizar (vazio = desativar):",
                   "Text to find (empty = disable):"),
                options.find_replace_enabled ? options.find_text : "", language);
            options.find_replace_enabled = !options.find_text.empty();
            if (options.find_replace_enabled)
                options.replacement_text = prompt_text(
                    tr(language, "Substituir por (pode ficar vazio):",
                       "Replace with (may be empty):"),
                    options.replacement_text, language);
            break;
        }
        case 6:
            configure_insert(options, language);
            break;
        case 7:
            options.recursive = !options.recursive;
            break;
        case 8:
            options.include_extension = !options.include_extension;
            break;
        case 9: {
            const fs::path chosen = browse_path(options.target, false, language);
            if (!chosen.empty() &&
                std::find(options.exclusions.begin(), options.exclusions.end(), chosen) ==
                    options.exclusions.end())
                options.exclusions.push_back(chosen);
            break;
        }
        case 10:
            if (options.exclusions.empty() ||
                ask_yes_no(tr(language, "Limpar toda a lista de exclusão?",
                              "Clear the entire exclusion list?"), language))
                options.exclusions.clear();
            break;
        case 11: {
            RenamerOptions preview = options;
            preview.dry_run = true;
            const RunResult result = run_with_progress(
                preview, language,
                tr(language, "Simulando alterações", "Simulating changes"));
            view_lines(tr(language, "Resultado da simulação", "Dry-run result"),
                       result.messages, language);
            break;
        }
        case 12:
            if (ask_yes_no(tr(language,
                              "Aplicar agora? Esta operação altera nomes reais.",
                              "Apply now? This operation changes real names."),
                           language)) {
                RenamerOptions apply = options;
                apply.dry_run = false;
                const RunResult result = run_with_progress(
                    apply, language,
                    tr(language, "Aplicando alterações", "Applying changes"));
                view_lines(tr(language, "Resultado da execução", "Execution result"),
                           result.messages, language);
            }
            break;
        case 13:
            show_help(language);
            break;
        case 14:
            return 0;
        default:
            break;
        }
    }
}

} // namespace fixnames
