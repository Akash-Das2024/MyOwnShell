#include <bits/stdc++.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <glob.h>
#include <termios.h>

using namespace std;
using ll = long long;
#define MAX_HISTORY_SIZE 1000
deque<string> history;
bool interrupt = false;
sigjmp_buf env;
vector<pair<int, pair<int, double>>> v;

void restore_history(const char *);
void save_history(const char *);
void shell();
string getcommand();
void clearscr(string);
void signal_handler(int);
vector<string> split_pipes(string &);
vector<string> split_line(string &);
int other_cmds(vector<string> &, int, int);
int execute(vector<string> &);
vector<string> glob(const string &);
void print_history();
void delep(string file_name);
bool comp(pair<int, double>, pair<int, double>);
int count_child(int);
pair<int, double> calculate_cpu_time(int);
map<string, string> getProcessDetails(int);
void traverseProcessTree(int, int, bool);

int main()
{
    // ignore ctrl-\ and ctrl-z
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    char *current_dir = new char[1024];
    getcwd(current_dir, 1024);
    strcat(current_dir, "/__s_h_e_l_l__h_i_s_t_o_r_y__.txt");
    restore_history(current_dir);
    shell();
    save_history(current_dir);
    return 0;
}

void restore_history(const char *file_name)
{
    ifstream ifs(file_name);
    if (ifs.is_open())
    {
        string str;
        while (getline(ifs, str))
        {
            history.push_back(str);
        }
    }
    ifs.close();
}

void save_history(const char *file_name)
{
    ofstream ofs(file_name, ios::trunc);
    for (const auto &str : history)
    {
        ofs << str << endl;
    }
    ofs.close();
}

void shell()
{
    string line;
    vector<string> args;
    vector<string> pipes;
    bool state = true;
    char *buf = new char[1024];
    while (state)
    {
        getcwd(buf, 1024);
        cout << "\033[0;32m";
        cout << buf;
        cout << "\033[0m";
        cout << "$ ";

        signal(SIGINT, signal_handler);
        line = getcommand();
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        if (interrupt)
        {
            interrupt = false;
            continue;
        }

        if (strcmp(line.c_str(), "history") == 0)
        {
            print_history();
            continue;
        }
        pipes = split_pipes(line);
        if (pipes.size() == 1)
        {
            args = split_line(line);
            state = execute(args);
        }
        else if (pipes.size() > 1)
        {
            int in = 0, pipe_des[2];
            for (int i = 0; i < pipes.size() - 1; i++)
            {
                pipe(pipe_des);
                args = split_line(pipes[i]);
                state = other_cmds(args, in, pipe_des[1]);
                close(pipe_des[1]);
                in = pipe_des[0];
            }
            args = split_line(pipes.back());
            state = other_cmds(args, in, STDOUT_FILENO);
            dup(STDIN_FILENO);
            dup(STDOUT_FILENO);
        }
        fflush(stdin);
        fflush(stdout);
        line.clear();
        args.clear();
    }
}

void print_history()
{
    int number = 1;
    for (auto it = history.rbegin(); it != history.rend(); it++)
    {
        cout << number++ << "  " << *it << endl;
    }
}

string getcommand()
{
    termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~ICANON;
    new_tio.c_lflag &= ~ECHO;
    new_tio.c_cc[VMIN] = 1;
    new_tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio); // change terminal flags to new_tio

    string current_cmd = "";
    string input_cmd = "";
    int i = -1;
    bool newline_flag = false;
    int cursor_pos = 0;

    while (1)
    {
        if (sigsetjmp(env, 1) == 0)
        {
            char ch = getchar();

            switch (ch)
            {
            case 1:
                while (cursor_pos > 0)
                {
                    cout << "\033[D";
                    cursor_pos--;
                }
                break;
            case 5:
                while (cursor_pos < current_cmd.size())
                {
                    cout << "\033[C";
                    cursor_pos++;
                }
                break;
            case '\033': // up-key down-key left-key right-key
            {
                char esc = getchar();
                if (esc == '[')
                {
                    char dir = getchar();
                    switch (dir)
                    {
                    case 'A': // up-key
                        if (i == -1)
                        {
                            input_cmd = current_cmd;
                        }
                        if (i < (int)history.size() - 1)
                        {
                            i++;
                            string temp = current_cmd;
                            if (i > 0)
                                history[i - 1] = temp;
                        }
                        if (i >= 0)
                        {
                            while (cursor_pos < current_cmd.size())
                            {
                                cout << "\033[C";
                                cursor_pos++;
                            }
                            clearscr(current_cmd);
                            current_cmd = history[i];
                            cursor_pos = current_cmd.size();
                            cout << current_cmd;
                        }
                        break;
                    case 'B': // down -key
                        if (i >= 0)
                        {
                            i--;
                            string tmp = current_cmd;
                            if (i < (int)history.size() - 1)
                                history[i + 1] = tmp;
                        }
                        while (cursor_pos < current_cmd.size())
                        {
                            cout << "\033[C";
                            cursor_pos++;
                        }
                        clearscr(current_cmd);
                        if (i == -1)
                        {
                            current_cmd = input_cmd;
                        }
                        else
                        {
                            current_cmd = history[i];
                        }
                        cursor_pos = current_cmd.size();
                        cout << current_cmd;
                        break;
                    case 'C': // right -key
                        if (cursor_pos < current_cmd.size())
                        {
                            cout << "\033[C";
                            cursor_pos++;
                        }
                        break;
                    case 'D': // left - key
                        if (cursor_pos > 0)
                        {
                            cout << "\033[D";
                            cursor_pos--;
                        }
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    // escape one char just like terminal
                }
                break;
            }
            case '\n':
            {
                newline_flag = true;
                break;
            }
            case 127: // backspace
            {
                if (cursor_pos > 0)
                {
                    current_cmd.erase(cursor_pos - 1, 1);
                    cout << "\b \b";
                    cout << "\033[1P";
                    cursor_pos--;
                }
                break;
            }
            default:
                break;
            }
            if (isprint(ch)) // if printable
            {
                cout << "\033[@";
                cout << (char)ch;
                current_cmd.insert(cursor_pos, 1, (char)ch);
                cursor_pos++;
            }
            if (newline_flag)
            {
                if (current_cmd.empty())
                    break;
                if (history.size() == MAX_HISTORY_SIZE)
                    history.pop_back();
                if (history.empty() || history[0] != current_cmd)
                    history.push_front(current_cmd);
                break;
            }
        }
        else // env jump
        {
            cout << "^C";
            break;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio); // regain old terminal
    cout << "\n";
    return current_cmd;
}

void clearscr(string current_cmd) // clear screen (input buffer) -- triggered when up-key or down-key is pressed
{
    int size = current_cmd.size();
    while (size--)
    {
        cout << "\b \b";
    }
}

void signal_handler(int sig) // SIGINT handler while taking inputs
{
    interrupt = true;
    siglongjmp(env, 1);
}

vector<string> split_pipes(string &line)
{
    int start = 0, end = 0;
    vector<string> cmds;
    string tmp;
    for (int i = 0; i < line.length(); i++)
    {
        if (line[i] != '|')
        {
            tmp += line[i];
        }
        else
        {
            cmds.push_back(tmp);
            tmp = "";
        }
    }
    if (tmp != "")
        cmds.push_back(tmp);
    return cmds;
}

vector<string> split_line(string &line)
{
    vector<string> cmds;
    string tmp;
    line.append(" ");
    for (int i = 0; i < line.length(); i++)
    {
        if (line[i] == '"')
        {
            if (tmp.length() != 0)
            {
                cmds.push_back(tmp);
                tmp = "";
            }
            i++;
            while (line[i] != '"')
            {
                tmp += line[i];
                i++;
            }
            cmds.push_back(tmp);
            tmp = "";
        }
        else if (line[i] == '\'')
        {
            if (tmp.length() != 0)
            {
                cmds.push_back(tmp);
                tmp = "";
            }
            i++;
            while (line[i] != '\'')
            {
                tmp += line[i];
                i++;
            }
            cmds.push_back(tmp);
            tmp = "";
        }
        else if (line[i] != ' ')
        {
            tmp += line[i];
        }
        else
        {
            if (tmp.length() != 0)
            {
                vector<string> A;
                if (tmp.find('*') != string ::npos || tmp.find('?') != string ::npos)
                {
                    A = glob(tmp);
                    for (auto it : A)
                        cmds.push_back(it);
                }
                else
                {
                    cmds.push_back(tmp);
                }
            }
            tmp = "";
        }
    }
    if (tmp != "")
        cmds.push_back(tmp);
    return cmds;
}

int other_cmds(vector<string> &args, int in, int out)
{
    int child_status;
    pid_t pid1 = 0, wpid;

    pid1 = fork();
    if (pid1 == 0)
    {
        // Child Process
        // cout << "a\n"; // SIG_IGN
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        int cnt = 0;
        if (in != 0)
        {
            dup2(in, STDIN_FILENO);
            close(in);
        }
        if (out != 1)
        {
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        for (int i = 0; i < args.size(); i++)
        {
            if (args[i] == "&")
                cnt++;
            else if (args[i] == ">")
            {
                mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                close(STDOUT_FILENO);
                dup(open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode));
                cnt += 2;
            }
            else if (args[i] == "<")
            {
                close(STDIN_FILENO);
                dup(open(args[i + 1].c_str(), O_RDONLY));
                cnt += 2;
            }
        }

        char **arg_s = new char *[args.size() + 1];
        int i = 0;
        for (i = 0; i < args.size() - cnt; i++)
        {
            arg_s[i] = strdup(args[i].c_str());
        }
        arg_s[i] = NULL;
        if (execvp(arg_s[0], arg_s) == -1)
            perror("Invalid Command");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent Process
        // cout << "b\n";
        if (args.back() != "&")
        {
            do
            {
                wpid = waitpid(pid1, &child_status, WUNTRACED);
            } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status) && !WIFSTOPPED(child_status));
            if (WIFSTOPPED(child_status))
            {
                cout << "\n";
                kill(pid1, SIGCONT);
            }
            else if (WIFSIGNALED(child_status))
            {
                cout << "\n";
            }
        }
    }
    return 1;
}

int execute(vector<string> &args)
{
    if (args[0] == "")
        return 1;
    else
    {
        if (args[0] == "cd")
        {
            if (args.size() == 1)
            {
                chdir(getenv("HOME"));
            }
            else if (args.size() > 2)
            {
                cout << "cd: too many arguments\n";
            }
            else
            {
                if (chdir(args[1].c_str()) != 0)
                {
                    cout << "Invalid argument\n";
                }
            }
        }
        else if (args[0] == "pwd" && args.size() == 1)
        {
            char *buf = new char[1024];
            getcwd(buf, 1024);
            cout << buf << endl;
        }
        else if (args[0] == "exit")
        {
            cout << "exit\n";
            return 0;
        }
        else if (args[0] == "delep")
        {
            if (args.size() == 2)
                delep(args[1]);
            else
            {
                cout << "delep: invalid number of arguments\n";
            }
        }
        else if (args[0] == "sb")
        {
            if (args.size() != 2 && args.size() != 3)
            {
                cout << "Usage: sb [PID] [-suggest]" << endl;
            }
            else
            {
                int pid = atoi(args[1].c_str());

                bool suggest = false;
                if (args.size() == 3 && string(args[2]) == "-suggest")
                {
                    suggest = true;
                }
                traverseProcessTree(pid, 0, suggest);
                vector<pair<int, double>> vnew;
                if (suggest)
                {
                    for (long unsigned int k = 0; k < v.size(); k++)
                    {
                        vnew.push_back({v[k].first, v[k].second.first - v[k].second.second});
                    }
                    sort(vnew.begin(), vnew.end(), comp);
                    cout << "\n# The suspected TROJAN process id is: " << vnew[vnew.size() - 1].first << endl;
                }
            }
        }
        else
            return other_cmds(args, STDIN_FILENO, STDOUT_FILENO);
    }
    return 1;
}

vector<string> glob(const string &pattern)
{
    // glob struct
    glob_t glob_op;
    memset(&glob_op, 0, sizeof(glob_op));

    // doing the glob operation
    int ret = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_op);
    if (ret != 0)
    {
        globfree(&glob_op);
        stringstream ss;
        ss << "glob() failed with return value, " << ret << endl;
        throw runtime_error(ss.str());
    }

    // collect all the filenames into a vector<string>
    vector<string> filenames;
    for (size_t i = 0; i < glob_op.gl_pathc; ++i)
    {
        filenames.push_back(string(glob_op.gl_pathv[i]));
    }

    // cleanup
    globfree(&glob_op);

    // done
    return filenames;
}

void delep(string file_name)
{
    vector<int> processes;
    char temp[5];
    char buffer[1024];
    strcpy(buffer, "lsof ");
    strcat(buffer, file_name.c_str());
    FILE *f = popen(buffer, "r");

    if (f == NULL)
        return;
    while (fgets(buffer, 1024, f) != NULL)
    {
        int pid;
        if (sscanf(buffer, "%*s %d", &pid) == 1)
        {
            processes.push_back(pid);
        }
    }
    pclose(f);
    if (processes.size() == 0)
    {
        if (access(file_name.c_str(), F_OK) == 0)
        {
            cout << "The file " << file_name << " is not locked\n";
            cout << "Do you want to delete the file (yes/no)\n";
            cin >> temp;
            if (temp[0] == 'Y' | temp[0] == 'y')
            {
                remove(file_name.c_str());
                cout << "The file " << file_name << " is successfully deleted" << endl;
            }
            getchar();
        }
    }
    else
    {
        cout << "The file " << file_name << " is locked by following processes: \n";
        for (int k = 0; k < processes.size(); k++)
        {
            cout << processes[k] << endl;
        }
        cout << "Do you want to kill all of these processes (yes/no)\n";
        cin >> temp;
        if (temp[0] == 'Y' | temp[0] == 'y')
        {
            for (int k = 0; k < processes.size(); k++)
            {
                kill(processes[k], SIGKILL);
            }
            remove(file_name.c_str());
            cout << "All process are killed and the file " << file_name << " is successfully deleted" << endl;
        }
        getchar();
    }
}

bool comp(pair<int, double> p1, pair<int, double> p2)
{
    return (p1.second < p2.second);
}

int count_child(int pid)
{
    char command[50];
    sprintf(command, "pstree -p %d | wc -l", pid);
    int num_children = 0;

    if (system(command) == 0)
    {
        FILE *fp = popen(command, "r");
        if (fp == NULL)
        {
            perror("popen failed");
            return (-1);
        }
        fscanf(fp, "%d", &num_children);
        pclose(fp);
    }
    return num_children;
    //     printf("The number of children of the current process (including all generations) is: %d\n", num_children);
}

pair<int, double> calculate_cpu_time(int pid)
{
    // Calling proc function to get different times
    char file[100];
    sprintf(file, "/proc/%s/stat", (to_string(pid)).c_str());
    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        return (make_pair(-1, -1));
    }
    char file_1[100];
    sprintf(file_1, "/proc/uptime"); // Storing uptime of the system
    FILE *fp_1 = fopen(file_1, "r");
    if (fp_1 == NULL)
    {
        perror("Error opening file");
        return (make_pair(-1, -1));
    }

    int utime, stime, start_time;
    double uptime;
    fscanf(fp_1, "%lf", &uptime);
    // storing different times from proc call
    fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %d %d %*d %*d %*d %*d %*d %*d %d", &utime, &stime, &start_time);
    int hertz = sysconf(_SC_CLK_TCK);
    // printf("%d, %d, %d, %lf, %d\n", utime, stime, start_time, uptime, hertz);
    double total_time = utime + stime;
    total_time = uptime - (start_time / hertz);
    // printf("total_time %lf\n",total_time);
    fclose(fp);
    fclose(fp_1);

    int cnt_child = count_child(pid);

    v.push_back({pid, {cnt_child, total_time}});
    return {cnt_child, total_time};
}

map<string, string> getProcessDetails(int pid)
{
    map<string, string> details;
    string filename = "/proc/" + to_string(pid) + "/status"; // Opening the status file for the process
    ifstream statusFile(filename.c_str());
    if (!statusFile.is_open())
    {
        cout << "Error opening file: " << filename << endl;
    }
    // Read the details from the status file
    string line;
    while (getline(statusFile, line))
    {
        int pos = line.find(':');
        if (pos != string::npos)
        {
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);
            details[key] = value;
        }
        // printf("......%s\n",(details["cpu_time"]));
    }
    pair<int, double> p;
    p = calculate_cpu_time(pid);
    details["cpu_time"] = to_string(p.second);
    details["num_child"] = to_string(p.first);
    return details;
}

void traverseProcessTree(int pid, int indent, bool suggest)
{
    // Read the details of the process
    map<string, string> details = getProcessDetails(pid);
    // Print the process ID and name
    cout << string(indent, ' ') << "Process ID: " << pid << endl;
    cout << string(indent, ' ') << "Process Name: " << details["Name"] << endl;
    cout << string(indent, ' ') << "Total children(recursively): " << details["num_child"] << endl;
    cout << string(indent, ' ') << "CPU time: " << details["cpu_time"] << endl
         << endl;

    // Recursively traverse the process tree
    int parentPID = atoi(details["PPid"].c_str());
    if (parentPID > 0)
    {
        traverseProcessTree(parentPID, indent + 2, suggest);
    }
}