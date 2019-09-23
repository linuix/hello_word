#include"exec_cmd.h"

bool readCommand(const std::string command)
{
    FILE *fp = popen(command.c_str(), "r");
    if (fp == nullptr)
    {
        return false;
    }
    char buffer[512];
    while (1)
    {
        if (feof(fp)) //是否读完
        {
            break;
        }
        int ret = fread(buffer, 1, sizeof(buffer), fp);
        if (ret < 0)
        {
            pclose(fp);
            return false;
        }
        printf("readCommand buffer = %s",buffer);
    }
    pclose(fp);
    return true;
}