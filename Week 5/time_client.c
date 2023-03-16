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

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(20000);
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

	// Send confirmation to server
	puts("Sending confirmation to server...");
	strcpy(buf, "Got the time!");
	my_send(sockfd, buf, strlen(buf) + 1, 0);
	puts("Confirmation sent to server");

	char str[6000] = "";

	puts("Receiving story from server...");
	my_recv(sockfd, str, 6000, 0);
	puts("Story received from server:");

	FILE * fp = fopen("recv.txt", "w");

	fputs(str, fp);
	fflush(fp);
	fclose(fp);

	my_close(sockfd);
}
