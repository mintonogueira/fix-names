/*
 * Interface gráfica GTK 4 do fix-names.
 *
 * Este executável é propositalmente separado da CLI. Nenhum CSS é aplicado:
 * widgets, cores, fontes e espaçamentos fundamentais são fornecidos pelo tema
 * GTK configurado no GNOME, XFCE, Cinnamon, MATE ou outro ambiente.
 */

#include "core.hpp"

#include <gtk/gtk.h>

#include <algorithm>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace fixnames;

namespace {

struct GtkState {
    Language language = Language::English;
    GtkWidget *window = nullptr;
    GtkWidget *path_entry = nullptr;
    GtkWidget *case_combo = nullptr;
    GtkWidget *accents_check = nullptr;
    GtkWidget *spaces_check = nullptr;
    GtkWidget *space_entry = nullptr;
    GtkWidget *underscores_check = nullptr;
    GtkWidget *underscore_entry = nullptr;
    GtkWidget *find_check = nullptr;
    GtkWidget *find_entry = nullptr;
    GtkWidget *replace_entry = nullptr;
    GtkWidget *insert_combo = nullptr;
    GtkWidget *insert_entry = nullptr;
    GtkWidget *position_spin = nullptr;
    GtkWidget *from_end_check = nullptr;
    GtkWidget *recursive_check = nullptr;
    GtkWidget *include_extension_check = nullptr;
    GtkWidget *exclusions_list = nullptr;
    GtkWidget *log_view = nullptr;
    std::vector<fs::path> exclusions;
};

struct ActivationData {
    std::string initial_path = ".";
    bool self_test = false;
};

const char *text(Language language, const char *pt, const char *en)
{
    return language == Language::PortugueseBrazil ? pt : en;
}

GtkWidget *make_label(const char *label)
{
    GtkWidget *widget = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(widget), 0.0F);
    return widget;
}

void set_margins(GtkWidget *widget, int margin)
{
    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
    gtk_widget_set_margin_top(widget, margin);
    gtk_widget_set_margin_bottom(widget, margin);
}

void set_log(GtkState *state, const std::vector<std::string> &lines)
{
    std::string joined;
    for (const std::string &line : lines) {
        joined += line;
        joined.push_back('\n');
    }
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->log_view));
    gtk_text_buffer_set_text(buffer, joined.c_str(), -1);
}

void set_log(GtkState *state, const std::string &line)
{
    set_log(state, std::vector<std::string>{line});
}

void rebuild_exclusion_list(GtkState *state)
{
    GtkWidget *child = gtk_widget_get_first_child(state->exclusions_list);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(state->exclusions_list), child);
        child = next;
    }
    for (const fs::path &path : state->exclusions) {
        GtkWidget *label = make_label(display_path(path).c_str());
        set_margins(label, 5);
        gtk_list_box_append(GTK_LIST_BOX(state->exclusions_list), label);
    }
}

RenamerOptions collect_options(GtkState *state, bool dry_run)
{
    RenamerOptions options;
    options.target = gtk_editable_get_text(GTK_EDITABLE(state->path_entry));
    options.case_mode = static_cast<CaseMode>(
        gtk_combo_box_get_active(GTK_COMBO_BOX(state->case_combo)));
    options.remove_accents = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->accents_check));
    options.replace_spaces = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->spaces_check));
    options.space_replacement = gtk_editable_get_text(
        GTK_EDITABLE(state->space_entry));
    options.replace_underscores = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->underscores_check));
    options.underscore_replacement = gtk_editable_get_text(
        GTK_EDITABLE(state->underscore_entry));
    options.find_replace_enabled = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->find_check));
    options.find_text = gtk_editable_get_text(GTK_EDITABLE(state->find_entry));
    options.replacement_text = gtk_editable_get_text(
        GTK_EDITABLE(state->replace_entry));
    options.insert_mode = static_cast<InsertMode>(
        gtk_combo_box_get_active(GTK_COMBO_BOX(state->insert_combo)));
    options.insert_text = gtk_editable_get_text(GTK_EDITABLE(state->insert_entry));
    options.insert_position = static_cast<std::size_t>(
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(state->position_spin)));
    options.position_from_end = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->from_end_check));
    options.recursive = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->recursive_check));
    options.include_extension = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(state->include_extension_check));
    options.exclusions = state->exclusions;
    options.dry_run = dry_run;
    return options;
}

void run_operation(GtkState *state, bool dry_run)
{
    const RenamerOptions options = collect_options(state, dry_run);
    const RunResult result = run_renamer(options, state->language);
    set_log(state, result.messages);
}

void on_folder_response(GtkNativeDialog *dialog, int response, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file) {
            char *path = g_file_get_path(file);
            if (path) {
                gtk_editable_set_text(GTK_EDITABLE(state->path_entry), path);
                g_free(path);
            }
            g_object_unref(file);
        }
    }
    g_object_unref(dialog);
}

void on_browse_folder(GtkButton *, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    GtkFileChooserNative *dialog = gtk_file_chooser_native_new(
        text(state->language, "Escolher pasta", "Choose folder"),
        GTK_WINDOW(state->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        text(state->language, "Escolher", "Select"),
        text(state->language, "Cancelar", "Cancel"));
    g_signal_connect(dialog, "response", G_CALLBACK(on_folder_response), state);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

void on_exclusions_response(GtkNativeDialog *dialog, int response, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    if (response == GTK_RESPONSE_ACCEPT) {
        GListModel *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(dialog));
        const guint count = g_list_model_get_n_items(files);
        for (guint index = 0; index < count; ++index) {
            GFile *file = G_FILE(g_list_model_get_item(files, index));
            char *path = g_file_get_path(file);
            if (path) {
                const fs::path candidate(path);
                if (std::find(state->exclusions.begin(), state->exclusions.end(),
                              candidate) == state->exclusions.end())
                    state->exclusions.push_back(candidate);
                g_free(path);
            }
            g_object_unref(file);
        }
        g_object_unref(files);
        rebuild_exclusion_list(state);
    }
    g_object_unref(dialog);
}

void on_add_exclusion(GtkButton *, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    GtkFileChooserNative *dialog = gtk_file_chooser_native_new(
        text(state->language, "Selecionar arquivos a ignorar",
             "Select files to exclude"),
        GTK_WINDOW(state->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        text(state->language, "Adicionar", "Add"),
        text(state->language, "Cancelar", "Cancel"));
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    g_signal_connect(dialog, "response", G_CALLBACK(on_exclusions_response), state);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

void on_remove_exclusion(GtkButton *, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(
        GTK_LIST_BOX(state->exclusions_list));
    if (!row)
        return;
    const int index = gtk_list_box_row_get_index(row);
    if (index >= 0 && static_cast<std::size_t>(index) < state->exclusions.size()) {
        state->exclusions.erase(state->exclusions.begin() + index);
        rebuild_exclusion_list(state);
    }
}

void on_toggle_spaces(GtkCheckButton *button, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    gtk_widget_set_sensitive(state->space_entry,
                             gtk_check_button_get_active(button));
}

void on_toggle_underscores(GtkCheckButton *button, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    gtk_widget_set_sensitive(state->underscore_entry,
                             gtk_check_button_get_active(button));
}

void on_toggle_find(GtkCheckButton *button, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    const gboolean active = gtk_check_button_get_active(button);
    gtk_widget_set_sensitive(state->find_entry, active);
    gtk_widget_set_sensitive(state->replace_entry, active);
}

void on_insert_changed(GtkComboBox *combo, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    const gboolean active = gtk_combo_box_get_active(combo) != 0;
    gtk_widget_set_sensitive(state->insert_entry, active);
    gtk_widget_set_sensitive(state->position_spin, active);
    gtk_widget_set_sensitive(state->from_end_check, active);
}

void on_preview(GtkButton *, gpointer data)
{
    run_operation(static_cast<GtkState *>(data), true);
}

void on_confirm_response(GtkDialog *dialog, int response, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    gtk_window_destroy(GTK_WINDOW(dialog));
    if (response == GTK_RESPONSE_YES)
        run_operation(state, false);
}

void on_apply(GtkButton *, gpointer data)
{
    auto *state = static_cast<GtkState *>(data);
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(state->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, "%s",
        text(state->language,
             "Aplicar as alterações? Destinos existentes não serão sobrescritos.",
             "Apply changes? Existing destinations will not be overwritten."));
    gtk_window_set_title(GTK_WINDOW(dialog),
                         text(state->language, "Confirmar", "Confirm"));
    g_signal_connect(dialog, "response", G_CALLBACK(on_confirm_response), state);
    gtk_window_present(GTK_WINDOW(dialog));
}

void attach_row(GtkGrid *grid, int row, GtkWidget *label, GtkWidget *control)
{
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, control, 1, row, 1, 1);
    gtk_widget_set_hexpand(control, TRUE);
}

void on_activate(GtkApplication *application, gpointer user_data)
{
    auto *activation = static_cast<ActivationData *>(user_data);
    auto *state = new GtkState;
    state->language = detect_language();

    state->window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(state->window), "fix-names");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 900, 780);
    gtk_window_set_icon_name(GTK_WINDOW(state->window), "fix-names");
    g_object_set_data_full(G_OBJECT(state->window), "fix-names-state", state,
                           [](gpointer pointer) {
                               delete static_cast<GtkState *>(pointer);
                           });

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins(outer, 12);
    gtk_window_set_child(GTK_WINDOW(state->window), outer);

    GtkWidget *title = gtk_label_new(nullptr);
    const std::string markup = "<b>fix-names " + std::string(VERSION) + "</b>";
    gtk_label_set_markup(GTK_LABEL(title), markup.c_str());
    gtk_label_set_xalign(GTK_LABEL(title), 0.0F);
    gtk_box_append(GTK_BOX(outer), title);

    GtkWidget *path_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->path_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(state->path_entry),
                          activation->initial_path.c_str());
    gtk_widget_set_hexpand(state->path_entry, TRUE);
    GtkWidget *browse = gtk_button_new_with_label(
        text(state->language, "Navegar...", "Browse..."));
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_folder), state);
    gtk_box_append(GTK_BOX(path_box), make_label(text(state->language,
                                                      "Pasta:", "Folder:")));
    gtk_box_append(GTK_BOX(path_box), state->path_entry);
    gtk_box_append(GTK_BOX(path_box), browse);
    gtk_box_append(GTK_BOX(outer), path_box);

    GtkWidget *scroll_options = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_options, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_options),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    set_margins(options_box, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_options), options_box);
    gtk_box_append(GTK_BOX(outer), scroll_options);

    GtkWidget *grid_widget = gtk_grid_new();
    GtkGrid *grid = GTK_GRID(grid_widget);
    gtk_grid_set_row_spacing(grid, 8);
    gtk_grid_set_column_spacing(grid, 12);
    gtk_box_append(GTK_BOX(options_box), grid_widget);

    state->case_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->case_combo),
                                   text(state->language, "Sem alteração", "No change"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->case_combo),
                                   text(state->language, "Tudo maiúsculo", "All uppercase"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->case_combo),
                                   text(state->language, "Tudo minúsculo", "All lowercase"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->case_combo),
                                   text(state->language, "Inicial de cada palavra", "Capitalize each word"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->case_combo), 0);
    attach_row(grid, 0, make_label(text(state->language, "Capitalização:", "Letter case:")),
               state->case_combo);

    state->accents_check = gtk_check_button_new_with_label(
        text(state->language, "Remover acentos", "Remove accents"));
    attach_row(grid, 1, make_label(text(state->language, "Acentuação:", "Accents:")),
               state->accents_check);

    GtkWidget *spaces_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->spaces_check = gtk_check_button_new_with_label(
        text(state->language, "Substituir espaços", "Replace spaces"));
    state->space_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(state->space_entry), 8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->space_entry), "-");
    gtk_widget_set_sensitive(state->space_entry, FALSE);
    g_signal_connect(state->spaces_check, "toggled", G_CALLBACK(on_toggle_spaces), state);
    gtk_box_append(GTK_BOX(spaces_box), state->spaces_check);
    gtk_box_append(GTK_BOX(spaces_box), state->space_entry);
    attach_row(grid, 2, make_label(text(state->language, "Espaços:", "Spaces:")), spaces_box);

    GtkWidget *underscores_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->underscores_check = gtk_check_button_new_with_label(
        text(state->language, "Substituir _", "Replace _"));
    state->underscore_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(state->underscore_entry), 8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->underscore_entry), "-");
    gtk_widget_set_sensitive(state->underscore_entry, FALSE);
    g_signal_connect(state->underscores_check, "toggled",
                     G_CALLBACK(on_toggle_underscores), state);
    gtk_box_append(GTK_BOX(underscores_box), state->underscores_check);
    gtk_box_append(GTK_BOX(underscores_box), state->underscore_entry);
    attach_row(grid, 3, make_label(text(state->language, "Sublinhados:", "Underscores:")),
               underscores_box);

    GtkWidget *find_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->find_check = gtk_check_button_new_with_label(
        text(state->language, "Ativar", "Enable"));
    state->find_entry = gtk_entry_new();
    state->replace_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->find_entry),
                                   text(state->language, "Localizar", "Find"));
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->replace_entry),
                                   text(state->language, "Substituir por", "Replace with"));
    gtk_widget_set_hexpand(state->find_entry, TRUE);
    gtk_widget_set_hexpand(state->replace_entry, TRUE);
    gtk_widget_set_sensitive(state->find_entry, FALSE);
    gtk_widget_set_sensitive(state->replace_entry, FALSE);
    g_signal_connect(state->find_check, "toggled", G_CALLBACK(on_toggle_find), state);
    gtk_box_append(GTK_BOX(find_box), state->find_check);
    gtk_box_append(GTK_BOX(find_box), state->find_entry);
    gtk_box_append(GTK_BOX(find_box), state->replace_entry);
    attach_row(grid, 4, make_label(text(state->language, "Localizar/substituir:", "Find/replace:")), find_box);

    GtkWidget *insert_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->insert_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->insert_combo),
                                   text(state->language, "Desativado", "Disabled"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->insert_combo),
                                   text(state->language, "Inserir", "Insert"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->insert_combo),
                                   text(state->language, "Sobrescrever", "Overwrite"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->insert_combo), 0);
    state->insert_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->insert_entry),
                                   text(state->language, "Texto", "Text"));
    state->position_spin = gtk_spin_button_new_with_range(0, 1000000, 1);
    state->from_end_check = gtk_check_button_new_with_label(
        text(state->language, "A partir do fim", "From end"));
    gtk_widget_set_sensitive(state->insert_entry, FALSE);
    gtk_widget_set_sensitive(state->position_spin, FALSE);
    gtk_widget_set_sensitive(state->from_end_check, FALSE);
    g_signal_connect(state->insert_combo, "changed", G_CALLBACK(on_insert_changed), state);
    gtk_box_append(GTK_BOX(insert_box), state->insert_combo);
    gtk_box_append(GTK_BOX(insert_box), state->insert_entry);
    gtk_box_append(GTK_BOX(insert_box), make_label(text(state->language, "Posição:", "Position:")));
    gtk_box_append(GTK_BOX(insert_box), state->position_spin);
    gtk_box_append(GTK_BOX(insert_box), state->from_end_check);
    attach_row(grid, 5, make_label(text(state->language, "Inserir/sobrescrever:", "Insert/overwrite:")), insert_box);

    GtkWidget *scope_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    state->recursive_check = gtk_check_button_new_with_label(
        text(state->language, "Aplicar recursivamente", "Apply recursively"));
    state->include_extension_check = gtk_check_button_new_with_label(
        text(state->language, "Incluir extensões na alteração", "Include extensions"));
    gtk_box_append(GTK_BOX(scope_box), state->recursive_check);
    gtk_box_append(GTK_BOX(scope_box), state->include_extension_check);
    attach_row(grid, 6, make_label(text(state->language, "Escopo:", "Scope:")), scope_box);

    GtkWidget *exclusion_title = make_label(
        text(state->language, "Itens que nunca serão atualizados:",
             "Items that will never be updated:"));
    gtk_box_append(GTK_BOX(options_box), exclusion_title);
    GtkWidget *exclude_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(exclude_scroll), 90);
    state->exclusions_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->exclusions_list),
                                    GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(exclude_scroll),
                                  state->exclusions_list);
    gtk_box_append(GTK_BOX(options_box), exclude_scroll);
    GtkWidget *exclude_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *add_exclusion = gtk_button_new_with_label(
        text(state->language, "Adicionar...", "Add..."));
    GtkWidget *remove_exclusion = gtk_button_new_with_label(
        text(state->language, "Remover selecionado", "Remove selected"));
    g_signal_connect(add_exclusion, "clicked", G_CALLBACK(on_add_exclusion), state);
    g_signal_connect(remove_exclusion, "clicked", G_CALLBACK(on_remove_exclusion), state);
    gtk_box_append(GTK_BOX(exclude_buttons), add_exclusion);
    gtk_box_append(GTK_BOX(exclude_buttons), remove_exclusion);
    gtk_box_append(GTK_BOX(options_box), exclude_buttons);

    GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *preview = gtk_button_new_with_label(
        text(state->language, "Pré-visualizar", "Preview"));
    GtkWidget *apply = gtk_button_new_with_label(
        text(state->language, "Aplicar alterações", "Apply changes"));
    g_signal_connect(preview, "clicked", G_CALLBACK(on_preview), state);
    g_signal_connect(apply, "clicked", G_CALLBACK(on_apply), state);
    gtk_box_append(GTK_BOX(action_box), preview);
    gtk_box_append(GTK_BOX(action_box), apply);
    gtk_box_append(GTK_BOX(outer), action_box);

    GtkWidget *log_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(log_scroll), 150);
    state->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->log_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->log_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->log_view), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll), state->log_view);
    gtk_box_append(GTK_BOX(outer), log_scroll);

    set_log(state, tr(state->language,
                      "Configure as opções e use Pré-visualizar antes de aplicar.",
                      "Configure options and use Preview before applying."));
    gtk_window_present(GTK_WINDOW(state->window));
    if (activation->self_test) {
        g_idle_add([](gpointer pointer) -> gboolean {
            g_application_quit(G_APPLICATION(pointer));
            return G_SOURCE_REMOVE;
        }, application);
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "");
    const Language language = detect_language();
    if (running_as_root()) {
        std::cerr << tr(language,
                        "ERRO: o fix-names nunca pode ser executado como root. Use uma conta comum.\n",
                        "ERROR: fix-names must never run as root. Use a regular account.\n");
        return 77;
    }

    ActivationData activation;
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--self-test")
            activation.self_test = true;
        else
            activation.initial_path = argv[index];
    }
    GtkApplication *application = gtk_application_new(
        "org.fixnames.Gtk", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate),
                     &activation);
    /* O caminho opcional já foi consumido acima. A aplicação recebe somente
     * argv[0], impedindo que GApplication interprete o caminho como opção. */
    char *application_argv[] = {argv[0], nullptr};
    const int status = g_application_run(G_APPLICATION(application), 1,
                                         application_argv);
    g_object_unref(application);
    return status;
}
