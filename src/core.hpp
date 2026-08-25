#pragma once

/*
 * Núcleo compartilhado do fix-names.
 *
 * Este cabeçalho contém somente tipos independentes de interface. A CLI,
 * ncurses, GTK e Qt montam uma configuração RenamerOptions e chamam a mesma
 * função run_renamer(). Isso impede que uma interface aplique regras de
 * renomeação diferentes das demais.
 */

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fixnames {

inline constexpr const char *VERSION = "2.1.2";

enum class Language {
    English,
    PortugueseBrazil
};

enum class CaseMode {
    None,
    Uppercase,
    Lowercase,
    CapitalizeWords
};

enum class InsertMode {
    None,
    Insert,
    Overwrite
};

struct RenamerOptions {
    /* Caminho escolhido. Diretórios têm o próprio nome preservado e apenas
     * seu conteúdo é processado. Arquivos/links informados diretamente podem
     * ser renomeados. */
    std::filesystem::path target;

    /* Todas as transformações ficam desativadas até que uma flag ou controle
     * de interface as habilite explicitamente. */
    CaseMode case_mode = CaseMode::None;
    bool remove_accents = false;
    bool replace_spaces = false;
    std::string space_replacement;

    /* Compatibilidade com a regra do código-base anterior. Sublinhados só
     * mudam quando esta opção independente é ativada. */
    bool replace_underscores = false;
    std::string underscore_replacement;

    bool find_replace_enabled = false;
    std::string find_text;
    std::string replacement_text;

    InsertMode insert_mode = InsertMode::None;
    std::string insert_text;
    std::size_t insert_position = 0;
    bool position_from_end = false;

    /* A extensão é protegida por padrão. Somente --include-extension ou o
     * controle equivalente nas interfaces permite alterá-la. */
    bool include_extension = false;

    /* A recursão é desativada por padrão. */
    bool recursive = false;

    /* dry_run produz o plano sem alterar o sistema de arquivos. */
    bool dry_run = false;

    /* Cada entrada pode ser absoluta ou relativa ao diretório-alvo. Um
     * diretório excluído protege também todo o conteúdo abaixo dele. */
    std::vector<std::filesystem::path> exclusions;
};

struct RunStats {
    std::size_t planned = 0;
    std::size_t renamed = 0;
    std::size_t unchanged = 0;
    std::size_t excluded = 0;
    std::size_t conflicts = 0;
    std::size_t errors = 0;
};

struct RunResult {
    bool success = false;
    RunStats stats;
    std::vector<std::string> messages;
};

using LogCallback = std::function<void(const std::string &)>;

/* O núcleo informa progresso em unidades reais de itens do sistema de
 * arquivos. O primeiro valor é a quantidade concluída e o segundo é o total
 * contado antes da operação. As interfaces convertem essa razão em 0--100% e
 * escolhem como desenhar sua própria barra. */
using ProgressCallback = std::function<void(std::size_t, std::size_t)>;

/* Converte os contadores do callback em 0--100 sem multiplicar primeiro dois
 * inteiros size_t. Essa função compartilhada evita estouro aritmético e
 * garante que CLI, ncurses, GTK e Qt exibam exatamente o mesmo percentual. */
int progress_percentage(std::size_t completed, std::size_t total) noexcept;

/* Detecta pt* pelas variáveis LC_ALL, LC_MESSAGES e LANG, nessa ordem. */
Language detect_language();

/* Retorna uma mensagem em português ou inglês sem misturar os idiomas. */
std::string tr(Language language, const char *pt_br, const char *english);

/* Verificação usada em todas as interfaces e repetida dentro do núcleo. */
bool running_as_root();

/* Valida combinações de opções antes de enumerar qualquer arquivo. */
bool validate_options(const RenamerOptions &options, std::string &error,
                      Language language);

/* Função pura usada pelas interfaces para pré-visualizações pontuais e pelos
 * testes. is_directory controla se a lógica de extensão será aplicada. */
std::string transform_name(const std::string &name, bool is_directory,
                           const RenamerOptions &options);

/* Executa ou simula o lote. O núcleo nunca segue links simbólicos, nunca
 * sobrescreve um destino e recusa execução quando EUID == 0. */
RunResult run_renamer(const RenamerOptions &options, Language language,
                      const LogCallback &callback = {},
                      const ProgressCallback &progress_callback = {});

/* Formata caminhos contendo controles sem alterar o caminho real. */
std::string display_path(const std::filesystem::path &path);

} // namespace fixnames
