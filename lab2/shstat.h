#include <unordered_map>
#include <string>
#include <unistd.h>

struct ShStat { // default public
    std::string last_dir;
    std::unordered_map<std::string, std::string> alias_table;
    int argc;
    char **argv;
    int exit_val;

    ShStat();
};

extern ShStat sh_stat;