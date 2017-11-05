#include "server.h"

int CmdUser(int _fd, int client_no, char* param) 
{
    if (strcmp(param, "anonymous") == 0) {
        WriteData(_fd, "331 Accept user anonymous. Please input your email as password.\r\n");
    }
    else {
        WriteData(_fd, "530 This user is unacceptable.\r\n");
    }
    return 0;
}

int CmdPass(int _fd, int client_no, char * param)
{
    if (strchr(param, '@') != NULL) {
        client_array[client_no].mode = IDLE_MODE;
        WriteData(_fd,
            "230-\r\n"
            "230-Welcome to FTP server for experiment 1.\r\n"
            "230-Only for test use.\r\n"
            "230 Guest login ok, access restrictions apply.\r\n"
        );
    }
    else {
        WriteData(_fd, "530 You may have input a wrong password.\r\n");
    }
    return 0;
}

int CmdSyst(int _fd, int client_no)
{
    WriteData(_fd, "215 UNIX Type: L8\r\n");
    return 0;
}

int CmdType(int _fd, int client_no, char * param)
{
    if (strcmp(param, "I") == 0) {
        WriteData(_fd, "200 Type set to I.\r\n");
    }
    else {
        WriteData(_fd, "504 The server supports the verb but does not support the parameter.\r\n");
    }
    return 0;
}

int CmdCwd(int _fd, int client_no, char * param)
{
    char pre_path[PATH_SIZE], after_path[PATH_SIZE], cur_path[PATH_SIZE];
    getcwd(pre_path, PATH_SIZE);

    EncodeNameprefix(cur_path, client_array[client_no].name_prefix);

    if (param[0] == '/') {
        EncodeNameprefix(after_path, param);
    }
    else {
        EncodePathname(after_path, _fd, param);
    }

    if (strstr(param, "..") != NULL) 
    {
        if (strcmp(param, "..") != 0) 
        {
            WriteData(_fd, "501 The server does not like '..' in param.");
            return -1;
        }
        else if (strlen(cur_path) <= strlen(root))
        {
            WriteData(_fd, "530 Permission denied.");
            return -1;
        }
    }

    DIR *dp;
    if ((dp = opendir(after_path)) == NULL) {
        WriteData(_fd, "550 No such file or directory.\r\n");
    }
    else {
        chdir(after_path);
        getcwd(after_path, PATH_SIZE);
        size_t len = strlen(root);
        if (root[len - 1] == '/')
            len--;
        strcpy(client_array[client_no].name_prefix, after_path + len);
        if (strlen(client_array[client_no].name_prefix) == 0)
            strcpy(client_array[client_no].name_prefix, "/");
        WriteData(_fd, "250 Okay.\r\n");
    }

    chdir(pre_path);
    return 0;
}

int CmdPwd(int _fd, int client_no)
{
    char response_msg[BUFFER_SIZE];
    sprintf(response_msg, "257 \"%s\" is the name prefix.\r\n", client_array[client_no].name_prefix);
    WriteData(_fd, response_msg);
    return 0;
}

int CmdPasv(int _fd, int client_no)
{
    struct sockaddr_in addr;
    socklen_t size_sockaddr = sizeof(struct sockaddr);
    getsockname(_fd, (struct sockaddr *)&addr, &size_sockaddr);
    char *ip_str = inet_ntoa(addr.sin_addr);
    int ip[4];
    sscanf(ip_str, "%d.%d.%d.%d", ip, ip + 1, ip + 2, ip + 3);

    srand((int)time(0));

    int port = DEFAULT_SERVER_DATA_PORT + (int)(15000.0 * rand() / (RAND_MAX + 1.0));
    client_array[client_no].data_port = port;
    client_array[client_no].mode = PASV_MODE;
    if (client_array[client_no].data_fd != 0) {
        close(client_array[client_no].data_fd);
        printf("Server => Closed connection on FD %d\n", client_array[client_no].data_fd);
    }

    client_array[client_no].data_fd = CreateSocket(port);
    printf("Server => Start listening FD %d @ localhost:%d. Waiting for data.\n", client_array[client_no].data_fd, port);
    char response_msg[BUFFER_SIZE];
    sprintf(response_msg, "227 =%d,%d,%d,%d,%d,%d.\r\n", ip[0], ip[1], ip[2], ip[3], (port >> 8) & 0xFF, port & 0xFF);
    WriteData(_fd, response_msg);
    return 0;
}

int CmdPort(int _fd, int client_no, char * param)
{
    int ip[6];
    sscanf(param, "%d,%d,%d,%d,%d,%d", ip, ip + 1, ip + 2, ip + 3, ip + 4, ip + 5);
    int port = (ip[4] << 8) + ip[5];
    if (client_array[client_no].data_fd != 0) {
        close(client_array[client_no].data_fd);
        printf("Server => Closed connection on FD %d\n", client_array[client_no].data_fd);
        client_array[client_no].data_fd = 0;
    }
    client_array[client_no].data_port = port;
    client_array[client_no].mode = PORT_MODE;
    WriteData(_fd, "200 Accept port.\r\n");
    return 0;
}

int CmdList(int _fd, int client_no, char * param)
{
    int connect_fd;
    struct stat stat_buf;

    char pre_path[PATH_SIZE], after_path[PATH_SIZE];
    memset(pre_path, 0, PATH_SIZE);
    memset(after_path, 0, PATH_SIZE);
    getcwd(pre_path, PATH_SIZE);

    if (strlen(param) != 0) {
        EncodePathname(after_path, _fd, param);
    }
    else {
        EncodeNameprefix(after_path, client_array[client_no].name_prefix);
    }
    chdir(after_path);

    connect_fd = BuildConnectionToClient(_fd);

    if (client_array[client_no].mode > IDLE_MODE) {
        if (stat(after_path, &stat_buf) < 0) {
            WriteData(_fd, "451 Path doesn't exist.\r\n");
        }
        else {
            if (S_ISDIR(stat_buf.st_mode)) {
                DIR *dp = opendir(after_path);
                if (!dp) {
                    WriteData(_fd, "451 Open directory failed.\r\n");
                }
                else {
                    WriteData(_fd, "150 Directory list begin to transfer.\r\n");

                    FILE *output = popen("ls -l", "r");
                    char buf[BUFFER_SIZE];
                    while (fgets(buf, sizeof(buf), output) != NULL)
                    {
                        if (buf[0] == 'd' || buf[0] == '-')
                        {
                            if (buf[strlen(buf) - 1] == '\n') {
                                buf[strlen(buf) - 1] = '\0';
                            }
                            if (dprintf(connect_fd, "%s\r\n", buf) < 0)
                            {
                                CloseDataConnection(_fd, connect_fd);
                                chdir(pre_path);
                                return -1;
                            }
                        }
                    }
                    pclose(output);

                    WriteData(_fd, "226 Directory list finish.\r\n");
                }
            }
            else {
                WriteData(_fd, "150 Directory list begin to transfer.\r\n");

                char full_command[PATH_SIZE] = "ls -l ";
                strcat(full_command, after_path);
                FILE *output = popen(full_command, "r");

                char buf[BUFFER_SIZE];
                while (fgets(buf, sizeof(buf), output) != NULL)
                {
                    if (buf[0] == 'd' || buf[0] == '-')
                    {
                        if (buf[strlen(buf) - 1] == '\n') {
                            buf[strlen(buf) - 1] = '\0';
                        }
                        if (dprintf(connect_fd, "%s\r\n", buf) < 0) 
                        {
                            CloseDataConnection(_fd, connect_fd);
                            chdir(pre_path);
                            return -1;
                        }
                    }
                }
                pclose(output);

                WriteData(_fd, "226 Directory list transfer complete.\r\n");
            }
        }
    }
    else {
        WriteData(_fd, "425 Please use PASV or PORT to build TCP connection.\r\n");
    }

    CloseDataConnection(_fd, connect_fd);
    chdir(pre_path);
    return 0;
}

int CmdRetr(int _fd, int client_no, char * param)
{
    int connect_fd, file_fd;
    struct stat stat_buf;
    char path[PATH_SIZE];
    if (strlen(param) > 0) {
        connect_fd = BuildConnectionToClient(_fd);
        EncodePathname(path, _fd, param);
        if (client_array[client_no].mode > IDLE_MODE) {
            if (access(path, R_OK) == 0 && (file_fd = open(path, O_RDONLY))) {
                fstat(file_fd, &stat_buf);
                WriteData(_fd, "150 Start to transfer file in binary mode.\r\n");
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    printf("Error pipe(): %s(%d)\n", strerror(errno), errno);
                }

                ssize_t size;
                while ((size = splice(file_fd, 0, pipe_fd[1], NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE)) > 0) {
                    splice(pipe_fd[0], NULL, connect_fd, 0, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
                }

                if (size < 0) {
                    printf("Error splice(): %s(%d)\n", strerror(errno), errno);
                    WriteData(_fd, "426 File transfer pipe broken.\r\n");
                }
                else {
                    WriteData(_fd, "226 File transfer complete.\r\n");
                }
            }
            else {
                WriteData(_fd, "551 Fail to open file.\r\n");
            }
        }
        else {
            WriteData(_fd, "425 Please use PASV or PORT to build TCP connection.\r\n");
        }
        CloseDataConnection(_fd, connect_fd);
    }
    else {
        WriteData(_fd, "504 The server supports the verb but does not support the parameter.\r\n");
    }
    return 0;
}

int CmdStor(int _fd, int client_no, char * param)
{
    int connect_fd, file_fd;
    char path[PATH_SIZE];
    if (strlen(param) > 0) {
        connect_fd = BuildConnectionToClient(_fd);
        EncodePathname(path, _fd, param);
        if (client_array[client_no].mode > IDLE_MODE) {
            FILE *fp = fopen(path, "wb");
            if (fp == NULL) {
                printf("Error fopen(): %s(%d)\n", strerror(errno), errno);
            }
            file_fd = fileno(fp);
            int pipe_fd[2];
            if (pipe(pipe_fd) == -1) {
                printf("Error pipe(): %s(%d)\n", strerror(errno), errno);
            }

            WriteData(_fd, "150 Start to receive file in binary mode.\r\n");

            ssize_t size;
            while ((size = splice(connect_fd, 0, pipe_fd[1], NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE)) > 0) {
                splice(pipe_fd[0], NULL, file_fd, 0, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
            }

            if (size < 0) {
                printf("Error splice(): %s(%d)\n", strerror(errno), errno);
                WriteData(_fd, "426 File receive pipe broken.\r\n");
            }
            else {
                WriteData(_fd, "226 File receive complete.\r\n");
            }
        }
        else {
            WriteData(_fd, "425 Please use PASV or PORT to build TCP connection.\r\n");
        }
        CloseDataConnection(_fd, connect_fd);
    }
    else {
        WriteData(_fd, "504 The server supports the verb but does not support the parameter.\r\n");
    }
    return 0;
}

int CmdMkd(int _fd, int client_no, char * param)
{
    char path[PATH_SIZE];
    if (param[0] == '/') {
        EncodeNameprefix(path, param);
    }
    else {
        EncodePathname(path, _fd, param);
    }

    DIR *dp;
    if ((dp = opendir(path)) == NULL) {
        if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
            printf("Error mkdir(): %s(%d)\n", strerror(errno), errno);
            WriteData(_fd, "550 Making directory failed.\r\n");
        }
        else {
            size_t len = strlen(root);
            if (root[len - 1] == '/')
                len--;
            char response_msg[BUFFER_SIZE];
            sprintf(response_msg, "250 \"%s\" is the new directory.\r\n", path + len);
            WriteData(_fd, response_msg);
        }
    }
    else {
        WriteData(_fd, "550 Making directory failed.\r\n");
    }
    return 0;
}

int CmdRmd(int _fd, int client_no, char * param)
{
    char path[PATH_SIZE];
    if (param[0] == '/') {
        EncodeNameprefix(path, param);
    }
    else {
        EncodePathname(path, _fd, param);
    }

    DIR *dp;
    if ((dp = opendir(path)) != NULL) {
        if (rmdir(path) == -1) {
            printf("Error rmdir(): %s(%d)\n", strerror(errno), errno);
            WriteData(_fd, "550 Remove directory failed.\r\n");
        }
        else {
            size_t len = strlen(root);
            if (root[len - 1] == '/')
                len--;
            char response_msg[BUFFER_SIZE];
            sprintf(response_msg, "250 Remove \"%s\" successfully.\r\n", path + len);
            WriteData(_fd, response_msg);
        }
    }
    else {
        WriteData(_fd, "550 Remove directory failed.\r\n");
    }
    return 0;
}

int CmdQuit(int _fd, int client_no)
{
    if (client_array[client_no].mode == PASV_MODE) {
        close(client_array[client_no].data_fd);
        printf("Server => Closed connection on FD %d\n", client_array[client_no].data_fd);
        client_array[client_no].data_fd = 0;
    }
    WriteData(_fd, "221 Bye.\r\n");
    printf("Server => Closed connection on FD %d\n", _fd);
    close(_fd);

    // Delete from client status array.
    for (int j = client_no; j < MAX_CONNECTIONS - 1; j++) {
        client_array[j] = client_array[j + 1];
    }
    current_connections--;

    return 0;
}

