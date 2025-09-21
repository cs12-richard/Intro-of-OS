#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
using namespace std;

vector<string> words;
string word;
int background;

struct in_out {
    string in_r;
    string out_r;
};

in_out check() {
    in_out temp;
    for (int i = 0; i < words.size(); i++) {
        if (words[i] == "<") {
            temp.in_r = words[i + 1];
            words.erase(words.begin() + i);
            words.erase(words.begin() + i);
            i--;
        } else if (words[i] == ">") {
            temp.out_r = words[i + 1];
            words.erase(words.begin() + i);
            words.erase(words.begin() + i);
            i--;
        } else if (words[i] == "&") {
            background = 1;
            words.erase(words.begin() + i);
            i--;
        }
    }
    return temp;
}

int main() {
    signal(SIGCHLD, SIG_IGN);

    while (true) {
        background = 0;
        cout << "> ";
        string line;
        getline(cin, line);
        if (line.empty()){
            continue;
        }

        stringstream ss(line);
        words.clear();
        while (ss >> word) {
            words.push_back(word);
        }

        int pipe_pos = -1;
        for (size_t i = 0; i < words.size(); i++) {
            if (words[i] == "|") {
                pipe_pos = i;
                break;
            }
        }

        in_out temp = check();

        if (pipe_pos != -1) {//pipe
            vector<string> l_cmd(words.begin(), words.begin() + pipe_pos);
            vector<string> r_cmd(words.begin() + pipe_pos + 1, words.end());

            int pipefd[2];
            pipe(pipefd);

            pid_t pid1 = fork();
            if (pid1 < 0) {
                fprintf(stderr, "Fork Failed");
                exit(-1);
            } else if (pid1 == 0) {
                close(pipefd[0]);
                dup2(pipefd[1], 1);
                close(pipefd[1]);

                if (!temp.in_r.empty()) {
                    int fd = open(temp.in_r.c_str(), O_RDONLY);
                    dup2(fd, 0);
                    close(fd);
                }
                if (!temp.out_r.empty()) {
                    int fd = open(temp.out_r.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    dup2(fd, 1);
                    close(fd);
                }

                vector<char*> argv(l_cmd.size() + 1);
                for (size_t i = 0; i < l_cmd.size(); i++) {
                    argv[i] = const_cast<char*>(l_cmd[i].c_str());
                }
                argv.back() = nullptr;

                execvp(argv[0], argv.data());
                exit(1);
            }

            pid_t pid2 = fork();
            if (pid2 < 0) {
                fprintf(stderr, "Fork Failed");
                exit(-1);
            } else if (pid2 == 0) {
                close(pipefd[1]);
                dup2(pipefd[0], 0);
                close(pipefd[0]);

                vector<char*> argv(r_cmd.size() + 1);
                for (size_t i = 0; i < r_cmd.size(); i++) {
                    argv[i] = const_cast<char*>(r_cmd[i].c_str());
                }
                argv.back() = nullptr;

                execvp(argv[0], argv.data());
                exit(1);
            }

            close(pipefd[0]);
            close(pipefd[1]);

            if (!background) {
                waitpid(pid1, nullptr, 0);
                waitpid(pid2, nullptr, 0);
            }
        } else {// no pipe
            pid_t pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Fork Failed");
                exit(-1);
            } else if (pid == 0) {
                if (!temp.in_r.empty()) {
                    int fd = open(temp.in_r.c_str(), O_RDONLY);
                    dup2(fd, 0);
                    close(fd);
                }
                if (!temp.out_r.empty()) {
                    int fd = open(temp.out_r.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    dup2(fd, 1);
                    close(fd);
                }

                vector<char*> argv(words.size() + 1);
                for (size_t i = 0; i < words.size(); i++) {
                    argv[i] = const_cast<char*>(words[i].c_str());
                }
                argv.back() = nullptr;

                execvp(argv[0], argv.data());
                exit(1);
            } else {
                if (!background) {
                    waitpid(pid, nullptr, 0);
                }
            }
        }
    }
}
