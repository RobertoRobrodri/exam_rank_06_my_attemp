#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// select() can monitor only file descriptors numbers that are less than FD_SETSIZE (1024)

typedef struct user {
	int fd;
	int id;
	struct user *next;
}	user;

// list functions
user *lstnew(int fd, int id)
{
	user *lst;

	lst = malloc(sizeof(user));
	if (lst == NULL)
		return 0;
	lst->fd = fd;
	lst->id = id;
	lst->next = NULL;
	return lst;
}

void lst_add_back(user **lst, user *new)
{
	user *aux;

	if (*lst == 0)
		*lst = new;
	else
	{
		aux = *lst;
		while (aux->next != NULL)
			aux = aux->next;
		aux->next = new;
	}
}

user *find_in_list(user *lst, int fd)
{
	user *aux = lst;

	while (aux->next != NULL)
	{
		if (aux->fd == fd)
			break;
		aux = aux->next;
	}
	return aux;
}

// void print_list(user *lst)
// {
// 	user *aux = lst;

// 	while(aux != NULL)
// 	{
// 		printf("fd --> %d\n", aux->fd);
// 		aux = aux->next;
// 	}
// }

void remove_from_list(user **lst, int fd)
{
    user *aux = *lst;
    user *prev = NULL;

    // If the node to be removed is the head of the list
    if (aux != NULL && aux->fd == fd)
    {
        *lst = aux->next;
        free(aux);
        return;
    }

    // Find the node to be removed
    while (aux != NULL && aux->fd != fd)
    {
        prev = aux;
        aux = aux->next;
    }

    // If the node was not found
    if (aux == NULL)
        return;

    // Unlink the node from the list
    prev->next = aux->next;
    free(aux);
}

void free_list(user **lst)
{
	user *aux = *lst;

	while(*lst != NULL)
	{
		aux = (*lst)->next;
		free(*lst);
		*lst = aux;
	}
}

// utils
void throw_error(char *str)
{
	write(1, str, strlen(str));
}

struct sockaddr_in init_socket_struct(int port) {
	struct sockaddr_in addr;

	//  IPV4 addresses
	addr.sin_family = AF_INET;
	//  Convert our port to a network address (host to network)
	addr.sin_port = htons(port);
	//  Our address as integer
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	return addr;
}

fd_set init_fd_set(int socket_fd)
{
	fd_set client_fds;

	memset(&client_fds, 0, sizeof(client_fds));
	// limpiar el fd, primer paso para inicializar el set
	FD_ZERO(&client_fds);
	// incluir el fd del socket que escucha al set
	FD_SET(socket_fd, &client_fds);
	return client_fds;
}

// IRC

int send_msg_to_all(char *msg, user *client_list)
{
	int len;
	user *aux = client_list;

	while (aux != NULL)
	{
		len = send(aux->fd, msg, strlen(msg), 0);
		if (len == -1)
		{
			throw_error("Fatal error\n");
			return 1;
		}
		aux = aux->next;
	}
	return 0;
}

int main_loop(int socket_fd)
{
	fd_set client_sockets, ready_sockets;
	int max_fd = socket_fd;
	user *client_list = NULL;
	int id = 0;

	client_sockets = init_fd_set(socket_fd);
	while (1)
	{
		char buffer[20000];
		memset(buffer, 0, 20000);

		ready_sockets = client_sockets;
		if (select(max_fd + 1, &ready_sockets, NULL, NULL, NULL) < 0)
		{
			throw_error("Fatal error\n");
			return 1;
		}
		for (int i = 0; i <= max_fd; i++)
		{
			if (FD_ISSET(i, &ready_sockets) > 0)
			{
				if (i == socket_fd)
				{
				//	accept_communication(i, &client_sockets);
					int new_fd = accept(socket_fd, NULL, NULL);
					if (new_fd == -1)
					{
						throw_error("Fatal error\n");
						free_list(&client_list);
						return 1;
					}
					FD_SET(new_fd, &client_sockets);
					lst_add_back(&client_list, lstnew(new_fd, id));
					sprintf(buffer, "server: client %d just arrived\n", id++);
					if (send_msg_to_all(buffer, client_list))
					{
						free_list(&client_list);
						return 1;
					}
					max_fd = new_fd > max_fd ? new_fd : max_fd;
				}
				else
				{
				// 	receive_communication(i, &client_sockets);
					
					int len;

					len = recv(i, buffer, 20000, 0);
					switch (len) {
						case -1:
						{
							free_list(&client_list);
							return 1;
						}
						case 0:
						{
							close(i);
							// send disconnect msg to all users
							FD_CLR(i, &client_sockets);
							user *to_rmv = find_in_list(client_list, i);
							sprintf(buffer, "server: client %d just left\n", to_rmv->id);
							remove_from_list(&client_list, i);
							if (send_msg_to_all(buffer, client_list))
							{
								free_list(&client_list);
								return 1;
							}
							break;
						}
						default:
						{
							if (send_msg_to_all(buffer, client_list) == 1)
							{
								free_list(&client_list);
								return 1;
							}
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
	struct sockaddr_in addr;
	int socket_fd;

	if (argc != 2) {
		throw_error("Wrong number of arguments\n");
		return 1;
	}
	// Create socket
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1)
	{
		throw_error("Fatal error\n");
		return 1;
	}
	// Init struct that the socket needs
	int opt = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		throw_error("Fatal error\n");
		return 1;	
	}
	addr = init_socket_struct(atoi(argv[1]));
	if (bind(socket_fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		throw_error("Fatal error\n");
		return 0;
	}
	if (listen(socket_fd, FD_SETSIZE) == -1)
	{
		throw_error("Fatal error\n");
		return 0;
	}
	return main_loop(socket_fd);
}