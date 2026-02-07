#include <iostream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <ostream>
#include <fcntl.h>
#include <list>
#include <iterator>
#include <boost/algorithm/string.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <sstream>

using namespace std;

string input;
vector<string> command;
list<string> history{};
string user;
const char *homedir = getpwuid(getuid())->pw_dir;
string commandPrompt;
string paths[10] = {"", "/usr/local/sbin/", "/usr/local/bin/", "/usr/sbin/", "/usr/bin/", "/sbin/", "/bin/", "/usr/games/", "/usr/local/games/", "/snap/bin/"};

bool isNumber(const string &id)
{
    return !id.empty() && find_if(id.begin(), id.end(), [](unsigned char c)
                                  { return !isdigit(c); }) == id.end();
    // https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
}

int runCommand(vector<string> c)
{
    int status;
    pid_t c_pid = fork();
    if (c_pid == -1)
    {
        cout << "shits fucked" << endl;
        return -1;
    }
    if (c_pid == 0)
    {
        for (int i = 0; i < sizeof(paths); i++)
        {
            string command = paths[i] + c[0];
            if (access(command.c_str(), X_OK) == 0) // Exists and is executable
                switch (c.size())
                { // I fully understand this is not even close to the best way to do this, but I care more about the user than the code right now
                case 1:
                    execl(command.c_str(), c[0].c_str(), (char *)0);
                    break;
                case 2:
                    execl(command.c_str(), c[0].c_str(), c[1].c_str(), (char *)0);
                    break;
                case 3:
                    execl(command.c_str(), c[0].c_str(), c[1].c_str(), c[2].c_str(), (char *)0);
                    break;
                case 4:
                    execl(command.c_str(), c[0].c_str(), c[1].c_str(), c[2].c_str(), c[3].c_str(), (char *)0);
                    break;
                case 5:
                    execl(command.c_str(), c[0].c_str(), c[1].c_str(), c[2].c_str(), c[3].c_str(), c[4].c_str(), (char *)0);
                    break;
                default:
                    execl(command.c_str(), c[0].c_str(), c[1].c_str(), c[2].c_str(), c[3].c_str(), c[4].c_str(), c[5].c_str(), (char *)0);
                    break;
                }
        }
        return 0;
    }

    waitpid(-1, &status, WUNTRACED);
    return status;
}

vector<string> expandCommand(string i)
{
    vector<string> c;
    boost::split(c, i, boost::is_any_of(" ")); // https://www.geeksforgeeks.org/split-string-by-space-into-vector-in-cpp-stl/
    return c;
}

void runPrevious(string id)
{
    if (isNumber(id))
    {
        int idn = stoi(id.c_str());
        auto history_prev = history.begin();
        advance(history_prev, idn);
        runCommand(expandCommand(*history_prev));
    }
    else
    {
        for (auto ent = history.begin(); ent != history.end(); ++ent)
        {
            string entry = *ent;
            if (entry.substr(0, id.length()) == id)
            {
                runCommand(expandCommand(entry));
                return;
            }
        }
        cout << "Previous command not found." << endl;
    }
}

bool getCommand(string com)
{
    if (com == "")
    {
        return true;
    }
    else
    {
        if (com[0] == 0x0C)
        {
            cout << "\033[2J\033[1;1H"; // Magic line from https://stackoverflow.com/questions/17335816/clear-screen-using-c
            return true;
        }
        com.erase(com.begin(), find_if(com.begin(), com.end(), bind1st(not_equal_to<char>(), ' ')));        // Removes uneeded whitespace
        com.erase(find_if(com.rbegin(), com.rend(), bind1st(not_equal_to<char>(), ' ')).base(), com.end()); // https://stackoverflow.com/questions/1798112/removing-leading-and-trailing-spaces-from-a-string
        int semi;
        for (int c = 0; c < com.length(); c++)
        { // Que command check
            if (com[c] == '~')
            {
                boost::replace_all(com, "~", homedir);
            }
            if (com[c] == ';')
            {
                semi = c;
                string com1 = com.substr(0, c);
                string com2 = com.substr(c + 1);
                getCommand(com1);
                getCommand(com2);
                return true;
            }
        }
        int concat;
        for (int c = 0; c < com.length() - 1; c++)
        { // Concat command check
            if (com[c] == '&' && com[c + 1] == '&')
            {
                concat = c;
                string com1 = com.substr(0, c);
                string com2 = com.substr(c + 2);
                if (getCommand(com1))
                {
                    getCommand(com2);
                }
                else
                {
                    return false;
                }
                return true;
            }
        }
        history.push_front(com);
        command = expandCommand(com);
        if (com.find("=") != string::npos)
        {
            size_t q1 = com.find("\"");
            size_t q2 = com.find("\"", q1 + 1);
            commandPrompt = (com.substr(q1 + 1, q2 - q1 - 1)) + "$";
            return true;
        }
        if (command[0][0] == '!')
        {
            runPrevious(command[0].substr(1, command[0].length() - 1));
            return true;
        }
        if (command[0] == "cd")
        {
            if ((int)command.size() == 1 || command[1] == homedir)
            {
                chdir(homedir);
                return true;
            }
            chdir(command[1].c_str());
            return true;
        }
        if (com.find('<') != string::npos)
        {
            ifstream inFile;
            try
            {
                inFile.open(command[2], ios::in);
            }
            catch (const ifstream::failure &e)
            {
                cout << command[2] << " could not be opened." << endl;
                return false;
            }
            stringstream strStream;
            strStream << inFile.rdbuf();
            string str = strStream.str();
            inFile.close();
            getCommand(command[0] + ' ' + str);
            return true;
        }
        if (com.find('>') != string::npos)
        {
            int outFile;
            if (com.find(">>") != string::npos)
            {
                cout << "append" << endl;
                outFile = open(command[2].c_str(), O_WRONLY | O_CREAT | O_APPEND);
            }
            else
            {
                outFile = open(command[2].c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            }
            if (outFile == -1)
            {
                cout << command[2] << " could not be opened." << endl;
                return false;
            }
            int out = dup(1);
            dup2(outFile, 1);
            getCommand(com.substr(0, com.find('>') - 1));
            dup2(out, 1);
            close(outFile);
            close(out);
            return true;
        }
        if (runCommand(command) != -1)
        {
            return true;
        }
        else
        {
            cout << "Error" << endl;
            return false;
        }
    }
}

int main(int argc, char *argv[])
{
    cout << "Username: ";
    getline(cin, user);
    commandPrompt = (user + "$ ");
    while (true)
    {
        cout << commandPrompt;
        getline(cin, input);
        if (input == "exit")
            exit;
        else
            getCommand(input);
    }
}