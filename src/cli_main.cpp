/*
 * Ponto de entrada da CLI/ncurses.
 *
 * Sem argumentos, fix-names abre ncurses. Com flags, executa como ferramenta
 * tradicional de terminal. Nenhuma transformação possui efeito implícito:
 * cada uma precisa ser habilitada por sua própria flag.
 */

#include "core.hpp"
#include "ncurses_ui.hpp"

#include <cerrno>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <iomanip>
#include <string>

namespace {

using namespace fixnames;

void print_help(Language language)
{
    std::cout << tr(language,
R"(Uso:
  fix-names                         Abre a interface ncurses.
  fix-names [OPÇÕES] [CAMINHO]      Executa pela linha de comando.

Transformações (todas são opcionais e ativadas por flag):
  -u, --uppercase                   Converte o nome para MAIÚSCULAS.
  -l, --lowercase                   Converte o nome para minúsculas.
  -c, --capitalize                  Coloca a inicial de cada palavra em maiúscula.
  -a, --remove-accents              Remove acentos e sinais diacríticos latinos.
  -s, --spaces CARACTERE            Substitui cada espaço pelo caractere informado.
      --underscores CARACTERE       Substitui cada _ pelo caractere informado.
  -f, --find TEXTO                  Localiza TEXTO literalmente.
  -p, --replace TEXTO               Substitui cada ocorrência pelo TEXTO informado.
      --insert TEXTO                Insere TEXTO na posição configurada.
      --overwrite TEXTO             Sobrescreve a partir da posição configurada.
      --position N                  Posição em caracteres; o padrão é zero.
      --from-end                    Conta a posição a partir do fim do nome.

Escopo e segurança:
  -r, --recursive                   Inclui subpastas e seus respectivos arquivos.
      --include-extension           Permite alterar extensões (protegidas por padrão).
  -x, --exclude CAMINHO             Ignora um item; pode ser repetida.
      --exclude-from ARQUIVO        Lê uma exclusão por linha; # inicia comentário.
  -n, --dry-run                     Apenas mostra o plano, sem alterar nada.
  -y, --yes                         Executa sem pedir confirmação.
  -i, --interactive                 Abre ncurses, opcionalmente com CAMINHO inicial.
  -h, --help                        Mostra esta ajuda.
  -V, --version                     Mostra a versão.
      --                             Encerra a leitura das opções.

Regras:
  * --uppercase, --lowercase e --capitalize são mutuamente exclusivas.
  * --find requer --replace, mesmo quando o substituto for vazio.
  * --insert e --overwrite são mutuamente exclusivos.
  * Diretórios informados têm o nome-base preservado; seu conteúdo é processado.
  * O programa nunca segue links, nunca sobrescreve destinos e recusa root.

Exemplos:
  fix-names --lowercase --remove-accents --spaces=- --dry-run ~/Documentos
  fix-names --uppercase --recursive --exclude "não alterar.txt" ~/Imagens
  fix-names --find antigo --replace novo --yes .
  fix-names --insert 2026- --position 0 --yes .
)",
R"(Usage:
  fix-names                         Opens the ncurses interface.
  fix-names [OPTIONS] [PATH]        Runs from the command line.

Transformations (all optional and enabled by flags):
  -u, --uppercase                   Converts the name to UPPERCASE.
  -l, --lowercase                   Converts the name to lowercase.
  -c, --capitalize                  Capitalizes the first letter of each word.
  -a, --remove-accents              Removes Latin accents and diacritics.
  -s, --spaces CHARACTER            Replaces each space with CHARACTER.
      --underscores CHARACTER       Replaces each _ with CHARACTER.
  -f, --find TEXT                   Finds TEXT literally.
  -p, --replace TEXT                Replaces every occurrence with TEXT.
      --insert TEXT                 Inserts TEXT at the configured position.
      --overwrite TEXT              Overwrites from the configured position.
      --position N                  Character position; defaults to zero.
      --from-end                    Counts the position from the end.

Scope and safety:
  -r, --recursive                   Includes subdirectories and their files.
      --include-extension           Allows extensions to change (protected by default).
  -x, --exclude PATH                Excludes an item; may be repeated.
      --exclude-from FILE           Reads one exclusion per line; # starts a comment.
  -n, --dry-run                     Shows the plan without changing anything.
  -y, --yes                         Runs without asking for confirmation.
  -i, --interactive                 Opens ncurses, optionally with an initial PATH.
  -h, --help                        Shows this help.
  -V, --version                     Shows the version.
      --                             Ends option parsing.

Rules:
  * --uppercase, --lowercase and --capitalize are mutually exclusive.
  * --find requires --replace, even when the replacement is empty.
  * --insert and --overwrite are mutually exclusive.
  * A directory target keeps its own name; its contents are processed.
  * The program never follows links, never overwrites destinations, and refuses root.

Examples:
  fix-names --lowercase --remove-accents --spaces=- --dry-run ~/Documents
  fix-names --uppercase --recursive --exclude "keep this.txt" ~/Pictures
  fix-names --find old --replace new --yes .
  fix-names --insert 2026- --position 0 --yes .
)");
}

bool parse_size(const char *text, std::size_t &value)
{
    if (!text || !*text || *text == '-')
        return false;
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        parsed > std::numeric_limits<std::size_t>::max())
        return false;
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool load_exclusion_file(const std::string &filename, RenamerOptions &options,
                         Language language)
{
    std::ifstream input(filename);
    if (!input) {
        std::cerr << tr(language, "ERRO: não foi possível abrir a lista: ",
                       "ERROR: could not open exclusion list: ")
                  << filename << '\n';
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line.front() == '#')
            continue;
        options.exclusions.emplace_back(line);
    }
    if (!input.eof()) {
        std::cerr << tr(language, "ERRO: falha durante a leitura da lista: ",
                       "ERROR: failed while reading exclusion list: ")
                  << filename << '\n';
        return false;
    }
    return true;
}

bool confirm(Language language)
{
    std::cout << tr(language,
                    "Aplicar as alterações acima? [s/N] ",
                    "Apply the changes above? [y/N] ") << std::flush;
    std::string answer;
    std::getline(std::cin, answer);
    if (language == Language::PortugueseBrazil)
        return answer == "s" || answer == "S" || answer == "sim" || answer == "SIM";
    return answer == "y" || answer == "Y" || answer == "yes" || answer == "YES";
}

/* Desenha uma única barra atualizável no terminal. A largura fixa mantém a
 * saída legível inclusive em TTYs simples, enquanto os contadores mostram de
 * onde o percentual foi calculado. O relatório detalhado é impresso somente
 * depois de a barra terminar, evitando que as duas saídas se sobreponham. */
void print_progress(std::size_t completed, std::size_t total,
                    Language language, int &last_percentage)
{
    const int percentage = progress_percentage(completed, total);
    if (percentage == last_percentage)
        return;
    last_percentage = percentage;

    constexpr int width = 30;
    const int filled = percentage * width / 100;
    std::cerr << '\r' << tr(language, "Progresso ", "Progress ") << '[';
    for (int index = 0; index < width; ++index)
        std::cerr << (index < filled ? '#' : '-');
    std::cerr << "] " << std::setw(3) << percentage << "% ("
              << completed << '/' << total << ')' << std::flush;
}

} // namespace

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "");
    const fixnames::Language language = fixnames::detect_language();

    /* A verificação antecede inclusive --help e --version: isso cumpre a regra
     * literal de que o aplicativo nunca deve ser usado em uma sessão root. */
#ifndef FIX_NAMES_TEST_ALLOW_ROOT
    if (fixnames::running_as_root()) {
        std::cerr << fixnames::tr(
            language,
            "ERRO: o fix-names nunca pode ser executado como root. Use uma conta comum.\n",
            "ERROR: fix-names must never run as root. Use a regular account.\n");
        return 77;
    }
#endif

    fixnames::RenamerOptions options;
    options.target = ".";
    bool assume_yes = false;
    bool interactive = argc == 1;
    bool case_selected = false;
    bool replace_option_seen = false;
    bool find_option_seen = false;
    bool insert_selected = false;
    bool overwrite_selected = false;

    enum LongOption {
        OptInsert = 1000,
        OptOverwrite,
        OptPosition,
        OptFromEnd,
        OptIncludeExtension,
        OptExcludeFrom,
        OptUnderscores
    };

    const option long_options[] = {
        {"uppercase", no_argument, nullptr, 'u'},
        {"lowercase", no_argument, nullptr, 'l'},
        {"capitalize", no_argument, nullptr, 'c'},
        {"remove-accents", no_argument, nullptr, 'a'},
        {"spaces", required_argument, nullptr, 's'},
        {"underscores", required_argument, nullptr, OptUnderscores},
        {"find", required_argument, nullptr, 'f'},
        {"replace", required_argument, nullptr, 'p'},
        {"insert", required_argument, nullptr, OptInsert},
        {"overwrite", required_argument, nullptr, OptOverwrite},
        {"position", required_argument, nullptr, OptPosition},
        {"from-end", no_argument, nullptr, OptFromEnd},
        {"recursive", no_argument, nullptr, 'r'},
        {"include-extension", no_argument, nullptr, OptIncludeExtension},
        {"exclude", required_argument, nullptr, 'x'},
        {"exclude-from", required_argument, nullptr, OptExcludeFrom},
        {"dry-run", no_argument, nullptr, 'n'},
        {"yes", no_argument, nullptr, 'y'},
        {"interactive", no_argument, nullptr, 'i'},
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'V'},
        {nullptr, 0, nullptr, 0}
    };

    for (;;) {
        const int parsed = getopt_long(argc, argv, "ulcas:f:p:rx:nyihV",
                                       long_options, nullptr);
        if (parsed == -1)
            break;

        switch (parsed) {
        case 'u': case 'l': case 'c':
            if (case_selected) {
                std::cerr << fixnames::tr(
                    language,
                    "ERRO: use somente uma opção de capitalização.\n",
                    "ERROR: use only one case-conversion option.\n");
                return 2;
            }
            case_selected = true;
            options.case_mode = parsed == 'u' ? fixnames::CaseMode::Uppercase
                              : parsed == 'l' ? fixnames::CaseMode::Lowercase
                                              : fixnames::CaseMode::CapitalizeWords;
            break;
        case 'a': options.remove_accents = true; break;
        case 's':
            options.replace_spaces = true;
            options.space_replacement = optarg;
            break;
        case OptUnderscores:
            options.replace_underscores = true;
            options.underscore_replacement = optarg;
            break;
        case 'f':
            find_option_seen = true;
            options.find_text = optarg;
            break;
        case 'p':
            replace_option_seen = true;
            options.replacement_text = optarg;
            break;
        case OptInsert:
            if (overwrite_selected || insert_selected) {
                std::cerr << fixnames::tr(language,
                    "ERRO: use somente --insert ou --overwrite.\n",
                    "ERROR: use only --insert or --overwrite.\n");
                return 2;
            }
            insert_selected = true;
            options.insert_mode = fixnames::InsertMode::Insert;
            options.insert_text = optarg;
            break;
        case OptOverwrite:
            if (insert_selected || overwrite_selected) {
                std::cerr << fixnames::tr(language,
                    "ERRO: use somente --insert ou --overwrite.\n",
                    "ERROR: use only --insert or --overwrite.\n");
                return 2;
            }
            overwrite_selected = true;
            options.insert_mode = fixnames::InsertMode::Overwrite;
            options.insert_text = optarg;
            break;
        case OptPosition:
            if (!parse_size(optarg, options.insert_position)) {
                std::cerr << fixnames::tr(language,
                    "ERRO: --position exige um número inteiro não negativo.\n",
                    "ERROR: --position requires a non-negative integer.\n");
                return 2;
            }
            break;
        case OptFromEnd: options.position_from_end = true; break;
        case 'r': options.recursive = true; break;
        case OptIncludeExtension: options.include_extension = true; break;
        case 'x': options.exclusions.emplace_back(optarg); break;
        case OptExcludeFrom:
            if (!load_exclusion_file(optarg, options, language))
                return 2;
            break;
        case 'n': options.dry_run = true; break;
        case 'y': assume_yes = true; break;
        case 'i': interactive = true; break;
        case 'h': print_help(language); return 0;
        case 'V': std::cout << "fix-names " << fixnames::VERSION << '\n'; return 0;
        default:
            std::cerr << fixnames::tr(language,
                "Use 'fix-names --help' para consultar a sintaxe.\n",
                "Use 'fix-names --help' for syntax.\n");
            return 2;
        }
    }

    if (find_option_seen != replace_option_seen) {
        std::cerr << fixnames::tr(language,
            "ERRO: --find e --replace devem ser usados juntos.\n",
            "ERROR: --find and --replace must be used together.\n");
        return 2;
    }
    options.find_replace_enabled = find_option_seen && replace_option_seen;

    if (optind < argc)
        options.target = argv[optind++];
    if (optind < argc) {
        std::cerr << fixnames::tr(language,
            "ERRO: informe somente um caminho por execução.\n",
            "ERROR: provide only one path per run.\n");
        return 2;
    }

    if (interactive)
        return fixnames::run_ncurses_interface(std::move(options), language);

    std::string validation_error;
    if (!fixnames::validate_options(options, validation_error, language)) {
        std::cerr << fixnames::tr(language, "ERRO: ", "ERROR: ")
                  << validation_error << '\n';
        return 2;
    }

    if (!options.dry_run && !assume_yes && !confirm(language)) {
        std::cout << fixnames::tr(language, "Operação cancelada.\n",
                                 "Operation cancelled.\n");
        return 0;
    }

    int last_percentage = -1;
    const fixnames::RunResult result = fixnames::run_renamer(
        options, language, {},
        [&language, &last_percentage](std::size_t completed, std::size_t total) {
            print_progress(completed, total, language, last_percentage);
        });
    if (last_percentage >= 0)
        std::cerr << '\n';
    for (const std::string &line : result.messages)
        std::cout << line << '\n';
    return result.success ? 0 : 1;
}
