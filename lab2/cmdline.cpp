#include "cmdline.h"
#include "shstat.h"
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <regex>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <pwd.h>
#include <exception>

using namespace std;

/*
    some helper functions 
*/
namespace {
    /*
        Tokenizing a String but ignoring delimiters within quotes
    */
    vector<string> split_quote(const string &s)
    {
        vector<string> tokens;
        static const string pattern = "\'[^\']*\'|\"[^\"]*\"|[^\'\"\\s]+";
        static const regex re(pattern);
        
        for (sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
            tokens.push_back(it->str());
        }

        return tokens;
    }

    vector<string> split_quote_piped(const string &s) 
    {
        static const string pattern = "(\'[^\']*\')|(\"[^\"]*\")|([^|\'\"]+)(\\|)*";
        static const regex re(pattern);
        bool last_is_pipe = true;
        vector<string> tokens;

        for (sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
            if (last_is_pipe) {
                tokens.push_back(it->str());
            } else {
                tokens.back().append(it->str());
            }
            if ((*it)[4].matched) {
                last_is_pipe = true;
                tokens.back().pop_back();
            } else {
                last_is_pipe = false;
            }
        }

        return tokens;
    }

    void trim(string &s, const string &chars = " ")
    {
        s.erase(0, s.find_first_not_of(chars));
        s.erase(s.find_last_not_of(chars) + 1);
    }

    string trim(const string &s, const string &chars = " ")
    {
        string tmp(s);
        trim(tmp, chars);
        return tmp;
    }

    /*
        only the first redirect will be recognized
        all else will be ignored
        return file descriptor
    */
    int redirect_in(const string &s)
    {
        static const regex re("(?:<|<<)\\s*(\'[^\']*\'|\"[^\"]*\"|[\\w./-]+).*");

        smatch m;
        int fd = 0;
        if (regex_search(s, m, re)) {
            string filename = m[1].str();
            fd = open(filename.c_str(), O_RDONLY);
        }

        return fd;
    }

    int redirect_out(const string &s)
    {
        static const regex re("(?:(>)|(>>))\\s*(\'[^\']*\'|\"[^\"]*\"|[\\w./-]+).*");

        smatch m;
        int fd = 1;
        if (regex_search(s, m, re)) {
            string filename = m[3].str();
            if (m[1].matched) {
                fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666); // will be umasked
            } else {
                fd = open(filename.c_str(), O_APPEND | O_WRONLY, 0666);
            }
        }

        return fd;
    }

    string mask_quoted_redirect(const string &s)
    {
        static const regex dre("(\'[^\']*)(<|<<|>|>>)([^\']*\')");
        static const regex sre("(\"[^\"]*)(<|<<|>|>>)([^\"]*\")");

        string ret = regex_replace(s, dre, "$1$3");
        ret = regex_replace(ret, sre, "$1$3");
        return ret;
    }

    string alias(const string &s) 
    {
        static const regex cmd_re("^[^\\|]+|\\|(\\s*[^\\|]+)");
        string aliasstr(s), tmp(s);

        smatch m;
        string cmd;

        size_t offset = 0;

        while (true) {
            if (regex_search(tmp, m, cmd_re)) {
                if (m[1].matched) {
                    cmd = m[1].str();
                } else {
                    cmd = m[0].str();
                }
                trim(cmd);
                auto map_it = sh_stat.alias_table.find(cmd);
                if (map_it != sh_stat.alias_table.cend()) {
                    int jump_pipe = m[1].matched;
                    aliasstr.replace(m.position(0) + offset + jump_pipe, m.length(0) - jump_pipe, map_it->second);
                    tmp = aliasstr.substr(m.position(0) + offset);
                    offset = m.position(0) + offset;
                } else {
                    tmp = m.suffix();
                    offset += m.position(0) + m.length(0);
                }
            } else {
                break;
            }
        }
        
        return aliasstr;
    }

    pair<string, string> parse_kv(const vector<string> &argv)
    {
        if (argv.size() <= 1) {
            throw logic_error("expect key=value but not found");
        }
        pair<string, string> kv;
        if (argv.size() == 2) {
            size_t pos = argv[1].find_first_of('=');
            kv.first = argv[1].substr(0, pos);
            kv.second = argv[1].substr(pos + 1);
        } else {
            if (argv[1].back() != '=') {
                throw logic_error("expect key=value but not found");
            }
            kv.first = argv[1].substr(0, argv[1].size() - 1);
            kv.second = argv[2];
        }
        return kv;
    }
}

extern char **environ;

const char *Cmd::builtins[] = {
    "exit",
    "pwd",
    "env",
    "export",
    "unset",
    "cd",
    "alias",
    "unalias"
};

Cmd::Cmd(const string &cmd)
{
    static const regex var_re("\\$(\\w+)");
    static const regex user_re("~([\\w-]*)(\\S*)");

    vector<string> tokens = split_quote(cmd);
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if ((*it)[0] == '<' || (*it)[0] == '>') {
            if (*it == "<" || *it == ">" || *it == "<<" || *it == ">>") {
                ++it;
            } else { // format like <filename
                continue;
            }
        } else {
            smatch m;
            // "$X" is "{value of X}" and '$HOME' is '$HOME'
            if ((it->size() > 0) && ((*it)[0] != '\'')
                && (regex_search(*it, m, var_re))) {
                char *value;
                // in fact ok due to c return value lifetime rule
                if (value = getenv(m[1].str().c_str())) {
                    it->replace(m.position(1) - 1, m.length(1) + 1, value);
                } else {
                    it->replace(m.position(1) - 1, m.length(1) + 1, "");
                }
            } else if (regex_match(*it, m, user_re)) {
                string user_dirname;
                if (m.length(1) == 0) {
                    user_dirname = ccgethome();
                } else {
                    auto user_pw = getpwnam(m[1].str().c_str());
                    if (user_pw) {
                        user_dirname = user_pw->pw_dir ? user_pw->pw_dir : "";
                    }
                }
                // don't user regex_replace in case any fmt in string
                it->replace(m.position(0), m.length(1) + 1, user_dirname);
            } 
            if (it->size() > 0 && ((*it)[0] == '\'' || (*it)[0] == '\"')) {
                it->erase(0, 1);
                it->erase(it->size() - 1);
            }
            _argv.push_back(*it);
        }
    }

    string command = _argv[0];
    const char **builtins_end = builtins + sizeof(builtins) / sizeof(char*);
    _is_builtin = find_if(builtins, builtins_end, 
        [&] (auto s) {
            return command == s;
        }) != builtins_end;
}

int Cmd::exec(int infd, int outfd) const 
{
    int saved_stdin = dup(0);
    int saved_stdout = dup(1);
    int ret;

    if (infd != 0) {
        dup2(infd, 0);
        close(infd);
    }
    if (outfd != 1) {
        dup2(outfd, 1);
        close(outfd);
    }

    if (!_is_builtin) {
        if (fork() == 0) {
            vector<char *> argv;
            for (const auto &s: _argv) {
                argv.push_back(const_cast<char *>(s.c_str()));
            }
            argv.push_back(NULL);
            if ((ret = execvp(_argv[0].c_str(), argv.data())) < 0) {
                // a rather crude hanler
                // will i use libexplain?
                cerr << "No such command \"" << _argv[0] << "\" or command runtime error." << endl;
            }
            exit(ret);
        }

        wait(NULL);
        ret = 0;
    } else {
        try {
            if (_argv[0] == "exit") {
                exit(0);
            } else if (_argv[0] == "pwd") {
                string pwd_dirname = ccgetcwd();
                cout << pwd_dirname << endl;
                ret = 0;
            } else if (_argv[0] == "cd") {
                string cd_dirname;
                if (_argv.size() == 1) {
                    cd_dirname = "";
                } else {
                    cd_dirname = trim(_argv.at(1));
                }
                if (cd_dirname == "") { // the "~user" has been parsed
                    cd_dirname = ccgethome();
                } else if (cd_dirname == "-") { 
                    cd_dirname = sh_stat.last_dir;
                    cout << cd_dirname << endl;
                }
                sh_stat.last_dir = ccgetcwd();
                ret = chdir(cd_dirname.c_str());
            } else if (_argv[0] == "env") {
                for (char **p = environ; *p; p++) {
                    cout << *p << endl;
                }
                ret = 0;
            } else if (_argv[0] == "export") {
                auto kv = parse_kv(_argv);
                ret = setenv(kv.first.c_str(), kv.second.c_str(), 1);
            } else if (_argv[0] == "unset") {
                ret = unsetenv(_argv.at(1).c_str());
            } else if (_argv[0] == "alias") {
                if (_argv.size() == 1) {
                    for (const auto &kv: sh_stat.alias_table) {
                        cout << "alias " << kv.first << "='" << kv.second << "'" << endl;
                    }
                    ret = 0;
                } else {
                    auto kv = parse_kv(_argv);
                    sh_stat.alias_table[kv.first] = kv.second;
                    ret = 0;
                }
            } else if (_argv[0] == "unalias") {
                string k = _argv.at(1);
                sh_stat.alias_table.erase(k);
            }
        } catch (const out_of_range &e) {
            cerr << "command arguments error" << endl;
        } catch (const logic_error &e) {
            cerr << e.what() << endl;
        }
    }

    dup2(saved_stdin, 0);
    dup2(saved_stdout, 1);
    close(saved_stdin);
    close(saved_stdout);
    return ret;
}

Cmdline::Cmdline(const string &cmdline): _executed(false)
{
    string tmp = alias(cmdline);

    for (const auto &s: split_quote_piped(tmp)) {
        _cmds.push_back(Cmd(trim(s)));
    }
    tmp = mask_quoted_redirect(tmp);
    
    _infd = redirect_in(tmp);
    _outfd = redirect_out(tmp);
}

int Cmdline::exec() const
{
    size_t n = _cmds.size();
    int fd[2];
    int infd = _infd;
    int outfd;
    int ret;

    for (int i = 0; i < n; ++i) {
        if (i != n - 1) {
            pipe(fd);
            outfd = fd[1];
        } else {
            outfd = _outfd;
        }
        ret = _cmds[i].exec(infd, outfd);
        infd = fd[0];
    }

    _executed = true;

    return ret;
}

string ccgetcwd() 
{
    string dirname;
    char *cur_dir = get_current_dir_name();
    dirname = cur_dir ? cur_dir : "";
    if (cur_dir) free(cur_dir);
    return dirname;
}

string ccgethome()
{
    string homedir;
    auto pwd = getpwuid(getuid());
    char *home ;
    if (home = getenv("HOME")) {
        return home;
    }
    home = pwd ? pwd->pw_dir : nullptr;
    homedir = home ? home : "";
    return homedir;
}