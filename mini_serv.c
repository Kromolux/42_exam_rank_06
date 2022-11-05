#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>


typedef struct client {
    int     fd;
    int     id;
    char    *buffer;
}   client;

void    exit_wrong(void)
{
    write(STDERR_FILENO, "Wrong number of arguments\n", 27);
    exit(1);
}

void    exit_fatal(int fd)
{
    write(STDERR_FILENO, "Fatal error\n", 13);
    close(fd);
    exit(1);
}

int extract_message(int fd, char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				exit_fatal(fd);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

void    send_msg(int sender_client_fd, struct client *clients, int client_max, char *msg)
{
    for (int i = 3; i <= client_max; ++i)
    {
        if (clients[i].fd != 0 && clients[i].fd != sender_client_fd)
            send(clients[i].fd, (void *) msg, strlen(msg), MSG_DONTWAIT);
    }
}

int main(int argc, char **argv)
{
	int sockfd, port;
	struct sockaddr_in servaddr; 

    if (argc != 2)
        exit_wrong();

    port = atoi(argv[1]);

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if ( sockfd == -1)
        exit_fatal(sockfd);

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        exit_fatal(sockfd);

	if (listen(sockfd, 10) != 0)
        exit_fatal(sockfd);

    int max_fd = sockfd;
    struct fd_set select_fd, read_fd;
    FD_ZERO(&select_fd);
    FD_SET(sockfd, &select_fd);

    size_t rc;
    int actual_client = 0;
    int new_client_fd;
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE + 1];
    char tmp[BUFFER_SIZE + 42];
    struct client clients[FD_SETSIZE + 1];
    char *msg = NULL;
    while (1)
    {
        read_fd = select_fd;
        if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) > 0)
        {
            for (int i = 3; i <= max_fd; ++i)
            {
                if (FD_ISSET(i, &read_fd))
                {
                    if (i == sockfd)
                    {
                        new_client_fd = accept(sockfd, NULL, NULL);
                        if (new_client_fd > 0)
                        {
                            FD_SET(new_client_fd, &select_fd);
                            clients[new_client_fd].fd = new_client_fd;
                            clients[new_client_fd].id = actual_client;
                            clients[new_client_fd].buffer = (void*) calloc(1,1);
                            ++actual_client;
                            if (new_client_fd > max_fd)
                                max_fd = new_client_fd;
                            sprintf(tmp, "server: client %d just arrived\n", clients[new_client_fd].id);
                            send_msg(new_client_fd, clients, max_fd, tmp);
                        }
                    }
                    else
                    {
                        rc = recv(i, (void*) buffer, BUFFER_SIZE, 0);
                        if (rc <= 0 || rc > BUFFER_SIZE)
                        {
                            close(i);
                            FD_CLR(i, &select_fd);
                            clients[i].fd = 0;
                            sprintf(tmp, "server: client %d just left\n", clients[i].id);
                            send_msg(i, clients, max_fd, tmp);
                            break;
                        }
                        buffer[rc] = '\0';
                        clients[i].buffer = realloc(clients[i].buffer, strlen(clients[i].buffer) + rc + 1);
                        strcat(clients[i].buffer, buffer);
                        msg = NULL;
                        while (extract_message(sockfd, &clients[i].buffer, &msg) == 1)
                        {
                            sprintf(tmp, "client %d: %s", clients[i].id, msg);
                            send_msg(i, clients, max_fd, tmp);
                            free(msg);
                            msg = NULL;
                        }
                    }
                }
            }
        }
    }
   return (0);
}
