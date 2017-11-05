#pragma once

#define _GNU_SOURCE

#define DEFAULT_SERVER_CONTROL_PORT 21
#define DEFAULT_SERVER_DATA_PORT    15000
#define DEFAULT_ROOT_PATH           "/tmp"
#define MAX_CONNECTIONS             16
#define MAX_EVENTS                  64
#define BUFFER_SIZE	                8192
#define PATH_SIZE                   256
#define IP_SIZE                     15
#define VERB_SIZE                   5
#define PARAM_SIZE                  512
#define MAX_PARAMS                  1
#define UNLOGIN_MODE                -1
#define IDLE_MODE                   0
#define PASV_MODE                   1
#define PORT_MODE                   2
#define SIX_MONTH_SECONDS           15552000

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    int connect_fd;
    int data_fd;
    int binary_flag;
    char name_prefix[PATH_SIZE];
    char ip[IP_SIZE];
    int data_port;
    int mode;
}client_info;

char root[PATH_SIZE];
extern client_info client_array[MAX_CONNECTIONS];
extern int current_connections;

// Create socket
// Return the socket FD
// _port is the port
int CreateSocket(int _port);

// Read request from socket
// Returns the length of the request body, and 0 is fail.
// _fd is the socket FD,
// _buffer contains the request body read from _connfd, within BUFFER_SIZE.
ssize_t ReadData(int _fd, char *_buffer, size_t _buf_size);

// Write response to socket
// Returns the length of the response body.
// _fd is the socket FD,
// _buffer contains the response body.
ssize_t WriteData(int _fd, char *_buffer);

// Let socket be unblocked
// Return code, which 0 is success, and -1 is fail
// _fd is the socket FD
int UnblockSocket(int _fd);

// Handle the FTP client command
// Return code, which 0 is success.
// _fd is the socket FD
// _cmd is the command string
int HandleCommand(int _fd, char *_cmd);

int ConcatPath(char *_dest, char *_src);

// Encode pathname
// Return code, which 0 is success.
// _target is the target string, which should be initially blank
// _fd is the socket FD
// _path is the last part of real path
int EncodePathname(char *_target, int _fd, char *_path);

// Encode Nameprefix
// Return code, which 0 is success.
// _target is the target string, which should be initially blank
// _name_prefix is the name prefix
int EncodeNameprefix(char *_target, char *_name_prefix);

int FindClientByFD(int _fd);

int BuildConnectionToClient(int _fd);
int CloseDataConnection(int _fd, int connect_fd);

int GeneratePermissionString(char *_target, int _permission);
int GenerateLsFormatString(char *_target, struct stat *_st, struct dirent *_entry);

// commands handler
int CmdUser(int _fd, int client_no, char* param);
int CmdPass(int _fd, int client_no, char* param);
int CmdSyst(int _fd, int client_no);
int CmdType(int _fd, int client_no, char* param);
int CmdCwd(int _fd, int client_no, char* param);
int CmdPwd(int _fd, int client_no);
int CmdPasv(int _fd, int client_no);
int CmdPort(int _fd, int client_no, char* param);
int CmdList(int _fd, int client_no, char* param);
int CmdRetr(int _fd, int client_no, char* param);
int CmdStor(int _fd, int client_no, char* param);
int CmdMkd(int _fd, int client_no, char* param);
int CmdRmd(int _fd, int client_no, char* param);
int CmdQuit(int _fd, int client_no);