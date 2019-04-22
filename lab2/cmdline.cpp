#include "cmdline.h"
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <regex>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>

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
        static const string pattern = "\'[^\']*\'|\"[^\"]*\"|[^\\s]+";
        static const regex re(pattern);
        
        for (sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
            tokens.push_back(it->str());
        }

        return tokens;
    }

    vector<string> split_quote_piped(const string &s) 
    {
        static const string pattern = "(\'[^\']*\')|(\"[^\"]*\")|([^|]+)";
        static const regex re(pattern);
        bool last_is_pipe = true;
        vector<string> tokens;

        for (sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
            if (last_is_pipe) {
                tokens.push_back(it->str());
            } else {
                tokens.back().append(it->str());
            }
            if ((*it)[3].matched) {
                last_is_pipe = true;
            } else {
                last_is_pipe = false;
            }
        }
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
                fd = open(filename.c_str(), O_CREAT | O_WRONLY);
            } else {
                fd = open(filename.c_str(), O_APPEND | O_WRONLY);
            }
        }

        return fd;
    }

    string mask_quoted_redirect(const string &s)
    {
        static const regex dre("(\'[^\']*)(<|<<|>|>>)([^\']*\')");
        static const regex sre("(\"[^\"]*)(<|<<|>|>>)([^\"]*\")");

        string ret = regex_replace(s, dre, "$1$3");
        string ret = regex_replace(ret, sre, "$1$3");
        return ret;
    }
}

extern char **environ;

Cmd::Cmd(const string &cmd)
{
    bool neglect = false;
    vector<string> tokens = split_quote(cmd);
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if ((*it)[0] == '<' || (*it)[0] == '>') {
            if (*it == "<" || *it == ">" || *it == "<<" || *it == ">>") {
                ++it;
            } else { // format like <filename
                continue;
            }
        } else {
            _argv.push_back(*it);
        }
    }
}

int Cmd::exec() const 
{
    if (_argv[0] == "exit") {
        exit(0);
    } 
    if (_argv[0] == "pwd") {
        char *dirname = get_current_dir_name();
        cout << dirname << endl;
        return 0;
    }
    if (_argv[0] == "cd") {
        return chdir(_argv[1].c_str());
    }
    if (_argv[0] == "env") {
        for (char **p = environ; p; p++) {
            cout << *p << endl;
        }
        return 0;
    }
    if (_argv[0] == "export") {
        string kv = _argv[1];
        size_t pos = _argv[1].find_first_of(kv);
        string k = _argv[1].substr(0, pos);
        string v = _argv[1].substr(pos + 1);
        return setenv(k.c_str(), v.c_str(), 1);
    }
    if (_argv[0] == "unset") {
        return unsetenv(_argv[1].c_str());
    }

    vector<char *> argv;
    for (const auto &s: _argv) {
        argv.push_back(const_cast<char *>(s.c_str()));
    }
    argv.push_back(nullptr);
    if (execvp(_argv[0].c_str(), argv.data()) < 0) {
        // a rather crude hanler
        // will i use libexplain?
        cerr << "No such command \"" << _argv[0] << "\" or command runtime error." << endl;
    }
}

Cmdline::Cmdline(const string &cmdline): _executed(false)
{
    for (const auto &s: split_quote_piped(cmdline)) {
        _cmds.push_back(Cmd(trim(s)));
    }
    string tmp = mask_quoted_redirect(cmdline);
    
    _infd = redirect_in(tmp);
    _outfd = redirect_out(tmp);
}

int Cmdline::exec() const
{
    size_t n = _cmds.size();
    int fd[2];
    int infd = _infd;
    int outfd;

    for (int i = 0; i < n - 1; ++i) {
        if (i != n - 1) {
            pipe(fd);
            outfd = fd[1];
        } else {
            outfd = _outfd;
        }
        if (fork() == 0) {
            if (infd != 0) {
                dup2(infd, 0);
                close(infd);
            }
            if (outfd != 1) {
                dup2(outfd, 1);
                close(outfd);
            }
            return _cmds[i].exec();
        }
        close(fd[1]);
        close(infd);
        infd = fd[0];
    }

    _executed = true;

    wait(NULL);
    
    return 0;
}
