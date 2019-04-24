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

void initialize_cmdline(int argc, char **argv)
{
    sh_stat.alias_table["ll"] = "ls -l";
    sh_stat.argc = argc;
    sh_stat.argv = argv;
}

int main(int argc, char *argv[])
{
    initialize_cmdline(argc, argv);

    char *line;
    while (true) {
        line = readline(prompt);

        if (line && *line) {
            add_history(line);
            Cmdline cmdline(line);
            sh_stat.exit_val = cmdline.exec();
            free(line);
        }
    }
}