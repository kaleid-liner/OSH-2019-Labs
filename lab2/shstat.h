#include <unordered_map>
#include <string>
#include <unistd.h>

struct ShStat { // default public
    std::string last_dir;

    ShStat();
};

extern ShStat sh_stat;