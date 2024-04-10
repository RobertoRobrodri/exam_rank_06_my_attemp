#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>

#define BUFFER_SIZE 20000

typedef struct s_client {
	int 			fd;
	int 			id;
	struct s_client *next;
} t_client;

/* list functions */

t_client *lst_new(int fd, int id)
{
	t_client *new;

	new = malloc(sizeof(t_client));
	if (new == NULL)
		return NULL;
	new->fd = fd;
	new->id = id;
	new->next = NULL;
	return new;
}

void lst_add_back(t_client **lst, t_client *new)
{
	t_client *aux;

	aux = *lst;
	if (*lst == NULL)
		*lst = new;
	else
	{
		while (aux->next != NULL)
			aux = aux->next;
		aux->next = new;
		
	}
}

t_client *find_in_list(t_client **lst, int fd)
{
	t_client *aux = *lst;

	while (aux != NULL)
	{
		if (aux->fd == fd)
			break;
		aux = aux->next;
	}
	if (aux == NULL)
		return NULL;
	return aux;
}

void	remove_from_list(t_client **lst, int fd)
{
	t_client *aux = *lst;
	t_client *prev = NULL;

	if (aux != NULL && aux->fd == fd)
	{
		*lst = aux->next;
		free(aux);
		return ;
	}
	while (aux != NULL)
	{
		if (aux->fd == fd)
			break;
		prev = aux;
		aux = aux->next;	
	}
	if (aux == NULL)
		return ;
	prev->next = aux->next;
	free(aux);
	return ;
}

void free_list(t_client **lst)
{
	t_client *aux = *lst;

	while (aux != NULL)
	{
		aux = (*lst)->next;
		close((*lst)->fd);
		free(*lst);
		*lst = aux;
	}
}

int find_max_fd(t_client **lst, int socket_fd)
{
	t_client *aux = *lst;
	int fd = socket_fd;

	while (aux != NULL)
	{
		if (aux->fd > fd)
			fd = aux->fd;
		aux = aux->next;
	}
	return fd;
}

// Utils

int	throw_error(char *str)
{
	write(STDERR_FILENO, str, strlen(str));
	return 1;
}

int send_msg_to_all(t_client *user_list, int fd, char *msg, fd_set write_sockets)
{
	t_client *aux = user_list;

	while (aux != NULL)
	{
		if (aux->fd != fd && FD_ISSET(aux->fd, &write_sockets) > 0)
			send(aux->fd, msg, strlen(msg), 0);
		aux = aux->next;
	}
	return 0;
}

// IRC

int main_loop(int socket_fd)
{
	fd_set client_sockets, read_sockets, write_sockets;
	int max_fd = socket_fd;
	t_client *user_list = NULL;
	t_client *aux = NULL;
	int len, new_fd;
	int id = 0;
	char buffer[BUFFER_SIZE];

	FD_ZERO(&client_sockets);
	FD_SET(socket_fd, &client_sockets);
	while (1)
	{
		read_sockets = client_sockets;
		write_sockets = client_sockets;
		select(max_fd + 1, &read_sockets, &write_sockets, NULL, NULL);
		
		// Check fd activity
		for (int i = 0; i <= max_fd; i++)
		{
			if (FD_ISSET(i, &read_sockets) > 0)
			{
				memset(buffer, 0, BUFFER_SIZE);
				// join client
				if (i == socket_fd)
				{
					new_fd = accept(socket_fd, NULL, NULL);
					if (new_fd == -1)
						continue ;
					lst_add_back(&user_list, lst_new(new_fd, id));
					FD_SET(new_fd, &client_sockets);
					sprintf(buffer, "server: client %d just arrived\n", id++);
					send_msg_to_all(user_list, new_fd, buffer, write_sockets);
					max_fd = find_max_fd(&user_list, socket_fd);
				}
				else
				{
					// Recv
					len = 1000;
					while (len == 1000)
						len = recv(i, buffer + strlen(buffer), 1000, 0);
					
						// desconectar
					if (len <= 0 && *buffer == 0)
					{
						aux = find_in_list(&user_list, i);
						sprintf(buffer, "server: client %d just left\n", aux->id);
						FD_CLR(i, &client_sockets);
						close(i);
						remove_from_list(&user_list, i);
						send_msg_to_all(user_list, i, buffer, write_sockets);
						max_fd = find_max_fd(&user_list, socket_fd);
					}
					// enviar msg
					else
					{
						char msg[BUFFER_SIZE + 100];
						char tmp[BUFFER_SIZE];
						aux = find_in_list(&user_list, i);
						int j = 0;
						int k = 0;
						memset(tmp, 0, BUFFER_SIZE);
						while (j < len)
						{
							tmp[k] = buffer[j];
							k++;
							if (buffer[j] == '\n')
							{
								sprintf(msg, "client %d: %s", aux->id, tmp);
								send_msg_to_all(user_list, i, msg, write_sockets);
								k = 0;
								memset(tmp, 0, BUFFER_SIZE);
								memset(msg, 0, BUFFER_SIZE);
							}
							j++;
						}
					}
				}
			}
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2)
		return (throw_error("Wrong number of arguments\n"));

	int socket_fd;
	struct sockaddr_in addr;

	// socket create and verification 
	socket_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (socket_fd == -1)
		return throw_error("Fatal error\n");
	addr.sin_family = AF_INET; 
	addr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	addr.sin_port = htons(atoi(argv[1])); 
	// Binding newly created socket to given IP and verification 
	if ((bind(socket_fd, (const struct sockaddr *)&addr, sizeof(addr))) != 0) 
		return throw_error("Fatal error\n");
	if (listen(socket_fd, 0) != 0)
		return throw_error("Fatal error\n");
	return main_loop(socket_fd);
}
