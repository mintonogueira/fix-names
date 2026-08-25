/*
 * Testes do núcleo compartilhado.
 *
 * A primeira seção testa transformações puras. A segunda cria uma árvore
 * temporária real para validar recursão opcional, exclusões, links, extensões,
 * colisões e aplicação efetiva. O diretório temporário nunca é um caminho
 * amplo e é removido somente depois de seu prefixo ser validado.
 */

#include "core.hpp"

#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace fixnames;

namespace {

void check(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void touch(const fs::path &path)
{
    std::ofstream output(path);
    check(static_cast<bool>(output), "não foi possível criar " + path.string());
    output << "test\n";
}

fs::path make_test_directory()
{
    std::string pattern = "/tmp/fix-names-tests-XXXXXX";
    std::vector<char> writable(pattern.begin(), pattern.end());
    writable.push_back('\0');
    char *created = ::mkdtemp(writable.data());
    check(created != nullptr, "mkdtemp falhou");
    return fs::path(created);
}

void pure_transform_tests()
{
    RenamerOptions options;
    options.case_mode = CaseMode::Uppercase;
    options.remove_accents = true;
    options.replace_spaces = true;
    options.space_replacement = "-";
    check(transform_name("ação final.TAR.GZ", false, options) ==
              "ACAO-FINAL.TAR.GZ",
          "maiúsculas/acentos/espaços/extensão composta");

    options.replace_underscores = true;
    options.underscore_replacement = "-";
    check(transform_name("ação final_teste.TAR.GZ", false, options) ==
              "ACAO-FINAL-TESTE.TAR.GZ",
          "sublinhados opcionais");

    options = {};
    options.case_mode = CaseMode::Lowercase;
    check(transform_name("FOTO DE VIAGEM.JpG", false, options) ==
              "foto de viagem.JpG",
          "minúsculas com extensão preservada");

    options = {};
    options.case_mode = CaseMode::CapitalizeWords;
    check(transform_name("mARIA da_sILVA.txt", false, options) ==
              "Maria Da_Silva.txt",
          "inicial de cada palavra");

    options = {};
    options.find_replace_enabled = true;
    options.find_text = "antigo";
    options.replacement_text = "novo";
    check(transform_name("antigo-antigo.conf", false, options) ==
              "novo-novo.conf",
          "localizar e substituir todas as ocorrências");

    options = {};
    options.insert_mode = InsertMode::Insert;
    options.insert_text = "X";
    options.insert_position = 1;
    check(transform_name("ação.txt", false, options) == "aXção.txt",
          "inserção por posição Unicode");

    options.insert_mode = InsertMode::Overwrite;
    options.insert_text = "ZZ";
    check(transform_name("ação.txt", false, options) == "aZZo.txt",
          "sobrescrita por posição Unicode");

    options = {};
    options.case_mode = CaseMode::Uppercase;
    options.include_extension = true;
    check(transform_name("foto.JpG", false, options) == "FOTO.JPG",
          "alteração explícita da extensão");

    options = {};
    options.replace_spaces = true;
    options.space_replacement = "_";
    std::string error;
    check(validate_options(options, error, Language::PortugueseBrazil) == false,
          "target vazio deve ser rejeitado");
    options.target = ".";
    check(validate_options(options, error, Language::PortugueseBrazil),
          "um caractere UTF-8 válido deve ser aceito");
    options.space_replacement = "--";
    check(!validate_options(options, error, Language::PortugueseBrazil),
          "mais de um caractere deve ser rejeitado");
}

void filesystem_tests()
{
    const fs::path root = make_test_directory();
    try {
        touch(root / "AÇÃO FINAL.TXT");
        touch(root / "MANTER ESTE.TXT");
        touch(root / ".CONFIG.JpG");
        fs::create_directory(root / "SUB PASTA");
        touch(root / "SUB PASTA" / "ARQUIVO INTERNO.TXT");
        fs::create_symlink("destino-inexistente", root / "ATALHO.LnK");

        RenamerOptions options;
        options.target = root;
        options.case_mode = CaseMode::Lowercase;
        options.remove_accents = true;
        options.replace_spaces = true;
        options.space_replacement = "-";
        options.exclusions.push_back("MANTER ESTE.TXT");

        RunResult first = run_renamer(options, Language::PortugueseBrazil);
        check(first.success, "execução não recursiva deveria terminar sem falhas");
        check(fs::exists(root / "acao-final.TXT"), "arquivo imediato não renomeado");
        check(fs::exists(root / "MANTER ESTE.TXT"), "exclusão não preservada");
        check(fs::exists(root / ".config.JpG"), "arquivo oculto/extensão não preservado");
        check(fs::is_symlink(fs::symlink_status(root / "atalho.LnK")),
              "link simbólico não foi apenas renomeado");
        check(fs::exists(root / "sub-pasta" / "ARQUIVO INTERNO.TXT"),
              "conteúdo de subpasta foi alterado sem --recursive");

        options.recursive = true;
        RunResult second = run_renamer(options, Language::PortugueseBrazil);
        check(second.success, "execução recursiva deveria terminar sem falhas");
        check(fs::exists(root / "sub-pasta" / "arquivo-interno.TXT"),
              "conteúdo recursivo não foi renomeado");

        fs::create_directory(root / "conflitos");
        touch(root / "conflitos" / "ação.txt");
        touch(root / "conflitos" / "ACAO.txt");
        RenamerOptions conflict;
        conflict.target = root / "conflitos";
        conflict.case_mode = CaseMode::Lowercase;
        conflict.remove_accents = true;
        RunResult collision = run_renamer(conflict, Language::PortugueseBrazil);
        check(!collision.success && collision.stats.conflicts == 2,
              "colisão dupla deveria preservar os dois arquivos");
        check(fs::exists(root / "conflitos" / "ação.txt") &&
                  fs::exists(root / "conflitos" / "ACAO.txt"),
              "arquivos em conflito foram alterados");

        RenamerOptions preview;
        preview.target = root;
        preview.case_mode = CaseMode::Uppercase;
        preview.dry_run = true;
        RunResult simulated = run_renamer(preview, Language::PortugueseBrazil);
        check(simulated.stats.renamed == 0, "simulação não pode renomear");
        check(fs::exists(root / "acao-final.TXT"), "simulação alterou arquivo");
    } catch (...) {
        if (root.string().rfind("/tmp/fix-names-tests-", 0) == 0)
            fs::remove_all(root);
        throw;
    }
    if (root.string().rfind("/tmp/fix-names-tests-", 0) == 0)
        fs::remove_all(root);
}

} // namespace

int main()
{
    std::setlocale(LC_ALL, "C.UTF-8");
    try {
        pure_transform_tests();
#ifdef FIX_NAMES_TEST_ALLOW_ROOT
        filesystem_tests();
        std::cout << "Todos os testes do núcleo: OK (modo isolado de teste)\n";
#else
        if (running_as_root()) {
            RenamerOptions options;
            options.target = ".";
            options.case_mode = CaseMode::Lowercase;
            const RunResult refused = run_renamer(options, Language::English);
            check(!refused.success && refused.stats.errors == 1,
                  "núcleo não recusou EUID 0");
            std::cout << "Testes puros e bloqueio de root: OK\n";
            std::cout << "Testes de arquivos: ignorados sob root por segurança\n";
        } else {
            filesystem_tests();
            std::cout << "Todos os testes do núcleo: OK\n";
        }
#endif
    } catch (const std::exception &error) {
        std::cerr << "FALHA: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
