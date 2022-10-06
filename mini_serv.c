#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netinet/in.h>

typedef struct client {
	int			fd;
	int			id;
	char		*buffer;
}			client;

void	exit_error(char * msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	exit (1);
}

int	is_int(char * input)
{
	for (; *input; ++input)
		if (*input > '9' || *input < '0')
			return (0);
	return (1);
}

void	send_msg(int sender_client_fd, struct client *clients, int client_max, char *tmp)
{
	for (int i = 4; i <= client_max; ++i)
	{
		if (clients[i].fd != 0 && clients[i].fd != sender_client_fd)
			send(clients[i].fd, (void*) tmp, strlen(tmp), MSG_DONTWAIT);
	}
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i;

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
				exit_error("Fatal error\n");
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

int	main(int argc, char ** argv)
{
	if (argc != 2)
		exit_error("Wrong number of arguments\n");
	if (is_int(argv[1]) == 0)
		exit_error("Fatal error\n");
	int	port_number = atoi(argv[1]);
	int	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (port_number < 0 || port_number > 0xffff || sock_fd == -1)
		exit_error("Fatal error\n");

	struct sockaddr_in serv_addr;
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	serv_addr.sin_port = htons(port_number);

	if (bind(sock_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
		exit_error("Fatal error\n");
	if (listen(sock_fd, 64) == -1)
		exit_error("Fatal error\n");

	int max_fd = sock_fd;
	struct fd_set select_fd, read_fd;
	FD_ZERO(&select_fd);
	FD_SET(sock_fd, &select_fd);

	int	actual_client_id = 0;
	int	new_client_fd;
	const int buffer_size = 4096;
	char buffer[buffer_size + 1];
	char tmp[buffer_size + 42];
	struct client clients[FD_SETSIZE + 1];
	while (1)
	{
		read_fd = select_fd;
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) > 0)
		{
			for (int i = 3; i <= max_fd; ++i)
			{
				if (FD_ISSET(i, &read_fd))
				{
					if (i == sock_fd) //new client connected
					{
						new_client_fd = accept(sock_fd, NULL, NULL);
						if (new_client_fd > 0)
						{
							FD_SET(new_client_fd, &select_fd);
							clients[new_client_fd].fd = new_client_fd;
							clients[new_client_fd].id = actual_client_id;
							clients[new_client_fd].buffer = calloc(1, sizeof(char));
							++actual_client_id;
							if (new_client_fd > max_fd)
								max_fd = new_client_fd;
							sprintf(tmp, "server: client %d just arrived\n", clients[new_client_fd].id);
							send_msg(new_client_fd, clients, max_fd, tmp);
						}
					}
					else
					{
						int	rc = recv(i, (void*) buffer, buffer_size, 0);
						if (rc <= 0 || rc > buffer_size)
						{
							close(i);
							FD_CLR(i, &select_fd);
							clients[i].fd = 0;
							sprintf(tmp, "server: client %d just left\n", clients[i].id);
							send_msg(i, clients, max_fd, tmp);
							if (clients[i].buffer != NULL)
							{
								free(clients[i].buffer);
								clients[i].buffer = NULL;
							}
							break; 
						}
						buffer[rc] = '\0';
						clients[i].buffer = realloc(clients[i].buffer, strlen(clients[i].buffer) + rc + 1);
						strcat(clients[i].buffer, buffer);
						char * msg = NULL;
						while (extract_message(&clients[i].buffer, &msg) == 1)
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
