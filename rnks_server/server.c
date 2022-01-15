#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "package.h"

#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable: 4996)

#define NO_SOCKET -1

int username_handler(char* input) {
	/*
	* Validating the Input
	* Returns true if input is exactly "s" or "S" followed by 5 digits ranging from [0-9].
	* Returns false if otherwise.
	*/
	return
		strlen(input) == 6 &&
		(input[0] == 's' ||
			input[0] == 'S') &&
		isdigit(input[1]) != 0 &&
		isdigit(input[2]) != 0 &&
		isdigit(input[3]) != 0 &&
		isdigit(input[4]) != 0 &&
		isdigit(input[5]) != 0;
}

int accept_client_handler(SOCKET target_socket, package* data) {
	int client_socket = NO_SOCKET;
	char client_num[8];
	struct sockaddr_in6 client_addr_IPv6;
	int addr_size = sizeof(client_addr_IPv6);

	client_socket = (int)accept(target_socket, (struct sockaddr*)&client_addr_IPv6, &addr_size);
	if (client_socket < 0) {
		printf("[-] Server accept failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("[+] Client joined the chat.\n");

	return client_socket;
}


int recv_msg_handler(SOCKET target_socket) {
	package receiver;
	ZeroMemory(receiver.buffer, sizeof(receiver.buffer));

	int ret = recv(target_socket, receiver.buffer, sizeof(receiver.buffer), 0);
	if (ret > 0) {
		printf("%s\n", receiver.buffer);
	}
	else {
		return 1;
	}

	return 0;
}

int send_msg_handler(SOCKET target_socket, package* data) {
	if (_kbhit()) {
		char c = _getch();
		if (c == '\r') {
			data->message[data->pt++] = '\0';
			if (strlen(data->message) > 0) {
				if (strcmp(data->message, "/exit") == 0) {
					printf("\n[-] You left the chat\n");
					return 1;
				}
				else {
					ZeroMemory(data->buffer, sizeof(data->buffer));
					sprintf(data->buffer, "[%s] %s", data->s_num, data->message);
					send(target_socket, data->buffer, sizeof(data->buffer), 0);
					data->pt = 0;
					ZeroMemory(data->message, sizeof(data->message));
					printf("\n");
				}
			}
		}
		else if (data->pt < sizeof(data->message) - 1) {
			data->message[data->pt++] = c;
			printf("\33[2K\r[%s] %s", data->s_num, data->message);
		}
	}
	return 0;
}

void main(int argc, char** argv) {
	struct sockaddr_in6 server_addr_IPv6;
	SOCKET server_socket = NO_SOCKET, client_socket = NO_SOCKET;
	fd_set read_socket, write_socket, except_socket;
	package data;
	//char IP_IPv6[40] = "2a02:2454:9d26:9e00:c0d1:233e:bcb9:b04e";
	int Port_IPv6 = 9000;

	if (!argv[1] || !argv[2]) {
		printf("[-] Not enough parameter.\n");
		exit(EXIT_FAILURE);
	}

	if (!username_handler(argv[1])) {
		printf("Username invalid.\n");
		exit(EXIT_FAILURE);
	}
	strcpy(data.s_num, argv[1]);
	printf("Open for: %s\n", data.s_num);

	if (!(Port_IPv6 = strtol(argv[2], NULL, 10))) {
		printf("Port invalid\n");
		exit(EXIT_FAILURE);
	}
	printf("Port: %d\n\n", Port_IPv6);

	WSADATA winsock;
	if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
		printf("[-] Startup failed. Error Code : %d.\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	server_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET) {
		printf("[-] Socket creation failed...\n");
		exit(EXIT_FAILURE);
	}
	printf("[+] Socket successfully created.\n");

	u_long iMode = 1;
	if (ioctlsocket(server_socket, FIONBIO, &iMode) != NO_ERROR) {
		printf("[-] Ioctlsocket failed.\n");
	}

	ZeroMemory(&server_addr_IPv6, sizeof(server_addr_IPv6));
	server_addr_IPv6.sin6_family = AF_INET6;
	server_addr_IPv6.sin6_port = htons(Port_IPv6);
	inet_pton(AF_INET6, INADDR_ANY, server_addr_IPv6.sin6_addr.s6_addr);

	if (bind(server_socket, (struct sockaddr*)&server_addr_IPv6, sizeof(server_addr_IPv6)) == SOCKET_ERROR) {
		printf("[-] Socket bind failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("[+] Socket successfully binded.\n");

	if ((listen(server_socket, 5)) != 0) {
		printf("[-] Listen failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("[+] Server listening...\n\n");

	ZeroMemory(data.message, sizeof(data.message));
	data.pt = 0;

	while (1) {
		//select is destructive -> reassign sockets
		FD_ZERO(&read_socket);
		FD_SET(server_socket, &read_socket);
		if (client_socket != NO_SOCKET)
			FD_SET(client_socket, &read_socket);

		FD_ZERO(&write_socket);
		if (client_socket != NO_SOCKET)
			FD_SET(client_socket, &write_socket);

		FD_ZERO(&except_socket);
		FD_SET(server_socket, &except_socket);
		if (client_socket != NO_SOCKET)
			FD_SET(client_socket, &except_socket);

		int highest_socket = (int)server_socket * (server_socket > client_socket) + client_socket * (server_socket < client_socket);

		int return_select = select(highest_socket, &read_socket, &write_socket, &except_socket, NULL);
		switch (return_select) {
		case -1:
			printf("[-] Select failed.");
			exit(EXIT_FAILURE);

		case 0:
			printf("[-] Select timed out.");
			exit(EXIT_FAILURE);

		default:
			if (FD_ISSET(server_socket, &read_socket)) {
				client_socket = accept_client_handler(server_socket, &data);
			}

			if (FD_ISSET(server_socket, &except_socket)) {
				printf("[-] Except for Server");
				shutdown(server_socket, SD_BOTH);
				exit(EXIT_SUCCESS);
			}

			/*
			* checks for sent messages from the client.
			* in case the client send a message, the message will be printed directly onto the console.
			* if the server is unable to receive the message to its client, the return value will be one,
			* in that case the server will gracefully terminate its connection to the client.
			*/
			if (client_socket != NO_SOCKET && FD_ISSET(client_socket, &read_socket)) {
				if (recv_msg_handler(client_socket) != 0) {
					printf("[-] Client left the chat.\n");
					shutdown(client_socket, SD_BOTH);
					client_socket = NO_SOCKET;
				}
			}

			/*
			* allows for input of single characters during each loop.
			* appends each character to data.message for a max of 1023 characters.
			* empty strings cant be sent.
			* if the server is unable to send the message to its client, the return value will be one,
			* in that case the server will gracefully terminate its connection to the client.
			*/
			if (client_socket != NO_SOCKET && FD_ISSET(client_socket, &write_socket)) {
				if (send_msg_handler(client_socket, &data) != 0) {
					printf("[-] Client left the chat.\n");
					shutdown(client_socket, SD_BOTH);
					client_socket = NO_SOCKET;
				}
			}

			/*
			* In case socket receives out of band data the connection between server and client gracefully terminates.
			*/
			if (client_socket != NO_SOCKET && FD_ISSET(client_socket, &except_socket)) {
				printf("[-] Exception for client_socket.\n");
				shutdown(client_socket, SD_BOTH);
				client_socket = NO_SOCKET;
			}
		}
	}
}

/*ToDo's Aufgabenstellung
* Empfänger:
* [x] Soll Daten senden können
* [x] Soll Daten empfangen können
* [x] Soll argv für sNummer des Senders erhalten können
* [x] Soll argv für Port erhalten können
* [x] Soll sNummer überprüfen können
* [x] Initialisieren des Empfänger TCP-Sockets möglich
* [x] bind() verwenden
* [x] Aufruf von select()
* [x] Rückgabewert von select() prüfen
* [x] Einlesen von Text via Konsole
* [x] Paket erstellen (in diesem Fall nur Nachricht da keine s-Num für Empfänger gefordert war)
* [x] Nachricht maximal 1024 Zeichen -> 1023 lesbare + 1 '\0' Zeichen gegeben.
* [x] Empfangen von Text via recv()
* [x] Ausgabe der sNummer + Text in der Konsole
* [x] Keine weiteren Threads
* [x] Asynchrones Eingeben & Empfangen
*
* Sender:
* [x] Soll Daten senden können
* [x] Soll Daten empfangen können
* [x] Soll argv für sNummer erhalten können
* [x] Soll argv für IPv6 Addresse des Servers erhalten können
* [x] Soll argv für Port erhalten können
* [x] Initialisieren des Sender TCP-Sockets möglich
* [x] Aufruf von select()
* [x] Rückgabewert von select() prüfen
* [x] Einlesen von Text via Konsole
* [x] Paket erstellen
* [x] Nachricht maximal 1024 Zeichen -> 1023 lesbare + 1 '\0' Zeichen gegeben.
* [x] Empfangen von Text via recv()
* [x] Ausgabe der sNummer + Text in der Konsole
* [x] Keine weiteren Threads
* [x] Asynchrones Eingeben & Empfangen
*
* Gesamtbewertung:
* [x] Parameterübergabe (Empf.: [sNummer v. KP | Port]) & (Sender: [sNummer | IPv6 Addresse | Port])
* [x] Konnektivität zwischen Empf. und Sender
* [x] Senden & Empfangen von Nachrichten zwischen Empf. und Sender
* [x] Paket mit sNummer + Nachricht (nur von Clientseite, weil bei Server nicht gefordert gewesen)
* [x] in beide Richtungen
* [?] entfernte Kommunikation (Theoretisch gegeben, vermutlich aber durch Firewall blockiert worden)
* [?] Arbeit mit nichtblockierenden Socketfunktionen (Ja wenn nur in select()-befindliche Socketfunktionen gemeint sind, sonst blockiert noch accept() und connect()
* [x] Arbeit mit select()
* [?] nichtblockierende Konsolen-Anwendung (Ja, sobald die Verbindung erfolgt ist)
* [x] sauber strukturierter Code (würd ich ja mal meinen)
* [ ] sauber dokumentierter Code
* [?] Alle Fragen beantworten können
*/