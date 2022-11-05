#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

typedef struct client {
	int		fd;
	int		id;
	char	*buffer;
}	client;

void	exit_wrong(void)
{
	write(STDERR_FILENO, "Wrong number of arguments\n", 27);
	exit(1);
}

void	exit_fatal(int fd)
{
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(fd);
	exit(1);
}

void	send_msg(int sender_fd, client *clients, int max_fd, char * msg)
{
	for (int i = 3; i <= max_fd; ++i)
		if (clients[i].fd != 0 && clients[i].fd != sender_fd)
			send(clients[i].fd, (void*) msg, strlen(msg), MSG_DONTWAIT);
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

int main(int argc, char ** argv)
{
	if (argc != 2)
		exit_wrong();

	int port = atoi(argv[1]);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// socket create and verification 
	if (sockfd == -1)
		exit_fatal(sockfd);

	struct sockaddr_in servaddr;
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

	int new_client_fd = 0;
	int actual_client_id = 0;
	int max_fd = sockfd;

	size_t	rc = 0;

	const int	BUFFER_SIZE = 0xffff;
	char		buffer[BUFFER_SIZE + 1];
	char		tmp[BUFFER_SIZE + 42];
	char		*msg = NULL;

	struct	client clients[FD_SETSIZE + 1];
	struct	fd_set select_fd, read_fd;
	FD_ZERO(&select_fd);
	FD_SET(sockfd, &select_fd);

	while(1)
	{
		read_fd = select_fd;
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) <= 0)
			continue;
		for (int i = 3; i <= max_fd; ++i)
		{
			if (!FD_ISSET(i, &read_fd))
				continue;
			if (i == sockfd)
			{
				new_client_fd = accept(sockfd, NULL, NULL);
				if (new_client_fd > 0)
				{
					clients[new_client_fd].fd = new_client_fd;
					clients[new_client_fd].id = actual_client_id;
					clients[new_client_fd].buffer = (void*) calloc(1, 1);
					++actual_client_id;
					FD_SET(new_client_fd, &select_fd);
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
					FD_CLR(i, &select_fd);
					sprintf(tmp, "server: client %d just left\n", clients[i].id);
					send_msg(i, clients, max_fd, tmp);
					free(clients[i].buffer);
					close(i);
					clients[i].fd = 0;
					clients[i].id = -1;
					continue ;
				}

				buffer[rc] = '\0';
				clients[i].buffer = (void*) realloc(clients[i].buffer, strlen(clients[i].buffer) + rc + 1);
				strcat(clients[i].buffer, buffer);

				while (extract_message(i, &clients[i].buffer, &msg) == 1)
				{
					sprintf(tmp, "client %d: %s", clients[i].id, msg);
					send_msg(i, clients, max_fd, tmp);
					free(msg);
					msg = NULL;
				}
			}
		}
	}
	return (0);
}
