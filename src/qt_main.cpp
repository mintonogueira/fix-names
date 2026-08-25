/*
 * Interface gráfica Qt 6 Widgets do fix-names.
 *
 * Este binário não compartilha widgets com GTK ou ncurses; compartilha apenas
 * o núcleo C++. Nenhum QSS nem estilo Fusion é forçado, portanto KDE Plasma e
 * LXQt fornecem naturalmente estilo, paleta, fontes e caixas de arquivo.
 */

#include "core.hpp"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <clocale>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef FIX_NAMES_ICON_PATH
#define FIX_NAMES_ICON_PATH "/usr/local/share/pixmaps/fix-names.png"
#endif

namespace fs = std::filesystem;
using namespace fixnames;

namespace {

QString qtr(Language language, const char *pt, const char *en)
{
    return QString::fromUtf8(language == Language::PortugueseBrazil ? pt : en);
}

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(const QString &initial_path, Language selected_language)
        : language(selected_language)
    {
        setWindowTitle(QStringLiteral("fix-names"));
        setWindowIcon(QIcon::fromTheme(QStringLiteral("fix-names"),
                                      QIcon(QStringLiteral(FIX_NAMES_ICON_PATH))));
        resize(940, 800);
        build_ui(initial_path);
    }

private:
    Language language;
    QLineEdit *path_edit = nullptr;
    QComboBox *case_combo = nullptr;
    QCheckBox *accents_check = nullptr;
    QCheckBox *spaces_check = nullptr;
    QLineEdit *space_edit = nullptr;
    QCheckBox *underscores_check = nullptr;
    QLineEdit *underscore_edit = nullptr;
    QCheckBox *find_check = nullptr;
    QLineEdit *find_edit = nullptr;
    QLineEdit *replace_edit = nullptr;
    QComboBox *insert_combo = nullptr;
    QLineEdit *insert_edit = nullptr;
    QSpinBox *position_spin = nullptr;
    QCheckBox *from_end_check = nullptr;
    QCheckBox *recursive_check = nullptr;
    QCheckBox *include_extension_check = nullptr;
    QListWidget *exclusion_list = nullptr;
    QProgressBar *progress_bar = nullptr;
    QPlainTextEdit *log_view = nullptr;
    std::vector<fs::path> exclusions;

    RenamerOptions collect_options(bool dry_run) const
    {
        RenamerOptions options;
        options.target = path_edit->text().toStdString();
        options.case_mode = static_cast<CaseMode>(case_combo->currentIndex());
        options.remove_accents = accents_check->isChecked();
        options.replace_spaces = spaces_check->isChecked();
        options.space_replacement = space_edit->text().toStdString();
        options.replace_underscores = underscores_check->isChecked();
        options.underscore_replacement = underscore_edit->text().toStdString();
        options.find_replace_enabled = find_check->isChecked();
        options.find_text = find_edit->text().toStdString();
        options.replacement_text = replace_edit->text().toStdString();
        options.insert_mode = static_cast<InsertMode>(insert_combo->currentIndex());
        options.insert_text = insert_edit->text().toStdString();
        options.insert_position = static_cast<std::size_t>(position_spin->value());
        options.position_from_end = from_end_check->isChecked();
        options.recursive = recursive_check->isChecked();
        options.include_extension = include_extension_check->isChecked();
        options.exclusions = exclusions;
        options.dry_run = dry_run;
        return options;
    }

    void show_result(const RunResult &result)
    {
        QStringList lines;
        for (const std::string &line : result.messages)
            lines.push_back(QString::fromUtf8(line.data(),
                                              static_cast<qsizetype>(line.size())));
        log_view->setPlainText(lines.join(QLatin1Char('\n')));
    }

    void run(bool dry_run)
    {
        progress_bar->setValue(0);
        progress_bar->setFormat(qtr(language, "0% — preparando", "0% — preparing"));

        /* O callback usa os contadores do núcleo e processa somente eventos
         * de desenho/timer. Eventos de entrada ficam excluídos para impedir
         * uma segunda requisição enquanto a atual ainda está em andamento. */
        const RunResult result = run_renamer(
            collect_options(dry_run), language, {},
            [this](std::size_t completed, std::size_t total) {
                const int percentage = total == 0
                                           ? 100
                                           : static_cast<int>((completed * 100) / total);
                progress_bar->setValue(percentage);
                progress_bar->setFormat(
                    QStringLiteral("%1% (%2/%3)")
                        .arg(percentage)
                        .arg(static_cast<qulonglong>(completed))
                        .arg(static_cast<qulonglong>(total)));
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            });
        show_result(result);
    }

    void browse_folder()
    {
        const QString selected = QFileDialog::getExistingDirectory(
            this, qtr(language, "Escolher pasta", "Choose folder"), path_edit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selected.isEmpty())
            path_edit->setText(selected);
    }

    void add_exclusions()
    {
        const QStringList selected = QFileDialog::getOpenFileNames(
            this, qtr(language, "Selecionar arquivos que serão ignorados",
                      "Select files to exclude"), path_edit->text());
        for (const QString &item : selected) {
            const fs::path candidate = item.toStdString();
            if (std::find(exclusions.begin(), exclusions.end(), candidate) ==
                exclusions.end()) {
                exclusions.push_back(candidate);
                exclusion_list->addItem(item);
            }
        }
    }

    void remove_exclusion()
    {
        const int row = exclusion_list->currentRow();
        if (row < 0 || static_cast<std::size_t>(row) >= exclusions.size())
            return;
        exclusions.erase(exclusions.begin() + row);
        delete exclusion_list->takeItem(row);
    }

    void confirm_apply()
    {
        const QMessageBox::StandardButton answer = QMessageBox::warning(
            this, qtr(language, "Confirmar", "Confirm"),
            qtr(language,
                "Aplicar as alterações? Destinos existentes não serão sobrescritos.",
                "Apply changes? Existing destinations will not be overwritten."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            run(false);
    }

    void build_ui(const QString &initial_path)
    {
        QWidget *central = new QWidget(this);
        QVBoxLayout *outer = new QVBoxLayout(central);

        QLabel *title = new QLabel(
            QStringLiteral("<b>fix-names %1</b>").arg(QString::fromLatin1(VERSION)),
            central);
        outer->addWidget(title);

        QHBoxLayout *path_layout = new QHBoxLayout;
        path_layout->addWidget(new QLabel(qtr(language, "Pasta:", "Folder:"), central));
        path_edit = new QLineEdit(initial_path, central);
        QPushButton *browse_button = new QPushButton(
            qtr(language, "Navegar...", "Browse..."), central);
        path_layout->addWidget(path_edit, 1);
        path_layout->addWidget(browse_button);
        outer->addLayout(path_layout);
        connect(browse_button, &QPushButton::clicked, this,
                [this] { browse_folder(); });

        QScrollArea *scroll = new QScrollArea(central);
        scroll->setWidgetResizable(true);
        QWidget *options_widget = new QWidget(scroll);
        QVBoxLayout *options_layout = new QVBoxLayout(options_widget);

        QGroupBox *transform_group = new QGroupBox(
            qtr(language, "Transformações", "Transformations"), options_widget);
        QFormLayout *form = new QFormLayout(transform_group);

        case_combo = new QComboBox(transform_group);
        case_combo->addItems(QStringList{
            qtr(language, "Sem alteração", "No change"),
            qtr(language, "Tudo maiúsculo", "All uppercase"),
            qtr(language, "Tudo minúsculo", "All lowercase"),
            qtr(language, "Inicial de cada palavra", "Capitalize each word")
        });
        form->addRow(qtr(language, "Capitalização:", "Letter case:"), case_combo);

        accents_check = new QCheckBox(
            qtr(language, "Remover acentos", "Remove accents"), transform_group);
        form->addRow(qtr(language, "Acentuação:", "Accents:"), accents_check);

        QWidget *spaces_widget = new QWidget(transform_group);
        QHBoxLayout *spaces_layout = new QHBoxLayout(spaces_widget);
        spaces_layout->setContentsMargins(0, 0, 0, 0);
        spaces_check = new QCheckBox(
            qtr(language, "Substituir espaços", "Replace spaces"), spaces_widget);
        space_edit = new QLineEdit(spaces_widget);
        space_edit->setPlaceholderText(QStringLiteral("-"));
        space_edit->setMaxLength(8);
        space_edit->setEnabled(false);
        spaces_layout->addWidget(spaces_check);
        spaces_layout->addWidget(space_edit, 1);
        form->addRow(qtr(language, "Espaços:", "Spaces:"), spaces_widget);
        connect(spaces_check, &QCheckBox::toggled, space_edit, &QWidget::setEnabled);

        QWidget *underscores_widget = new QWidget(transform_group);
        QHBoxLayout *underscores_layout = new QHBoxLayout(underscores_widget);
        underscores_layout->setContentsMargins(0, 0, 0, 0);
        underscores_check = new QCheckBox(
            qtr(language, "Substituir _", "Replace _"), underscores_widget);
        underscore_edit = new QLineEdit(underscores_widget);
        underscore_edit->setPlaceholderText(QStringLiteral("-"));
        underscore_edit->setMaxLength(8);
        underscore_edit->setEnabled(false);
        underscores_layout->addWidget(underscores_check);
        underscores_layout->addWidget(underscore_edit, 1);
        form->addRow(qtr(language, "Sublinhados:", "Underscores:"),
                     underscores_widget);
        connect(underscores_check, &QCheckBox::toggled, underscore_edit,
                &QWidget::setEnabled);

        QWidget *find_widget = new QWidget(transform_group);
        QHBoxLayout *find_layout = new QHBoxLayout(find_widget);
        find_layout->setContentsMargins(0, 0, 0, 0);
        find_check = new QCheckBox(qtr(language, "Ativar", "Enable"), find_widget);
        find_edit = new QLineEdit(find_widget);
        replace_edit = new QLineEdit(find_widget);
        find_edit->setPlaceholderText(qtr(language, "Localizar", "Find"));
        replace_edit->setPlaceholderText(qtr(language, "Substituir por", "Replace with"));
        find_edit->setEnabled(false);
        replace_edit->setEnabled(false);
        find_layout->addWidget(find_check);
        find_layout->addWidget(find_edit, 1);
        find_layout->addWidget(replace_edit, 1);
        form->addRow(qtr(language, "Localizar/substituir:", "Find/replace:"), find_widget);
        connect(find_check, &QCheckBox::toggled, find_edit, &QWidget::setEnabled);
        connect(find_check, &QCheckBox::toggled, replace_edit, &QWidget::setEnabled);

        QWidget *insert_widget = new QWidget(transform_group);
        QHBoxLayout *insert_layout = new QHBoxLayout(insert_widget);
        insert_layout->setContentsMargins(0, 0, 0, 0);
        insert_combo = new QComboBox(insert_widget);
        insert_combo->addItems(QStringList{
            qtr(language, "Desativado", "Disabled"),
            qtr(language, "Inserir", "Insert"),
            qtr(language, "Sobrescrever", "Overwrite")});
        insert_edit = new QLineEdit(insert_widget);
        insert_edit->setPlaceholderText(qtr(language, "Texto", "Text"));
        position_spin = new QSpinBox(insert_widget);
        position_spin->setRange(0, 1000000);
        from_end_check = new QCheckBox(
            qtr(language, "A partir do fim", "From end"), insert_widget);
        insert_edit->setEnabled(false);
        position_spin->setEnabled(false);
        from_end_check->setEnabled(false);
        insert_layout->addWidget(insert_combo);
        insert_layout->addWidget(insert_edit, 1);
        insert_layout->addWidget(new QLabel(qtr(language, "Posição:", "Position:"),
                                            insert_widget));
        insert_layout->addWidget(position_spin);
        insert_layout->addWidget(from_end_check);
        form->addRow(qtr(language, "Inserir/sobrescrever:", "Insert/overwrite:"),
                     insert_widget);
        connect(insert_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int index) {
                    const bool enabled = index != 0;
                    insert_edit->setEnabled(enabled);
                    position_spin->setEnabled(enabled);
                    from_end_check->setEnabled(enabled);
                });

        QWidget *scope_widget = new QWidget(transform_group);
        QHBoxLayout *scope_layout = new QHBoxLayout(scope_widget);
        scope_layout->setContentsMargins(0, 0, 0, 0);
        recursive_check = new QCheckBox(
            qtr(language, "Aplicar recursivamente", "Apply recursively"), scope_widget);
        include_extension_check = new QCheckBox(
            qtr(language, "Incluir extensões na alteração", "Include extensions"),
            scope_widget);
        scope_layout->addWidget(recursive_check);
        scope_layout->addWidget(include_extension_check);
        form->addRow(qtr(language, "Escopo:", "Scope:"), scope_widget);
        options_layout->addWidget(transform_group);

        QGroupBox *exclude_group = new QGroupBox(
            qtr(language, "Arquivos que não serão atualizados",
                "Files that will not be updated"), options_widget);
        QVBoxLayout *exclude_layout = new QVBoxLayout(exclude_group);
        exclusion_list = new QListWidget(exclude_group);
        exclusion_list->setSelectionMode(QAbstractItemView::SingleSelection);
        exclude_layout->addWidget(exclusion_list);
        QHBoxLayout *exclude_buttons = new QHBoxLayout;
        QPushButton *add_button = new QPushButton(
            qtr(language, "Adicionar...", "Add..."), exclude_group);
        QPushButton *remove_button = new QPushButton(
            qtr(language, "Remover selecionado", "Remove selected"), exclude_group);
        exclude_buttons->addWidget(add_button);
        exclude_buttons->addWidget(remove_button);
        exclude_buttons->addStretch();
        exclude_layout->addLayout(exclude_buttons);
        connect(add_button, &QPushButton::clicked, this, [this] { add_exclusions(); });
        connect(remove_button, &QPushButton::clicked, this,
                [this] { remove_exclusion(); });
        options_layout->addWidget(exclude_group);
        options_layout->addStretch();

        scroll->setWidget(options_widget);
        outer->addWidget(scroll, 1);

        QHBoxLayout *actions = new QHBoxLayout;
        QPushButton *preview_button = new QPushButton(
            qtr(language, "Pré-visualizar", "Preview"), central);
        QPushButton *apply_button = new QPushButton(
            qtr(language, "Aplicar alterações", "Apply changes"), central);
        actions->addWidget(preview_button);
        actions->addWidget(apply_button);
        actions->addStretch();
        outer->addLayout(actions);
        connect(preview_button, &QPushButton::clicked, this, [this] { run(true); });
        connect(apply_button, &QPushButton::clicked, this,
                [this] { confirm_apply(); });

        progress_bar = new QProgressBar(central);
        progress_bar->setRange(0, 100);
        progress_bar->setValue(0);
        progress_bar->setTextVisible(true);
        progress_bar->setFormat(QStringLiteral("0%"));
        outer->addWidget(progress_bar);

        log_view = new QPlainTextEdit(central);
        log_view->setReadOnly(true);
        log_view->setMinimumHeight(150);
        log_view->setPlainText(qtr(
            language,
            "Configure as opções e use Pré-visualizar antes de aplicar.",
            "Configure options and use Preview before applying."));
        outer->addWidget(log_view);
        setCentralWidget(central);
    }
};

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

    bool self_test = false;
    QString initial_path = QStringLiteral(".");
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--self-test")
            self_test = true;
        else
            initial_path = QString::fromLocal8Bit(argv[index]);
    }

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("fix-names"));
    application.setApplicationDisplayName(QStringLiteral("fix-names"));
    application.setWindowIcon(QIcon::fromTheme(
        QStringLiteral("fix-names"), QIcon(QStringLiteral(FIX_NAMES_ICON_PATH))));
    MainWindow window(initial_path, language);
    window.show();
    if (self_test)
        QTimer::singleShot(0, &application, [&application] {
            application.quit();
        });
    return application.exec();
}
