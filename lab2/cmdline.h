#include <string>
#include <vector>
#include <unistd.h>
#include <unordered_set>

class Cmd {
public:
    Cmd(const std::string &cmd);
    int exec(int infd, int outfd) const;
    
    static const char *builtins[];

private:
    // argv
    std::vector<std::string> _argv;
    bool _is_builtin;
};

class Cmdline {
public:
    Cmdline(const std::string &cmdline);
    int exec() const;
    ~Cmdline() {
        if (_infd != 0) {
            close(_infd);
        }
        if (_outfd != 1) {
            close(_outfd);
        }
    }

private:
    std::vector<Cmd> _cmds;
    // in file descriptor
    int _infd;
    // out file descriptor
    int _outfd;
    bool mutable _executed;
};