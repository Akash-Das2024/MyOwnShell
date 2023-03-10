#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

void child_process()
{
    int c;
    cin >> c;
    if (c == -1)
        return;
}

int main()
{
    pid_t pid;
    // sleep(120);
    cout << "pid of test process: " << getpid() << endl;
    while (1)
    {
        for (int i = 0; i < 5; i++)
        {
            pid = fork();
            if (pid == 0)
            {

                for (int j = 0; j < 10; j++)
                {
                    pid = fork();
                    if (pid == 0)
                    {
                    }
                }
                exit(0);
            }
        }
        sleep(120); // sleep for 2 minutes
    }
    child_process();
    return 0;
}