#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXLINE (1 << 10)
#define CHUNKSIZE (1 << 10)
#define BACKLOG 50

typedef struct _request
{
	char method[10];
	char path[100];
	char type[100];
} Request;

int getRequest(int sockfd, char **header)
{
	int nchars;
	int hsize = 0;
	int maxlimit = 256;
	char buff[CHUNKSIZE];
	bool header_received = false;

	char *htemp = malloc((maxlimit + 2) * sizeof(char));

	int cnt = 0;
	while (!header_received)
	{
		puts("Waiting for request...");
		nchars = recv(sockfd, buff, CHUNKSIZE, 0);
		printf("nchars = %d\n", nchars);

		if (nchars <= 0)
		{ // client suddenly disconnects
			printf("Disconnected...\n");
			close(sockfd);
			free(htemp);
			exit(EXIT_FAILURE);
		}

		int k = 0;
		while (k < nchars)
		{
			if (buff[k] == '\r')
			{
				cnt++;
			}
			else if (buff[k] != '\n' &&
					 buff[k] != '\r' &&
					 cnt == 1)
			{
				cnt--;
			}
			htemp[hsize++] = buff[k++];
			if (hsize == maxlimit)
			{ // maxlimit is doubled when about to overflow
				maxlimit *= 2;
				htemp = (char *)realloc(htemp, (maxlimit + 2) * sizeof(char));
			}
			if (cnt == 2)
			{
				htemp[hsize++] = '\n'; // put the \n in the header
				// myparse(htemp, hsize);
				header_received = true;
				// FILE *fptr = fopen("myexample.html", "w");
				// fprintf(fptr, "%.*s", nchars - k - 1, buff + k + 1); // skipping the '\n' in buff[k]
				// total += nchars - k;
				// fclose(fptr);
				break;
			}
		}
		// else
		// {
		// 	FILE *fptr = fopen("myexample.html", "a");
		// 	fprintf(fptr, "%.*s", nchars, buff);
		// 	total += nchars;
		// 	fclose(fptr);
		// 	if (total >= 1256)
		// 		break;
		// }
	}
	htemp[hsize] = '\0';
	*header = htemp;
	return hsize;
}

/**
 * @brief parse http header
 * @param buf buffer
 * @param n length of buf
 * @return true if parse success
 * @return false if parse failed
 */
bool parse_header(char *buf, int n, Request *req)
{
	bool flag = true;
	char line[1024];
	int i = 0, line_no = 0;

	while (i < n)
	{
		int j = 0;
		while (buf[i] != '\r' && buf[i] != '\n')
		{
			line[j++] = buf[i++];
		}
		line[j] = '\0';
		while (buf[i] == '\r' || buf[i] == '\n')
		{
			i++;
		}

		puts("----------");

		if (line_no == 0)
		{
			// parse first line
			char method[10], path[100], version[10];
			sscanf(line, "%s %s %s", method, path, version);

			printf("method: %s\n", method);
			printf("path: %s\n", path);
			printf("version: %s\n", version);

			strcpy(req->method, method);
			strcpy(req->path, path);

			// if (strcmp(method, "GET") != 0)
			// {
			// 	flag = false;
			// 	break;
			// }
			// if (strcmp(version, "HTTP/1.1") != 0)
			// {
			// 	flag = false;
			// 	break;
			// }
		}
		else
		{
			// parse other lines
			char key[100], value[100];
			sscanf(line, "%[^:]: %[^\n]", key, value);

			printf("key: %s\n", key);
			printf("value: %s\n", value);

			if (strcmp(key, "Content-Type") == 0 ||
				strcmp(key, "Accept") == 0)
			{
				strcpy(req->type, value);
			}

			// if (strcmp(key, "Host") != 0)
			// {
			// 	flag = false;
			// 	break;
			// }
		}

		++line_no;
	}

	return flag;
}

void getFileLastModified(char *path, char *date)
{
	struct stat buf;
	stat(path, &buf);
	struct tm *tm = gmtime(&buf.st_mtime);
	strftime(date, 100, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

int main()
{
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Cannot create socket!\n");
		return EXIT_FAILURE;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(20000);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Unable to bind local address!\n");
		return EXIT_FAILURE;
	}

	listen(sockfd, BACKLOG);

	while (true)
	{
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		puts("Connected");

		if (newsockfd < 0)
		{
			perror("Accept Error!\n");
			return EXIT_FAILURE;
		}

		if (fork() == 0)
		{
			close(sockfd);

			// Do stuff
			puts("Doing stuff");
			char *header;
			int hsize = getRequest(newsockfd, &header);
			printf("Request: %s\n", header);

			Request req;
			if (parse_header(header, hsize, &req))
			{
				printf("Method: %s\n", req.method);
				printf("Path: %s\n", req.path);
				printf("Type: %s\n", req.type);
			}
			else
			{
				puts("Parse failed");
			}

			if (strcmp(req.method, "GET") == 0)
			{
				FILE *fp = fopen(req.path, "rb");
				if (fp == NULL)
				{
					perror("Open file failed!\n");
					exit(EXIT_FAILURE);
				}

				// length of file
				fseek(fp, 0, SEEK_END);
				int length = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				char response[1 << 10];
				char date[100];
				getFileLastModified(req.path, date);
				sprintf(response, "HTTP/1.1 200 OK\r\n"
								  "Content-Type: %s\r\n"
								  "Content-Length: %d\r\n"
								  "Cache-Control: no-store\r\n"
								  "Content-Language: en-us\r\n"
								  "Last-Modified: %s\r\n"
								  "\r\n",
						req.type, length, date);
				
				puts(response);

				// fwrite(response, 1, strlen(response), fp2);
				send(newsockfd, response, strlen(response), 0);
				char p[CHUNKSIZE + 1] = {0};

				int bytes = 0, total = 0;
				while ((bytes = fread(p, 1, CHUNKSIZE, fp)) > 0)
				{
					printf("OK: %d\n", bytes);
					total += bytes;
					send(newsockfd, p, bytes, 0);
					// fwrite(p, 1, bytes, fp2);
				}
				fclose(fp);
			}

			close(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}
