#include "shstat.h"
#include "cmdline.h"

ShStat sh_stat;

ShStat::ShStat(): last_dir(ccgetcwd()), alias_table(), exit_val(0) {};