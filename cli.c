#include <stdio.h>
#include <string.h>

int main(int argc, char const* argv[])
{
    char cmd[1024] = { 0 };
    char* row;
    printf("cmd > ");
    fflush(stdout);
    fgets(cmd, 1024, stdin);

    if (strncmp("select", cmd, 6) == 0) {
        printf("select\n");
    } else if (strncmp("insert", cmd, 6) == 0) {
        row = cmd + 6;
        while (row[0] == ' ') {
            row++;
        }
        printf("insert row: %s\n", row);
    }

    return 0;
}
