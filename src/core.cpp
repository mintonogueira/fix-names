#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * Implementação do núcleo seguro do fix-names.
 *
 * A execução é dividida em quatro fases:
 *   1. contagem segura dos itens para a barra de progresso;
 *   2. enumeração e cálculo do novo nome;
 *   3. detecção de colisões dentro de cada diretório;
 *   4. renomeação em duas etapas (nome original -> temporário -> final).
 *
 * A etapa temporária permite trocar nomes e resolver cadeias como A -> B e
 * B -> C sem sobrescrever nada. Todas as renomeações usam renameat2() com
 * RENAME_NOREPLACE; se o kernel não oferecer a operação atômica, o programa
 * falha com segurança em vez de recorrer a rename(), que poderia sobrescrever.
 */

#include "core.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif

namespace fs = std::filesystem;

namespace fixnames {
namespace {

struct Utf8Token {
    char32_t codepoint = 0;
    std::string raw;
    bool valid = false;
};

struct EntryPlan {
    fs::path original_path;
    std::string original_name;
    std::string final_name;
    bool directory = false;
    bool symlink = false;
    bool excluded = false;
    bool active = false;
    bool conflict = false;
    std::string conflict_reason;
    std::string temporary_name;
};

struct ExecutionContext {
    const RenamerOptions &options;
    Language language;
    RunResult result;
    LogCallback callback;
    ProgressCallback progress_callback;
    std::vector<fs::path> normalized_exclusions;
    fs::path executable_path;
    unsigned long long temporary_counter = 0;
    std::size_t progress_current = 0;
    std::size_t progress_total = 0;
};

/* Decodifica um ponto Unicode. Bytes inválidos são devolvidos como tokens
 * individuais e permanecem byte por byte iguais no nome de saída. */
std::vector<Utf8Token> decode_utf8(const std::string &text)
{
    std::vector<Utf8Token> tokens;
    const auto *input = reinterpret_cast<const unsigned char *>(text.data());
    std::size_t offset = 0;

    while (offset < text.size()) {
        const unsigned char first = input[offset];
        char32_t cp = 0;
        std::size_t needed = 0;

        if (first < 0x80) {
            cp = first;
            needed = 1;
        } else if (first >= 0xC2 && first <= 0xDF) {
            cp = first & 0x1F;
            needed = 2;
        } else if (first >= 0xE0 && first <= 0xEF) {
            cp = first & 0x0F;
            needed = 3;
        } else if (first >= 0xF0 && first <= 0xF4) {
            cp = first & 0x07;
            needed = 4;
        }

        bool valid = needed != 0 && offset + needed <= text.size();
        if (valid) {
            for (std::size_t index = 1; index < needed; ++index) {
                const unsigned char next = input[offset + index];
                if ((next & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
                cp = (cp << 6) | (next & 0x3F);
            }
            if ((needed == 2 && cp < 0x80) ||
                (needed == 3 && cp < 0x800) ||
                (needed == 4 && cp < 0x10000) ||
                (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
                valid = false;
            }
        }

        if (!valid) {
            tokens.push_back({static_cast<char32_t>(first),
                              text.substr(offset, 1), false});
            ++offset;
        } else {
            tokens.push_back({cp, text.substr(offset, needed), true});
            offset += needed;
        }
    }
    return tokens;
}

std::string encode_utf8(char32_t cp)
{
    std::string output;
    if (cp <= 0x7F) {
        output.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return output;
}

bool is_combining_mark(char32_t cp)
{
    return (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}

/* Tabela explícita para letras latinas precompostas. Ela complementa a
 * remoção de marcas combinantes e mantém o comportamento determinístico mesmo
 * quando a localidade do sistema possui suporte Unicode incompleto. */
const char *fold_latin(char32_t cp)
{
    switch (cp) {
    case U'À': case U'Á': case U'Â': case U'Ã': case U'Ä': case U'Å':
    case U'à': case U'á': case U'â': case U'ã': case U'ä': case U'å':
    case 0x0100: case 0x0101: case 0x0102: case 0x0103:
    case 0x0104: case 0x0105: case 0x01CD: case 0x01CE:
    case 0x01DE: case 0x01DF: case 0x01E0: case 0x01E1:
    case 0x01FA: case 0x01FB: case 0x0200: case 0x0201:
    case 0x0202: case 0x0203: case 0x0226: case 0x0227:
    case 0x1EA0: case 0x1EA1: case 0x1EA2: case 0x1EA3:
    case 0x1EA4: case 0x1EA5: case 0x1EA6: case 0x1EA7:
    case 0x1EA8: case 0x1EA9: case 0x1EAA: case 0x1EAB:
    case 0x1EAC: case 0x1EAD: case 0x1EAE: case 0x1EAF:
    case 0x1EB0: case 0x1EB1: case 0x1EB2: case 0x1EB3:
    case 0x1EB4: case 0x1EB5: case 0x1EB6: case 0x1EB7:
        return "a";
    case U'Æ': case U'æ': case 0x01E2: case 0x01E3:
    case 0x01FC: case 0x01FD:
        return "ae";
    case U'Ç': case U'ç': case 0x0106: case 0x0107:
    case 0x0108: case 0x0109: case 0x010A: case 0x010B:
    case 0x010C: case 0x010D:
        return "c";
    case U'Ð': case U'ð': case 0x010E: case 0x010F:
    case 0x0110: case 0x0111:
        return "d";
    case U'È': case U'É': case U'Ê': case U'Ë':
    case U'è': case U'é': case U'ê': case U'ë':
    case 0x0112: case 0x0113: case 0x0114: case 0x0115:
    case 0x0116: case 0x0117: case 0x0118: case 0x0119:
    case 0x011A: case 0x011B: case 0x0204: case 0x0205:
    case 0x0206: case 0x0207: case 0x0228: case 0x0229:
    case 0x1EB8: case 0x1EB9: case 0x1EBA: case 0x1EBB:
    case 0x1EBC: case 0x1EBD: case 0x1EBE: case 0x1EBF:
    case 0x1EC0: case 0x1EC1: case 0x1EC2: case 0x1EC3:
    case 0x1EC4: case 0x1EC5: case 0x1EC6: case 0x1EC7:
        return "e";
    case 0x011C: case 0x011D: case 0x011E: case 0x011F:
    case 0x0120: case 0x0121: case 0x0122: case 0x0123:
    case 0x01E4: case 0x01E5: case 0x01E6: case 0x01E7:
    case 0x01F4: case 0x01F5:
        return "g";
    case 0x0124: case 0x0125: case 0x0126: case 0x0127:
        return "h";
    case U'Ì': case U'Í': case U'Î': case U'Ï':
    case U'ì': case U'í': case U'î': case U'ï':
    case 0x0128: case 0x0129: case 0x012A: case 0x012B:
    case 0x012C: case 0x012D: case 0x012E: case 0x012F:
    case 0x0130: case 0x01CF: case 0x01D0: case 0x0208:
    case 0x0209: case 0x020A: case 0x020B: case 0x1EC8:
    case 0x1EC9: case 0x1ECA: case 0x1ECB:
        return "i";
    case 0x0134: case 0x0135:
        return "j";
    case 0x0136: case 0x0137: case 0x0138:
        return "k";
    case 0x0139: case 0x013A: case 0x013B: case 0x013C:
    case 0x013D: case 0x013E: case 0x013F: case 0x0140:
    case 0x0141: case 0x0142:
        return "l";
    case U'Ñ': case U'ñ': case 0x0143: case 0x0144:
    case 0x0145: case 0x0146: case 0x0147: case 0x0148:
    case 0x0149: case 0x014A: case 0x014B: case 0x01F8:
    case 0x01F9:
        return "n";
    case U'Ò': case U'Ó': case U'Ô': case U'Õ': case U'Ö': case U'Ø':
    case U'ò': case U'ó': case U'ô': case U'õ': case U'ö': case U'ø':
    case 0x014C: case 0x014D: case 0x014E: case 0x014F:
    case 0x0150: case 0x0151: case 0x01A0: case 0x01A1:
    case 0x01D1: case 0x01D2: case 0x01EA: case 0x01EB:
    case 0x01EC: case 0x01ED: case 0x01FE: case 0x01FF:
    case 0x020C: case 0x020D: case 0x020E: case 0x020F:
    case 0x022A: case 0x022B: case 0x022C: case 0x022D:
    case 0x022E: case 0x022F: case 0x0230: case 0x0231:
    case 0x1ECC: case 0x1ECD: case 0x1ECE: case 0x1ECF:
    case 0x1ED0: case 0x1ED1: case 0x1ED2: case 0x1ED3:
    case 0x1ED4: case 0x1ED5: case 0x1ED6: case 0x1ED7:
    case 0x1ED8: case 0x1ED9: case 0x1EDA: case 0x1EDB:
    case 0x1EDC: case 0x1EDD: case 0x1EDE: case 0x1EDF:
    case 0x1EE0: case 0x1EE1: case 0x1EE2: case 0x1EE3:
        return "o";
    case U'Œ': case U'œ':
        return "oe";
    case 0x0154: case 0x0155: case 0x0156: case 0x0157:
    case 0x0158: case 0x0159:
        return "r";
    case U'ß':
        return "ss";
    case 0x015A: case 0x015B: case 0x015C: case 0x015D:
    case 0x015E: case 0x015F: case 0x0160: case 0x0161:
        return "s";
    case U'Þ': case U'þ':
        return "th";
    case 0x0162: case 0x0163: case 0x0164: case 0x0165:
    case 0x0166: case 0x0167:
        return "t";
    case U'Ù': case U'Ú': case U'Û': case U'Ü':
    case U'ù': case U'ú': case U'û': case U'ü':
    case 0x0168: case 0x0169: case 0x016A: case 0x016B:
    case 0x016C: case 0x016D: case 0x016E: case 0x016F:
    case 0x0170: case 0x0171: case 0x0172: case 0x0173:
    case 0x01AF: case 0x01B0: case 0x01D3: case 0x01D4:
    case 0x01D5: case 0x01D6: case 0x01D7: case 0x01D8:
    case 0x01D9: case 0x01DA: case 0x01DB: case 0x01DC:
    case 0x0214: case 0x0215: case 0x0216: case 0x0217:
    case 0x1EE4: case 0x1EE5: case 0x1EE6: case 0x1EE7:
    case 0x1EE8: case 0x1EE9: case 0x1EEA: case 0x1EEB:
    case 0x1EEC: case 0x1EED: case 0x1EEE: case 0x1EEF:
    case 0x1EF0: case 0x1EF1:
        return "u";
    case 0x0174: case 0x0175:
        return "w";
    case U'Ý': case U'ý': case U'ÿ': case 0x0176: case 0x0177:
    case 0x0178: case 0x1EF2: case 0x1EF3: case 0x1EF4:
    case 0x1EF5: case 0x1EF6: case 0x1EF7: case 0x1EF8:
    case 0x1EF9:
        return "y";
    case 0x0179: case 0x017A: case 0x017B: case 0x017C:
    case 0x017D: case 0x017E:
        return "z";
    default:
        return nullptr;
    }
}

std::string remove_accents(const std::string &text)
{
    std::string output;
    for (const Utf8Token &token : decode_utf8(text)) {
        if (!token.valid) {
            output += token.raw;
            continue;
        }
        if (is_combining_mark(token.codepoint))
            continue;
        if (const char *folded = fold_latin(token.codepoint))
            output += folded;
        else
            output += token.raw;
    }
    return output;
}

char32_t convert_case(char32_t cp, bool uppercase)
{
    const wint_t converted = uppercase ? std::towupper(static_cast<wint_t>(cp))
                                      : std::towlower(static_cast<wint_t>(cp));
    if (converted == WEOF)
        return cp;
    return static_cast<char32_t>(converted);
}

bool is_word_character(char32_t cp)
{
    if (cp < 128)
        return std::isalnum(static_cast<unsigned char>(cp)) != 0;
    return std::iswalnum(static_cast<wint_t>(cp)) != 0;
}

std::string apply_case(const std::string &text, CaseMode mode)
{
    if (mode == CaseMode::None)
        return text;

    std::string output;
    bool at_word_start = true;
    for (const Utf8Token &token : decode_utf8(text)) {
        if (!token.valid) {
            output += token.raw;
            at_word_start = true;
            continue;
        }

        char32_t cp = token.codepoint;
        if (mode == CaseMode::Uppercase)
            cp = convert_case(cp, true);
        else if (mode == CaseMode::Lowercase)
            cp = convert_case(cp, false);
        else if (mode == CaseMode::CapitalizeWords) {
            if (is_word_character(cp)) {
                cp = convert_case(cp, at_word_start);
                at_word_start = false;
            } else {
                at_word_start = true;
            }
        }
        output += encode_utf8(cp);
    }
    return output;
}

std::string replace_all(std::string text, const std::string &needle,
                        const std::string &replacement)
{
    if (needle.empty())
        return text;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
    return text;
}

std::string replace_ascii_spaces(const std::string &text,
                                 const std::string &replacement)
{
    std::string output;
    for (char byte : text) {
        if (byte == ' ')
            output += replacement;
        else
            output.push_back(byte);
    }
    return output;
}

std::string replace_ascii_underscores(const std::string &text,
                                      const std::string &replacement)
{
    std::string output;
    for (char byte : text) {
        if (byte == '_')
            output += replacement;
        else
            output.push_back(byte);
    }
    return output;
}

std::string insert_or_overwrite(const std::string &text,
                                const RenamerOptions &options)
{
    if (options.insert_mode == InsertMode::None)
        return text;

    std::vector<Utf8Token> original = decode_utf8(text);
    const std::vector<Utf8Token> addition = decode_utf8(options.insert_text);
    const std::size_t length = original.size();
    std::size_t position = options.insert_position;

    if (options.position_from_end)
        position = position > length ? 0 : length - position;
    else if (position > length)
        position = length;

    std::string output;
    for (std::size_t index = 0; index < position; ++index)
        output += original[index].raw;
    output += options.insert_text;

    std::size_t resume = position;
    if (options.insert_mode == InsertMode::Overwrite)
        resume = std::min(length, position + addition.size());
    for (std::size_t index = resume; index < length; ++index)
        output += original[index].raw;
    return output;
}

bool is_one_utf8_character(const std::string &text)
{
    const auto tokens = decode_utf8(text);
    return tokens.size() == 1 && tokens.front().valid;
}

bool valid_final_basename(const std::string &name)
{
    return !name.empty() && name != "." && name != ".." &&
           name.find('/') == std::string::npos &&
           name.find('\0') == std::string::npos;
}

std::pair<std::string, std::string>
split_extension(const std::string &name, bool is_directory,
                bool include_extension)
{
    if (is_directory || include_extension)
        return {name, {}};

    /* O primeiro ponto que não seja o ponto inicial inicia a extensão. Essa
     * regra preserva extensões compostas byte por byte. */
    const std::size_t search_from = !name.empty() && name.front() == '.' ? 1 : 0;
    const std::size_t dot = name.find('.', search_from);
    if (dot == std::string::npos)
        return {name, {}};
    return {name.substr(0, dot), name.substr(dot)};
}

fs::path absolute_lexical(const fs::path &path, std::error_code &error)
{
    fs::path result = fs::absolute(path, error);
    if (error)
        return {};
    return result.lexically_normal();
}

bool path_is_same_or_below(const fs::path &candidate, const fs::path &root)
{
    auto candidate_it = candidate.begin();
    auto root_it = root.begin();
    for (; root_it != root.end(); ++root_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *candidate_it != *root_it)
            return false;
    }
    return true;
}

bool status_exists(const fs::path &path)
{
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    return !error && status.type() != fs::file_type::not_found;
}

void emit(ExecutionContext &context, const std::string &message)
{
    context.result.messages.push_back(message);
    if (context.callback)
        context.callback(message);
}

bool is_excluded(const ExecutionContext &context, const fs::path &path)
{
    std::error_code error;
    const fs::path absolute = absolute_lexical(path, error);
    if (error)
        return false;

    if (!context.executable_path.empty() && absolute == context.executable_path)
        return true;
    for (const fs::path &excluded : context.normalized_exclusions) {
        if (path_is_same_or_below(absolute, excluded))
            return true;
    }
    return false;
}

/* Atualiza as interfaces somente por meio do callback. O valor concluído é
 * limitado ao total para que alterações simultâneas no diretório (um arquivo
 * criado depois da contagem, por exemplo) nunca produzam percentual acima de
 * 100%. */
void report_progress(ExecutionContext &context)
{
    if (context.progress_callback) {
        context.progress_callback(
            std::min(context.progress_current, context.progress_total),
            context.progress_total);
    }
}

void advance_progress(ExecutionContext &context, std::size_t amount = 1)
{
    context.progress_current += amount;
    report_progress(context);
}

/* Faz uma passagem exclusivamente de leitura para obter o denominador da
 * barra. Ela replica as regras da execução: não segue links, não desce em
 * diretórios excluídos e só visita subpastas quando a recursão está ativa.
 * Erros de leitura não são registrados nesta fase, pois a passagem efetiva os
 * relatará uma única vez. */
std::size_t count_directory_entries(const fs::path &directory,
                                    const ExecutionContext &context)
{
    std::error_code error;
    fs::directory_iterator iterator(directory,
                                    fs::directory_options::skip_permission_denied,
                                    error);
    if (error)
        return 0;

    std::size_t total = 0;
    for (const fs::directory_entry &entry : iterator) {
        ++total;
        if (!context.options.recursive || is_excluded(context, entry.path()))
            continue;

        std::error_code status_error;
        const fs::file_status status = entry.symlink_status(status_error);
        if (!status_error && fs::is_directory(status) && !fs::is_symlink(status))
            total += count_directory_entries(entry.path(), context);
    }
    return total;
}

int rename_noreplace(const fs::path &source, const fs::path &destination)
{
#ifdef SYS_renameat2
    return static_cast<int>(::syscall(SYS_renameat2, AT_FDCWD,
                                      source.c_str(), AT_FDCWD,
                                      destination.c_str(), RENAME_NOREPLACE));
#else
    (void)source;
    (void)destination;
    errno = ENOSYS;
    return -1;
#endif
}

std::string temporary_name(ExecutionContext &context, const fs::path &parent)
{
    for (;;) {
        std::ostringstream stream;
        stream << ".fix-names-tmp-" << static_cast<unsigned long>(::getpid())
               << '-' << ++context.temporary_counter;
        const std::string candidate = stream.str();
        if (!status_exists(parent / candidate))
            return candidate;
    }
}

std::string strerror_string(int number)
{
    return std::string(std::strerror(number));
}

void mark_conflicts(std::vector<EntryPlan> &entries, const fs::path &parent,
                    ExecutionContext &context)
{
    std::unordered_map<std::string, std::vector<std::size_t>> by_target;

    for (std::size_t index = 0; index < entries.size(); ++index) {
        EntryPlan &entry = entries[index];
        if (!entry.excluded && !entry.conflict &&
            entry.original_name != entry.final_name) {
            entry.active = true;
            ++context.result.stats.planned;
            by_target[entry.final_name].push_back(index);
        }
    }

    /* Se dois itens gerarem o mesmo destino, nenhum dos dois é escolhido
     * arbitrariamente. Ambos são preservados e registrados como conflito. */
    for (const auto &[target, indexes] : by_target) {
        if (indexes.size() <= 1)
            continue;
        for (std::size_t index : indexes) {
            entries[index].active = false;
            entries[index].conflict = true;
            entries[index].conflict_reason = tr(
                context.language,
                "mais de um item produziria o mesmo nome",
                "more than one item would produce the same name");
        }
        (void)target;
    }

    /* A análise é iterativa: quando um item deixa de participar por conflito,
     * seu nome original volta a bloquear qualquer outro destino igual. */
    bool changed;
    do {
        changed = false;
        std::unordered_set<std::string> active_sources;
        for (const EntryPlan &entry : entries) {
            if (entry.active)
                active_sources.insert(entry.original_name);
        }

        for (EntryPlan &entry : entries) {
            if (!entry.active)
                continue;
            if (status_exists(parent / entry.final_name) &&
                active_sources.count(entry.final_name) == 0) {
                entry.active = false;
                entry.conflict = true;
                entry.conflict_reason = tr(
                    context.language,
                    "o destino já existe e não será sobrescrito",
                    "the destination exists and will not be overwritten");
                changed = true;
            }
        }
    } while (changed);

    for (EntryPlan &entry : entries) {
        if (!entry.conflict)
            continue;
        ++context.result.stats.conflicts;
        emit(context, tr(context.language, "[CONFLITO] ", "[CONFLICT] ") +
             display_path(entry.original_path) + " -> " +
             display_path(parent / entry.final_name) + " (" +
             entry.conflict_reason + ")");
        advance_progress(context);
    }
}

bool rollback_group(const fs::path &parent, std::vector<EntryPlan *> entries,
                    std::size_t finalized_count, ExecutionContext &context)
{
    bool rollback_ok = true;

    /* Volta primeiro os destinos finais já concluídos para seus temporários. */
    for (std::size_t index = 0; index < finalized_count; ++index) {
        EntryPlan *entry = entries[index];
        if (rename_noreplace(parent / entry->final_name,
                             parent / entry->temporary_name) != 0) {
            rollback_ok = false;
            emit(context, tr(context.language,
                             "[ERRO CRÍTICO] Falha ao reverter ",
                             "[CRITICAL ERROR] Failed to roll back ") +
                 display_path(parent / entry->final_name) + ": " +
                 strerror_string(errno));
        }
    }

    /* Depois restaura todos os nomes originais. */
    for (EntryPlan *entry : entries) {
        if (!status_exists(parent / entry->temporary_name))
            continue;
        if (rename_noreplace(parent / entry->temporary_name,
                             parent / entry->original_name) != 0) {
            rollback_ok = false;
            emit(context, tr(context.language,
                             "[ERRO CRÍTICO] Falha ao restaurar ",
                             "[CRITICAL ERROR] Failed to restore ") +
                 display_path(parent / entry->original_name) + ": " +
                 strerror_string(errno));
        }
    }
    return rollback_ok;
}

void execute_group(const fs::path &parent, std::vector<EntryPlan> &entries,
                   ExecutionContext &context)
{
    mark_conflicts(entries, parent, context);

    std::vector<EntryPlan *> active;
    for (EntryPlan &entry : entries) {
        if (entry.excluded) {
            ++context.result.stats.excluded;
            emit(context, tr(context.language, "[IGNORADO] ", "[EXCLUDED] ") +
                 display_path(entry.original_path));
            advance_progress(context);
        } else if (entry.original_name == entry.final_name) {
            ++context.result.stats.unchanged;
            advance_progress(context);
        } else if (entry.active) {
            active.push_back(&entry);
        }
    }

    if (context.options.dry_run) {
        for (EntryPlan *entry : active) {
            emit(context, tr(context.language, "[SIMULAÇÃO] ", "[DRY RUN] ") +
                 display_path(parent / entry->original_name) + " -> " +
                 display_path(parent / entry->final_name));
            advance_progress(context);
        }
        return;
    }

    std::vector<EntryPlan *> moved_to_temporary;
    for (EntryPlan *entry : active) {
        entry->temporary_name = temporary_name(context, parent);
        if (rename_noreplace(parent / entry->original_name,
                             parent / entry->temporary_name) != 0) {
            const int saved_errno = errno;
            ++context.result.stats.errors;
            emit(context, tr(context.language,
                             "[ERRO] Não foi possível preparar ",
                             "[ERROR] Could not prepare ") +
                 display_path(parent / entry->original_name) + ": " +
                 strerror_string(saved_errno));
            rollback_group(parent, moved_to_temporary, 0, context);
            advance_progress(context, active.size());
            return;
        }
        moved_to_temporary.push_back(entry);
    }

    std::size_t finalized = 0;
    for (EntryPlan *entry : moved_to_temporary) {
        if (rename_noreplace(parent / entry->temporary_name,
                             parent / entry->final_name) != 0) {
            const int saved_errno = errno;
            ++context.result.stats.errors;
            emit(context, tr(context.language,
                             "[ERRO] Não foi possível concluir ",
                             "[ERROR] Could not complete ") +
                 display_path(parent / entry->final_name) + ": " +
                 strerror_string(saved_errno));
            rollback_group(parent, moved_to_temporary, finalized, context);
            /* Os itens já finalizados informaram progresso. O item que falhou
             * e os restantes também foram concluídos como tentativas após a
             * reversão segura do lote. */
            advance_progress(context, active.size() - finalized);
            return;
        }
        ++finalized;
        advance_progress(context);
    }

    for (EntryPlan *entry : active) {
        ++context.result.stats.renamed;
        emit(context, tr(context.language, "[RENOMEADO] ", "[RENAMED] ") +
             display_path(parent / entry->original_name) + " -> " +
             display_path(parent / entry->final_name));
    }
}

EntryPlan build_entry(const fs::directory_entry &directory_entry,
                      ExecutionContext &context)
{
    EntryPlan plan;
    plan.original_path = directory_entry.path();
    plan.original_name = directory_entry.path().filename().string();

    std::error_code error;
    const fs::file_status status = directory_entry.symlink_status(error);
    if (error) {
        ++context.result.stats.errors;
        emit(context, tr(context.language, "[ERRO] Não foi possível examinar ",
                         "[ERROR] Could not inspect ") +
             display_path(directory_entry.path()) + ": " + error.message());
        /* Um item que não pôde ser classificado não é renomeado às cegas. */
        plan.excluded = true;
    }
    plan.symlink = !error && fs::is_symlink(status);
    plan.directory = !error && fs::is_directory(status) && !plan.symlink;
    plan.excluded = plan.excluded || is_excluded(context, plan.original_path);
    plan.final_name = transform_name(plan.original_name, plan.directory,
                                     context.options);
    if (!valid_final_basename(plan.final_name)) {
        plan.conflict = true;
        plan.conflict_reason = tr(
            context.language,
            "a transformação produziria um nome vazio, reservado ou contendo '/'",
            "the transformation would produce an empty, reserved, or slash-containing name");
    }
    return plan;
}

void process_directory(const fs::path &directory, ExecutionContext &context)
{
    std::error_code error;
    fs::directory_iterator iterator(directory,
                                    fs::directory_options::skip_permission_denied,
                                    error);
    if (error) {
        ++context.result.stats.errors;
        emit(context, tr(context.language, "[ERRO] Não foi possível abrir ",
                         "[ERROR] Could not open ") +
             display_path(directory) + ": " + error.message());
        return;
    }

    std::vector<EntryPlan> entries;
    for (const fs::directory_entry &entry : iterator) {
        EntryPlan plan = build_entry(entry, context);
        entries.push_back(std::move(plan));
    }

    /* A descida ocorre antes do grupo atual ser renomeado. Por isso todos os
     * caminhos continuam válidos durante a travessia, inclusive quando uma
     * pasta receberá outro nome ao fim desta chamada. */
    if (context.options.recursive) {
        for (EntryPlan &entry : entries) {
            if (entry.directory && !entry.excluded)
                process_directory(entry.original_path, context);
        }
    }

    execute_group(directory, entries, context);
}

fs::path read_executable_path()
{
    std::vector<char> buffer(4096);
    for (;;) {
        const ssize_t length = ::readlink("/proc/self/exe", buffer.data(),
                                          buffer.size() - 1);
        if (length < 0)
            return {};
        if (static_cast<std::size_t>(length) < buffer.size() - 1) {
            buffer[static_cast<std::size_t>(length)] = '\0';
            std::error_code error;
            return absolute_lexical(fs::path(buffer.data()), error);
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

Language detect_language()
{
    const char *locale_name = std::getenv("LC_ALL");
    if (!locale_name || !*locale_name)
        locale_name = std::getenv("LC_MESSAGES");
    if (!locale_name || !*locale_name)
        locale_name = std::getenv("LANG");
    if (locale_name && (locale_name[0] == 'p' || locale_name[0] == 'P') &&
        (locale_name[1] == 't' || locale_name[1] == 'T'))
        return Language::PortugueseBrazil;
    return Language::English;
}

std::string tr(Language language, const char *pt_br, const char *english)
{
    return language == Language::PortugueseBrazil ? pt_br : english;
}

bool running_as_root()
{
    return ::geteuid() == 0;
}

bool validate_options(const RenamerOptions &options, std::string &error,
                      Language language)
{
    if (options.target.empty()) {
        error = tr(language, "nenhum caminho foi informado.",
                   "no path was provided.");
        return false;
    }

    if (options.case_mode == CaseMode::None && !options.remove_accents &&
        !options.replace_spaces && !options.replace_underscores &&
        !options.find_replace_enabled &&
        options.insert_mode == InsertMode::None) {
        error = tr(language, "ative pelo menos uma transformação.",
                   "enable at least one transformation.");
        return false;
    }

    if (options.replace_spaces) {
        if (!is_one_utf8_character(options.space_replacement) ||
            options.space_replacement.find('\0') != std::string::npos ||
            options.space_replacement == "/") {
            error = tr(language,
                       "o substituto de espaços deve ser exatamente um caractere UTF-8 diferente de '/'.",
                       "the space replacement must be exactly one UTF-8 character other than '/'.");
            return false;
        }
    }
    if (options.replace_underscores) {
        if (!is_one_utf8_character(options.underscore_replacement) ||
            options.underscore_replacement.find('\0') != std::string::npos ||
            options.underscore_replacement == "/") {
            error = tr(language,
                       "o substituto de sublinhados deve ser exatamente um caractere UTF-8 diferente de '/'.",
                       "the underscore replacement must be exactly one UTF-8 character other than '/'.");
            return false;
        }
    }

    if (options.find_replace_enabled && options.find_text.empty()) {
        error = tr(language, "o texto de busca não pode estar vazio.",
                   "the search text cannot be empty.");
        return false;
    }
    if (options.find_replace_enabled &&
        (options.find_text.find('\0') != std::string::npos ||
         options.replacement_text.find('\0') != std::string::npos)) {
        error = tr(language, "textos de busca/substituição não podem conter NUL.",
                   "find/replacement text cannot contain NUL.");
        return false;
    }
    if (options.find_replace_enabled &&
        options.replacement_text.find('/') != std::string::npos) {
        error = tr(language, "o texto substituto não pode conter '/'.",
                   "the replacement text cannot contain '/'.");
        return false;
    }

    if (options.insert_mode != InsertMode::None && options.insert_text.empty()) {
        error = tr(language, "o texto de inserção/sobrescrita não pode estar vazio.",
                   "the insert/overwrite text cannot be empty.");
        return false;
    }
    if (options.insert_mode != InsertMode::None &&
        options.insert_text.find('\0') != std::string::npos) {
        error = tr(language, "o texto inserido não pode conter NUL.",
                   "inserted text cannot contain NUL.");
        return false;
    }
    if (options.insert_mode != InsertMode::None &&
        options.insert_text.find('/') != std::string::npos) {
        error = tr(language, "o texto inserido não pode conter '/'.",
                   "inserted text cannot contain '/'.");
        return false;
    }
    return true;
}

std::string transform_name(const std::string &name, bool is_directory,
                           const RenamerOptions &options)
{
    auto [base, extension] = split_extension(name, is_directory,
                                              options.include_extension);

    /* Ordem deliberadamente única em todas as interfaces:
     * acentos -> localizar/substituir -> inserir/sobrescrever -> espaços ->
     * capitalização. A extensão protegida é recolocada somente no fim. */
    if (options.remove_accents)
        base = remove_accents(base);
    if (options.find_replace_enabled)
        base = replace_all(base, options.find_text, options.replacement_text);
    base = insert_or_overwrite(base, options);
    if (options.replace_spaces)
        base = replace_ascii_spaces(base, options.space_replacement);
    if (options.replace_underscores)
        base = replace_ascii_underscores(base, options.underscore_replacement);
    base = apply_case(base, options.case_mode);
    return base + extension;
}

RunResult run_renamer(const RenamerOptions &options, Language language,
                      const LogCallback &callback,
                      const ProgressCallback &progress_callback)
{
    ExecutionContext context{options, language, {}, callback, progress_callback,
                             {}, {}, 0, 0, 0};

#ifndef FIX_NAMES_TEST_ALLOW_ROOT
    if (running_as_root()) {
        context.result.stats.errors = 1;
        emit(context, tr(language,
                         "ERRO: o fix-names nunca pode ser executado como root. Use uma conta comum.",
                         "ERROR: fix-names must never run as root. Use a regular account."));
        return context.result;
    }
#endif

    std::string validation_error;
    if (!validate_options(options, validation_error, language)) {
        context.result.stats.errors = 1;
        emit(context, tr(language, "ERRO: ", "ERROR: ") + validation_error);
        return context.result;
    }

    std::error_code error;
    const fs::path target = absolute_lexical(options.target, error);
    if (error) {
        context.result.stats.errors = 1;
        emit(context, tr(language, "ERRO: caminho inválido: ",
                         "ERROR: invalid path: ") + error.message());
        return context.result;
    }
    if (target == target.root_path()) {
        context.result.stats.errors = 1;
        emit(context, tr(language,
                         "ERRO: o diretório raiz '/' é bloqueado por segurança.",
                         "ERROR: the root directory '/' is blocked for safety."));
        return context.result;
    }

    const fs::file_status target_status = fs::symlink_status(target, error);
    if (error || target_status.type() == fs::file_type::not_found) {
        context.result.stats.errors = 1;
        emit(context, tr(language, "ERRO: o caminho não existe: ",
                         "ERROR: path does not exist: ") + display_path(target));
        return context.result;
    }

    const fs::path exclusion_base = fs::is_directory(target_status) &&
                                    !fs::is_symlink(target_status)
                                        ? target : target.parent_path();
    for (const fs::path &item : options.exclusions) {
        const fs::path candidate = item.is_absolute() ? item : exclusion_base / item;
        std::error_code exclusion_error;
        fs::path normalized = absolute_lexical(candidate, exclusion_error);
        if (!exclusion_error)
            context.normalized_exclusions.push_back(std::move(normalized));
    }
    context.executable_path = read_executable_path();

    /* O total é conhecido antes da primeira alteração. Isso faz a barra
     * representar a requisição inteira, e não apenas uma estimativa baseada
     * na quantidade já descoberta. */
    if (fs::is_directory(target_status) && !fs::is_symlink(target_status))
        context.progress_total = count_directory_entries(target, context);
    else
        context.progress_total = 1;
    report_progress(context);

    if (fs::is_directory(target_status) && !fs::is_symlink(target_status)) {
        process_directory(target, context);
    } else {
        fs::directory_entry entry(target, error);
        if (error) {
            context.result.stats.errors = 1;
            emit(context, tr(language, "ERRO: não foi possível examinar: ",
                             "ERROR: could not inspect: ") + error.message());
            return context.result;
        }
        std::vector<EntryPlan> entries;
        entries.push_back(build_entry(entry, context));
        execute_group(target.parent_path(), entries, context);
    }

    std::ostringstream summary;
    summary << tr(language, "Resumo: planejados=", "Summary: planned=")
            << context.result.stats.planned
            << tr(language, ", renomeados=", ", renamed=")
            << context.result.stats.renamed
            << tr(language, ", inalterados=", ", unchanged=")
            << context.result.stats.unchanged
            << tr(language, ", ignorados=", ", excluded=")
            << context.result.stats.excluded
            << tr(language, ", conflitos=", ", conflicts=")
            << context.result.stats.conflicts
            << tr(language, ", erros=", ", errors=")
            << context.result.stats.errors;
    emit(context, summary.str());

    /* Uma mudança concorrente no diretório pode tornar a contagem inicial
     * diferente da travessia real. A conclusão sempre fecha a barra em 100%,
     * sem alterar as estatísticas da operação. */
    context.progress_current = context.progress_total;
    report_progress(context);

    context.result.success = context.result.stats.errors == 0 &&
                             context.result.stats.conflicts == 0;
    return context.result;
}

std::string display_path(const fs::path &path)
{
    const std::string raw = path.string();
    std::ostringstream output;
    for (unsigned char byte : raw) {
        switch (byte) {
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (byte < 0x20 || byte == 0x7F) {
                output << "\\x" << std::uppercase << std::hex
                       << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(byte)
                       << std::nouppercase << std::dec;
            } else {
                output << static_cast<char>(byte);
            }
        }
    }
    return output.str();
}

} // namespace fixnames
