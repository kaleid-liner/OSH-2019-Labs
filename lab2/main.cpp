#include <readline/readline.h>
#include <readline/history.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include "cmdline.h"
#include "shstat.h"

using namespace std;
const char *prompt = "> ";
extern ShStat sh_stat;

void initialize_cmdline()
{
    sh_stat.alias_table["ll"] = "ls -l";
}

int main()
{
    initialize_cmdline();

    char *line;
    while (true) {
        line = readline(prompt);

        if (line && *line) {
            add_history(line);
            Cmdline cmdline(line);
            cmdline.exec();
            free(line);
        }
    }
}