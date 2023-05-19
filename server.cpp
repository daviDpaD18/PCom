#include <netinet/tcp.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <math.h>
#include <poll.h>
#include "helpers.h"
#include "test.h"

#define CONSOLE_READ_BUFFER 512

void setup_socks(uint16_t port, int &socket_tcp, int &socket_udp, fd_set &fds) {
	FD_ZERO(&fds);
	
	socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
	socket_udp = socket(PF_INET, SOCK_DGRAM, 0);

	DIE(socket_udp < 0 || socket_tcp < 0, "socket");

	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	int ret;
	ret = ::bind(socket_tcp, (sockaddr *)&server_addr, sizeof(sockaddr));
	DIE(ret < 0, "bind");
	ret = ::bind(socket_udp, (sockaddr *)&server_addr, sizeof(sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(socket_tcp, MAX_CLIENTS_QUEUED);
	DIE(ret < 0, "listen");

	FD_SET(socket_tcp, &fds);
	FD_SET(socket_udp, &fds);
	FD_SET(STDIN_FILENO, &fds);
}

std::vector<subscriber>::iterator
find_user_by_sock(std::vector<subscriber> &subs, int sock)
{
	auto it = subs.begin();
	for (; it != subs.end(); ++it)
		if (it->socket == sock)
			break;
	return it;
}

std::vector<subscriber>::iterator
find_user_by_id(std::vector<subscriber> &subs, std::string id)
{
	auto it = subs.begin();
	for (; it != subs.end(); ++it)
		if (it->id == id)
			break;
	return it;
}

int handle_conn(int socket_tcp, std::vector<subscriber> &subs)
{
	// cerere de conexiune pe socketul cu listen, accepta
	socklen_t cli_addr_len = sizeof(sockaddr_in);
	sockaddr_in cli_addr;
	memset(&cli_addr, 0, sizeof(cli_addr));

	int newsocket = accept(socket_tcp, (struct sockaddr *)&cli_addr, &cli_addr_len);
	DIE(newsocket < 0, "accept");

	// dezactivarea algoritmului Nagle
	int opt_val = 1;
	setsockopt(newsocket, IPPROTO_TCP, TCP_NODELAY, &opt_val, sizeof(int));

	char uid_buffer[USER_ID_LEN + 1] = {0};
	int ret = recv(newsocket, uid_buffer, USER_ID_LEN, 0);
	DIE(ret < 0, "recv");
	if (ret == 0) {
		close(newsocket);
		return -1;
	}

	std::string user_id(uid_buffer);

	auto sub_iter = find_user_by_id(subs, user_id);

	// verific daca clientul este nou
	if (sub_iter == subs.end()) {
		subscriber newsub;
		newsub.id = user_id;
		newsub.socket = newsocket;
		newsub.connect = 1;
		subs.push_back(newsub);
		std::cerr << "Added " << newsub.id << " to logs\n";
		
		printf("New client %s connected from %s:%d.\n", uid_buffer,
			inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	} else if ((*sub_iter).connect == 1) { // verific daca exista subscriberul si este conectat
		std::cout << "Client " << user_id << " already connected.\n";
		close(newsocket);
		return -1;
	} else { // daca este deconectat, se reconecteaza
		printf("New client %s connected from %s:%d.\n", uid_buffer,
			inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		sub_iter->connect = 1;
		sub_iter->socket = newsocket;

		char oldMessage[MAXRECV];
		memset(oldMessage, 0, MAXRECV);
		// pentru clientii reconectati cu sf-ul 1 se trimit mesajele
		// stranse
		for (auto &subber : subs) {
			for (auto &subbed_topic : subber.topics) {
				if (!subbed_topic.sf)
					continue;

				for (auto &missed_msg : subbed_topic.mess)
					send(subber.socket, &missed_msg, sizeof(missed_msg), 0);

				subbed_topic.mess.clear();
			}
		}
	}

	return newsocket;
}

void parse_udp_message(int socket_udp, tcp_msg &client_msg)
{
	memset(&client_msg, 0, sizeof(client_msg));
	constexpr int MAX_UDP_MSG_LEN = 1551;
	uint8_t udp_message[MAX_UDP_MSG_LEN] = {0};

	socklen_t addr_len = sizeof(sockaddr_in);
	sockaddr_in udp_addr = {0};
	int ret = recvfrom(socket_udp, udp_message, MAX_UDP_MSG_LEN, 0,
					   (sockaddr *)&udp_addr, &addr_len);

	// parseaza adresa clientului udp
	strncpy(client_msg.ip, inet_ntoa(udp_addr.sin_addr), sizeof(client_msg.ip));
	client_msg.port = udp_addr.sin_port;
	memcpy(client_msg.topic, udp_message, sizeof(client_msg.topic));
	client_msg.topic[50] = 0;
	client_msg.type1 = udp_message[50];

	uint8_t *udp_payload = &udp_message[51];
	// scrie tipul de date in mesajul tcp

	if (client_msg.type1 == 0) {	// Verifica daca tipul e INT
		long parsed_payload = 0;

		strcpy(client_msg.type, "INT");
		parsed_payload = (long)ntohl(*(uint32_t *)&udp_payload[1]);
		if (udp_payload[0] == 1)
			parsed_payload = -parsed_payload;

		snprintf(client_msg.payload, sizeof(client_msg.payload), "%ld", parsed_payload);
	} else if (client_msg.type1 == 1) {	// Verifica daca e SHORT_REAL
		float parsed_payload = (float)ntohs(*(uint16_t *)udp_payload) / 100.0;
		strcpy(client_msg.type, "SHORT_REAL");

		snprintf(client_msg.payload, sizeof(client_msg.payload), "%.2f", parsed_payload);
	} else if (client_msg.type1 == 2) {
		double finalFloat = ((double)ntohl(*(uint32_t*)&udp_payload[1]))
								/ pow(10.0, udp_payload[5]);
		strcpy(client_msg.type, "FLOAT");
		if (udp_payload[0] == 1)
			finalFloat = -finalFloat;

		snprintf(client_msg.payload, sizeof(client_msg.payload), "%lf", finalFloat);
	} else {
		strcpy(client_msg.type, "STRING");
		memcpy(client_msg.payload, udp_payload, sizeof(client_msg.payload));
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	uint16_t port;
	
	if (argc < 2) {
		fprintf(stderr, "Usage: %s server_port\n", argv[0]);
		return -1;
	} else if ((port = atoi(argv[1])) == 0) {
		fprintf(stderr, "Invalid port!\n");
		return -1;
	}

	int socket_tcp, socket_udp;
	fd_set active_conns;
	setup_socks(port, socket_tcp, socket_udp, active_conns);

	int maxfd = std::max(socket_tcp, socket_udp);

	int ret;
	std::vector<subscriber> subs;

	while (true) {
		std::cerr << "User log count: " << (int)subs.size() << "\n";
		// Verifica pe ce socketi sunt valabile date
		fd_set tmp_fdset = active_conns;
		ret = select(maxfd + 1, &tmp_fdset, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fdset)) {
			char cmd_buf[CONSOLE_READ_BUFFER] = {0};
			memset(cmd_buf, 0, CONSOLE_READ_BUFFER);
			ret = read(STDIN_FILENO, cmd_buf, CONSOLE_READ_BUFFER);
			DIE(ret < 0, "read");

			if (strncmp(cmd_buf, "exit\n", 5) == 0)
				break;
			else
				fputs("Unknown command\n", stderr);
		}

		if (FD_ISSET(socket_tcp, &tmp_fdset)) {
			int newsocket = handle_conn(socket_tcp, subs);
			if (newsocket == -1)
				continue;	// Autentificarea a esuat

			// se adauga noul socket intors de accept() la multimea descriptorilor
			FD_SET(newsocket, &active_conns);
			maxfd = std::max(newsocket, maxfd);
		}

		if (FD_ISSET(socket_udp, &tmp_fdset)) {
			struct tcp_msg client_msg;
			parse_udp_message(socket_udp, client_msg);
			std::cerr << "Parsed message on topic " << client_msg.topic << "\n";
			
			// se cauta clientii abonati la topic
			for (auto& sub : subs){
				for (auto& topic : sub.topics){
					std::cerr << "Check topic " << topic.name << " of user " << sub.id << "\n";
					if (strcmp(topic.name, client_msg.topic) != 0) {
						continue;	// Topicurile nu coincid
					} else if (sub.connect == 1) {
						std::cerr << "Sending\n";
						ret = send(sub.socket, &client_msg, sizeof(tcp_msg), 0);
						std::cerr << "Sent: " << ret << "\n";
						break;		// Se trimite mesajul la clientul curent
					} else if (topic.sf) {
					// pentru cei abonati, dar deconectati si cu sf-ul 1, se memoreaza mesajele
					topic.mess.push_back(client_msg);
					}
				}	
			}
		}
		
		for (int i = 1; i <= maxfd; i++) {
			if (i == socket_tcp || i == socket_udp)
				continue;

			if (!FD_ISSET(i, &tmp_fdset))
				continue;
			// s-au primit date pe unul din socketii de client,
			// asa ca serverul trebuie sa le receptioneze
			char client_cmd[CLI_CMD_LEN] = {0};

			ret = recv(i, client_cmd, CLI_CMD_LEN, 0);

			if (ret <= 0) { // conexiunea s-a inchis
				// se cauta subscriberul in lista
				auto &client = *find_user_by_sock(subs, i);
				printf("Client %s disconnected.\n", client.id.c_str());

				close(i);
				FD_CLR(i, &active_conns);
				client.connect = 0;

				continue;
			} 

			if (strncmp(client_cmd, "subscribe", strlen("subscribe")) == 0) {
				char *topic_start = 1 + strchr(client_cmd, ' ');
				char *topic_end = strchr(topic_start, ' ');
				*topic_end = '\0';

				topic newTopic;
				strcpy(newTopic.name, topic_start);
				// Sf-ul va fi urmatorul caracter dupa spatiu
				newTopic.sf = (int)topic_end[1] - '0';

				auto user_it = find_user_by_sock(subs, i);
				user_it->topics.push_back(newTopic);
				
				std::cerr << "Recvd subscribe to " << newTopic.name << "with sf=" << newTopic.sf << "\n";
				std::cerr << "Topics for user " << user_it->id << ": " << (int)user_it->topics.size() << "\n";
			} else if (strncmp(client_cmd, "unsubscribe", strlen("unsubscribe")) == 0) {
				// se extrage numele topicului de la care s-a deconectat

				char *topic_start = 1 + strchr(client_cmd, ' ');
				char *topic_end = strchr(topic_start, ' ');
				*topic_end = '\0';

				auto &topics = find_user_by_sock(subs, i)->topics;
				for (auto it = topics.begin(); it != topics.end(); ++it) {
					if (strcmp(it->name, topic_start) == 0) {
						topics.erase(it);
						break;
					}
				}
			}
		}
	}

	for (int open_sock = 1; open_sock <= maxfd; open_sock++)
		if (FD_ISSET(open_sock, &active_conns))
			close(open_sock);

	return 0;
}
