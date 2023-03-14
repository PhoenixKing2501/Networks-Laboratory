#include "mysocket.h"

#define MSG_SIZE 100

int main()
{
	int sockfd;
	struct sockaddr_in serv_addr;

	char buf[MSG_SIZE] = "";

	if ((sockfd = my_socket(AF_INET, SOCK_MyTCP, 0)) < 0)
	{
		perror("Unable to create socket!\n");
		return EXIT_FAILURE;
	}

	// // bind client to a port
	// cli_addr.sin_family = AF_INET;
	// cli_addr.sin_addr.s_addr = INADDR_ANY;
	// cli_addr.sin_port = htons(30000);

	// if (bind(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
	// {
	// 	perror("Unable to bind local address!\n");
	// 	return EXIT_FAILURE;
	// }

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(20001);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

	puts("Connecting to server...");

	if ((my_connect(sockfd, (struct sockaddr *)&serv_addr, sizeof serv_addr)) < 0)
	{
		perror("Unable to connect to server\n");
		return EXIT_FAILURE;
	}


	// Receive the time sent from server
	puts("Receiving time from server...");
	my_recv(sockfd, buf, MSG_SIZE, 0);
	puts("Time received from server:");
	puts(buf);

	my_close(sockfd);
}
