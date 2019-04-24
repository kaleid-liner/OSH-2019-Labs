#include <string>
#include <vector>
#include <unistd.h>

class Cmd {
public:
    Cmd(const std::string &cmd);
    int exec(int infd, int outfd) const;

    // if can redirect, return true, else return false
    bool redirect(const std::string &s);
    
    static const char *builtins[];

private:
    // argv
    std::vector<std::string> _argv;
    std::vector<std::pair<int, int>> _rd_fds;
    bool _is_builtin;
};

class Cmdline {
public:
    Cmdline(const std::string &cmdline);
    int exec() const;

private:
    std::vector<Cmd> _cmds;
};

// cpp version of some c func
std::string ccgetcwd();
std::string ccgethome();