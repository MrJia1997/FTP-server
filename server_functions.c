#include "server.h"

int CreateSocket(int _port)
{
    struct sockaddr_in addr;
    addr.sin_port = htons((uint16_t)_port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return -1;
    }
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        abort();
    }
    if (listen(fd, MAX_CONNECTIONS) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        abort();
    }
    return fd;
}

ssize_t ReadData(int _fd, char *_buffer, size_t _buf_size) {
    ssize_t p = 0;
    while (1) {
        ssize_t n = read(_fd, _buffer + p, _buf_size - 1 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(_fd);
            return 0;
        }
        else if (n == 0) {
            return 0;
        }
        else {
            p += n;
            if (_buffer[p - 1] == '\n') {
                break;
            }
        }
    }
    _buffer[p - 1] = '\0';
    return p - 1;
}

ssize_t WriteData(int _fd, char *_buffer) {
    ssize_t p = 0;
    size_t len = strlen(_buffer); // Add '\0'
    printf("%s\n", _buffer);
    while (p < len) {
        ssize_t n = write(_fd, _buffer + p, len - p);
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return 0;
        }
        else {
            p += n;
        }
    }
    return len;
}

int UnblockSocket(int _fd)
{
    int flag_1, flag_2;
    flag_1 = fcntl(_fd, F_GETFL, 0);
    if (flag_1 == -1) {
        perror("fcntl");
        return -1;
    }
    flag_1 |= O_NONBLOCK;
    flag_2 = fcntl(_fd, F_SETFL, flag_1);
    if (flag_2 == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

int HandleCommand(int _fd, char * _cmd)
{
    printf("Client %d => %s\n", _fd, _cmd);
    
    // Process string
    size_t len = strlen(_cmd);
    char verb[VERB_SIZE], param[PARAM_SIZE];
    memset(verb, 0, sizeof(verb));
    memset(param, 0, sizeof(param));
    int space_pos = -1;
    for (int i = 0; i < len; i++) {
        if (_cmd[i] == ' ') {
            space_pos = i;
            _cmd[i] = '\0';
            strcpy(param, _cmd + i + 1);
            break;
        }
    }
    strcpy(verb, _cmd);
    if (space_pos < 0) {
        size_t len = strlen(verb);
        verb[len - 1] = '\0';
    }
    else {
        size_t len = strlen(param);
        param[len - 1] = '\0';
    }

    int client_no;
    if ((client_no = FindClientByFD(_fd)) == -1) {
        WriteData(_fd, "500 Server internal error.\r\n");
        memset(param, 0, sizeof(param));
        return -1;
    }

    // Handle commands
    // USER
    if (strcmp(verb, "USER") == 0) {
        CmdUser(_fd, client_no, param);
    }
    // PASS
    else if (strcmp(verb, "PASS") == 0) {
        CmdPass(_fd, client_no, param);
    }
    // SYST
    else if (strcmp(verb, "SYST") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdSyst(_fd, client_no);
    }
    // TYPE
    else if (strcmp(verb, "TYPE") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdType(_fd, client_no, param);
    }
    // CWD
    else if (strcmp(verb, "CWD") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdCwd(_fd, client_no, param);
    }
    // PWD
    else if (strcmp(verb, "PWD") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdPwd(_fd, client_no);
    }
    // PASV
    else if (strcmp(verb, "PASV") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdPasv(_fd, client_no);
    }
    // PORT
    else if (strcmp(verb, "PORT") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdPort(_fd, client_no, param);
    }
    // LIST
    else if (strcmp(verb, "LIST") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdList(_fd, client_no, param);
    }
    // RETR
    else if (strcmp(verb, "RETR") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdRetr(_fd, client_no, param);
    }
    // STOR
    else if (strcmp(verb, "STOR") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdStor(_fd, client_no, param);
    }
    // MKD
    else if (strcmp(verb, "MKD") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdMkd(_fd, client_no, param);
    }
    // RMD
    else if (strcmp(verb, "RMD") == 0 && client_array[client_no].mode >= IDLE_MODE) {
        CmdRmd(_fd, client_no, param);
    }
    // QUIT
    else if (strcmp(verb, "QUIT") == 0) {
        CmdQuit(_fd, client_no);
    }
    // Others
    else {
        if (client_array[client_no].mode < IDLE_MODE)
            WriteData(_fd, "530 Permission denied.\r\n");
        else
            WriteData(_fd, "502 The server hasn't support this verb yet.\r\n");
    }
    memset(param, 0, sizeof(param));
    return 0;
}

int ConcatPath(char * _dest, char * _src)
{
    size_t len = strlen(_dest);
    if (_dest[len - 1] == '/' && _src[0] == '/') {
        _dest[len - 1] = '\0';
    }
    else if (_dest[len - 1] != '/' && _src[0] != '/') {
        _dest[len] = '/';
        _dest[len + 1] = '\0';
    }
    strcat(_dest, _src);
    return 0;
}

int EncodePathname(char * _target, int _fd, char * _path)
{
    memset(_target, 0, sizeof(_target));
    strcpy(_target, root);
    if (_path[0] == '/') {
        ConcatPath(_target, _path);
    }
    else {
        int i = FindClientByFD(_fd);
        if (i == -1) {
            fprintf(stderr, "EncodePathname(): Server internal error\n");
            return -1;
        }
        ConcatPath(_target, client_array[i].name_prefix);
        ConcatPath(_target, _path);
    }
    return 0;
}

int EncodeNameprefix(char * _target, char * _name_prefix)
{
    strcpy(_target, root);
    ConcatPath(_target, _name_prefix);
    size_t len = strlen(_target);
    if (_target[len - 1] == '/') {
        _target[len - 1] = '\0';
    }
    return 0;
}

int FindClientByFD(int _fd)
{
    int i = 0;
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        if (client_array[i].connect_fd == _fd) {
            break;
        }
    }
    if (i == MAX_CONNECTIONS) {
        fprintf(stderr, "FindClientByFD(): Server internal error\n");
        return -1;
    }
    else {
        return i;
    }
}

int BuildConnectionToClient(int _fd)
{
    int connect_fd;
    char host_buf[NI_MAXHOST], server_buf[NI_MAXSERV];
    int client_no = FindClientByFD(_fd);
    struct sockaddr_in addr_client;
    socklen_t size_sockaddr = sizeof(struct sockaddr);
    memset(&addr_client, 0, sizeof(addr_client));
    if (client_array[client_no].mode == PASV_MODE) {
        socklen_t size_sockaddr = sizeof(addr_client);
        connect_fd = accept(client_array[client_no].data_fd, &addr_client, &size_sockaddr);
    }
    else if (client_array[client_no].mode == PORT_MODE) {
        connect_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        addr_client.sin_family = AF_INET;
        addr_client.sin_port = htons((uint16_t)client_array[client_no].data_port);
        inet_pton(AF_INET, client_array[client_no].ip, &addr_client.sin_addr);
        connect(connect_fd, (struct sockaddr*)&addr_client, sizeof(addr_client));
    }
    if (getnameinfo((struct sockaddr*)&addr_client, size_sockaddr, host_buf, sizeof(host_buf), server_buf, sizeof(server_buf), (NI_NUMERICHOST | NI_NUMERICSERV)) == 0) {
        printf("Server => Build connection on FD %d, Addr %s:%s\n", connect_fd, host_buf, server_buf);
    }
    return connect_fd;
}

int CloseDataConnection(int _fd, int connect_fd)
{
    close(connect_fd);
    printf("Server => Closed connection on FD %d\n", connect_fd);
    int client_no = FindClientByFD(_fd);
    if (client_array[client_no].mode == PASV_MODE) {
        close(client_array[client_no].data_fd);
        printf("Server => Closed connection on FD %d\n", client_array[client_no].data_fd);
        client_array[client_no].data_fd = 0;
    }
    client_array[client_no].mode = IDLE_MODE;
    return 0;
}

int GeneratePermissionString(char * _target, int _permission)
{
    int current_permission = 0;
    int read, write, exec;
    read = write = exec = 0;
    char temp[3];
    memset(temp, 0, sizeof(temp));
    for (int i = 6; i >= 0; i -= 3) {
        current_permission = ((_permission & ALLPERMS) >> i) & 0x7;
        read = (current_permission >> 2) & 0x1;
        write = (current_permission >> 1) & 0x1;
        exec = (current_permission) & 0x1;

        sprintf(temp, "%c%c%c",
            read ? 'r' : '-',
            write ? 'w' : '-',
            exec ? 'x' : '-');
        strcat(_target, temp);

    }
    return 0;
}

int GenerateLsFormatString(char * _target, struct stat * _st, struct dirent * _entry)
{
    char time_buf[100];
    memset(time_buf, 0, sizeof(time_buf));
    char permission_buf[9];
    memset(permission_buf, 0, sizeof(permission_buf));
    
    time_t raw_time = _st->st_mtime;
    struct tm* time_file = localtime(&raw_time);
    time_t now = time(NULL);
    if((now - raw_time) < SIX_MONTH_SECONDS)
        strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", time_file);
    else
        strftime(time_buf, sizeof(time_buf), "%b %d  %Y", time_file);
    
    GeneratePermissionString(permission_buf, (_st->st_mode & ALLPERMS));

    sprintf(_target, "%c%s %d %d %d %13d %s %s",
        (_entry->d_type == DT_DIR) ? 'd' : '-',
        permission_buf,
        (int)_st->st_nlink,
        _st->st_uid,
        _st->st_gid,
        (int)_st->st_size,
        time_buf,
        _entry->d_name);
    return 0;
}
