#include "server.h"

int main(int argc, char **argv) {
	int listen_fd, epoll_fd;
    int port = DEFAULT_SERVER_CONTROL_PORT;
    struct epoll_event e, *events;

    strcpy(root, DEFAULT_ROOT_PATH);

    // Params handler
    // TODO: More elegant style
	if (argc > 1) {
		int i = 1;
		while (i < argc) {
			if (strcmp(argv[i], "-port") == 0) {
				i++;
				port = atoi(argv[i]);
				if (port < 1 || port > 0xFFFF) {
					fprintf(stderr, "Error port: syntax error\n");
					abort();
				}
				i++;
			}
			else if (strcmp(argv[i], "-root") == 0) {
				i++;
				if (strlen(argv[i]) > BUFFER_SIZE) {
					fprintf(stderr, "Error root: syntax error\n");
					abort();
				}
                if (argv[i][0] == '/') {
                    strcpy(root, argv[i]);
                }
                else {
                    getcwd(root, sizeof(root));
                    strcat(root, "/");
                    strcat(root, argv[i]);
                }
                i++;
			}
		}
	}

    chdir(root);

    listen_fd = CreateSocket(port);

    // Unblock
    if (UnblockSocket(listen_fd) == -1) {
        abort();
    }

    // Epoll
    if ((epoll_fd = epoll_create1(0)) == -1) {
        printf("Error epoll_create1(): %s(%d)\n", strerror(errno), errno);
        abort();
    }

    e.data.fd = listen_fd;
    e.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &e) == -1) {
        printf("Error epoll_ctl(): %s(%d)\n", strerror(errno), errno);
        abort();
    }

    events = calloc(MAX_EVENTS, sizeof e);

    // Initialize complete
    printf("Server => FTP Server started up @ localhost:%d. Waiting for clients...\n", port);

    // Main loop
    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!events[i].events & EPOLLIN)) {
                
                fprintf(stderr, "Error epoll\n");
                close(events[i].data.fd);
                continue;
            }
            else if (listen_fd == events[i].data.fd) {
                // Have notification on listening socket
                while (1) {
                    int connect_fd;
                    char host_buf[NI_MAXHOST], server_buf[NI_MAXSERV];
                    struct sockaddr_in addr_client;
                    socklen_t size_sockaddr = sizeof(struct sockaddr);
                    if ((connect_fd = accept(listen_fd, (struct sockaddr*)&addr_client, &size_sockaddr)) == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            // All incoming connections have been processed.
                            break;
                        }
                        else {
                            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                            break;
                        }
                    }

                    if (getnameinfo((struct sockaddr*)&addr_client, size_sockaddr, host_buf, sizeof(host_buf), server_buf, sizeof(server_buf), (NI_NUMERICHOST | NI_NUMERICSERV)) == 0) {
                        printf("Server => Accept connection on FD %d, Addr %s:%s\n", connect_fd, host_buf, server_buf);
                    }

                    // Add to client status array.
                    client_array[current_connections].connect_fd = connect_fd;
                    client_array[current_connections].data_fd = 0;
                    client_array[current_connections].binary_flag = 0;
                    strcpy(client_array[current_connections].name_prefix, "/");
                    client_array[current_connections].data_port = 0;
                    strcpy(client_array[current_connections].ip, host_buf);
                    client_array[current_connections].mode = UNLOGIN_MODE;

                    current_connections++;

                    // Unblock
                    if (UnblockSocket(connect_fd) == -1) {
                        abort();
                    }
                    
                    e.data.fd = connect_fd;
                    e.events = EPOLLIN | EPOLLET;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connect_fd, &e) == -1) {
                        printf("Error epoll_ctl(): %s(%d)\n", strerror(errno), errno);
                        abort();
                    }

                    // Welcome msg
                    WriteData(connect_fd, "220 Anonymous FTP server ready.\r\n");

                }
                continue;
            }
            else {
                char buf[BUFFER_SIZE];
                ssize_t len;
                len = ReadData(events[i].data.fd, buf, sizeof(buf));
                if (len == 0) {
                    printf("Server => Closed connection on FD %d\n", events[i].data.fd);
                    close(events[i].data.fd);
                    
                    // Delete from client status array.
                    int j = FindClientByFD(events[i].data.fd);
                    if (j == -1) {
                        continue;
                    }
                    if (client_array[j].mode == PASV_MODE) {
                        close(client_array[j].data_fd);
                        printf("Server => Closed connection on FD %d\n", client_array[j].data_fd);
                        client_array[j].data_fd = 0;
                    }
                    for (; j < MAX_CONNECTIONS - 1; j++) {
                        client_array[j] = client_array[j + 1];
                    }
                    current_connections--;
                }
                else {
                    // Command handler
                    HandleCommand(events[i].data.fd, buf);
                }
            }
        }
	}

    //End
    free(events);
    printf("Server => FTP Server closed.\n");
	close(listen_fd);
}

