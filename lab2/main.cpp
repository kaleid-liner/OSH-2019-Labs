#include <readline/readline.h>
#include <readline/history.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include "cmdline.h"

using namespace std;
const char *prompt = "> ";

int main()
{
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