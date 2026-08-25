#pragma once

#include "core.hpp"

namespace fixnames {

/* Abre a interface textual. A configuração inicial pode conter um caminho e
 * opções recebidas pela CLI por meio de --interactive. */
int run_ncurses_interface(RenamerOptions initial, Language language);

} // namespace fixnames
