#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <netinet/tcp.h>
#include "test.h"

void setup_conn(char **argv, int &serv_sock, fd_set& fds)
{
	sockaddr_in serv_addr;
	FD_ZERO(&fds);
	serv_sock = socket(AF_INET, SOCK_STREAM, 0); // se creeaza socketul dedicat serverului
	DIE(serv_sock < 0, "socket");

	// se completeaza structura de date pentru socketul dedicat serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	int ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "Invalid address");

	// dezactiveaza algoritmul Nagle
	int opt_val = 1;
	setsockopt(serv_sock, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof(int)); 

	ret = connect(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// se seteaza descriptorii de citire
	FD_SET(STDIN_FILENO, &fds);
	FD_SET(serv_sock, &fds);

	char userID[11] = {0};
	strncpy(userID, argv[1], 10);

	ret = send(serv_sock, userID, USER_ID_LEN, 0);
	DIE(ret < 0, "send");

	std::cerr << "Sent " << ret << " bytes to server\n";
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc < 4) {
		fprintf(stderr, "Usage: %s userID server_address server_port\n", argv[0]);
		return -1;
	}

	int serv_sock;
	fd_set fds;

	setup_conn(argv, serv_sock, fds);

	while (true) {
		fd_set tmp_fds = fds;
		int ret = select(serv_sock + 1, &tmp_fds, NULL, NULL, NULL);

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			std::string input;
			std::getline(std::cin, input);
			input += "\n";

			// se afiseaza mesajul in functie de comanda primita
			if (input.substr(0, 4) == "exit")
				break;
			else if (input.substr(0, 9) == "subscribe")
				std::cout << "Subscribed to topic." << std::endl;
			else if (input.substr(0, 11) == "unsubscribe")
				std::cout << "Unsubscribed from topic." << std::endl;

			// se trimite mesajul la server
			char cmd_buffer[CLI_CMD_LEN] = {0};
			strcpy(cmd_buffer, input.c_str());
			ret = send(serv_sock, cmd_buffer, CLI_CMD_LEN, 0);
			DIE(ret < 0, "send");
		}

		if (FD_ISSET(serv_sock, &tmp_fds)) {
			tcp_msg message;
			memset(&message, 0, sizeof(tcp_msg));
			ret = recv(serv_sock, &message, sizeof(tcp_msg), 0);
			if (ret == 0) {
				break;
			} else {
				// Parseaza mesajul peste tcp
				char out_buffer[MAXRECV + 1];
				sprintf(out_buffer, "%s:%d - %s - %s - %s\n", message.ip,
					message.port, message.topic, message.type, message.payload);

				std::cout << out_buffer;
			}
		}
	}
	// se inchide socketul

	close(serv_sock);
	return 0;
}
