/**
 * @file all.h
 * @brief En-tête principal regroupant tous les includes du serveur.
 */

#ifndef ALL_H
#define ALL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <io.h>
#else
#include <getopt.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <signal.h>
#endif

#ifdef _WIN32
#define CLOSESOCKET(s) closesocket((SOCKET)(s))
#define MKDIR_DATA(path) _mkdir(path)
#else
#define CLOSESOCKET(s) close(s)
#define MKDIR_DATA(path) mkdir((path), 0755)
#endif

#include "../lib/list.h"
#include "../lib/tcp.h"
#include "../lib/lobby.h"
#include "../lib/chat.h"
#include "../lib/message.h"
#include "../lib/user.h"
#include "../lib/game.h"
#include "../lib/utils.h"
#include "../lib/codenames.h"
#include "../lib/uuid.h"
#include "../lib/version.h"

#endif // ALL_H