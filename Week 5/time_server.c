#include "mysocket.h"

#define MSG_SIZE 100

/*
	Function to get the time and store in buf
*/
void get_time(char *buf)
{
	time_t result = time(NULL);
	strftime(buf, MSG_SIZE, "%c", localtime(&result));
}

int main()
{
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;

	char buf[MSG_SIZE] = "";

	if ((sockfd = my_socket(AF_INET, SOCK_MyTCP, 0)) < 0)
	{
		perror("Cannot create socket!\n");
		return EXIT_FAILURE;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(20001);

	if (my_bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Unable to bind local address!\n");
		return EXIT_FAILURE;
	}

	my_listen(sockfd, 5);

	while (true)
	{
		puts("Waiting for connection...");
		clilen = sizeof(cli_addr);
		newsockfd = my_accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		puts("Connection accepted");

		// Print connection details
		printf("Connection from %s, port %d\n",
			   inet_ntoa(cli_addr.sin_addr),
			   ntohs(cli_addr.sin_port));

		if (newsockfd < 0)
		{
			perror("Accept Error!\n");
			return EXIT_FAILURE;
		}

		get_time(buf);

		// Send time to client
		puts("Sending time to client...:");
		puts(buf);
		my_send(newsockfd, buf, strlen(buf) + 1, 0);
		puts("Time sent to client");

		my_close(newsockfd);
	}
}
