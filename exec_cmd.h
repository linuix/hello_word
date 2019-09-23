#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <fstream>
#include <stdio.h>


bool readCommand(const std::string command);