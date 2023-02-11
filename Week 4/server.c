#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE (1 << 10)
#define CHUNK_SIZE (1 << 10)

typedef struct _request
{
	char method[10];
	char path[100];
	char type[100];
} Request;

void net_receive(int sockfd, char *buf, int max_size)
{
	int n = 0;
	while (n < max_size)
	{
		// int r = recv(sockfd, buf + n, max_size - n, 0);
		int r = read(sockfd, buf + n, max_size - n);
		if (r < 0)
		{
			perror("Receive Error!\n");
			close(sockfd);
			exit(EXIT_FAILURE);
		}
		else if (r == 0)
		{
			break;
		}
		n += r;
	}
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

int main()
{
	int fd = open("request.txt", O_RDONLY, 0666);

	char buf[1024];
	int n = read(fd, buf, 1024);
	buf[n] = '\0';
	printf("%s", buf);

	Request req = {0};
	bool flag = parse_header(buf, n, &req);
	printf("%s\n", flag ? "true" : "false");

	// print request
	printf("method: `%s`, path: `%s`, type: `%s`\n",
		   req.method, req.path, req.type);

	// create response

	// read file (send later)
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

	char response[1 << 8];
	sprintf(response, "HTTP/1.1 200 OK\r\n"
					  "Content-Type: %s\r\n"
					  "Content-Length: %d\r\n"
					  "\r\n",
			req.type, length);

	FILE *fp2 = fopen("response.pdf", "wb");
	if (fp2 == NULL)
	{
		perror("Open file failed!\n");
		exit(EXIT_FAILURE);
	}

	fwrite(response, 1, strlen(response), fp2);
	char p[CHUNK_SIZE + 1] = {0};

	int bytes = 0, total = 0;
	while ((bytes = fread(p, 1, CHUNK_SIZE, fp)) > 0)
	{
		printf("OK: %d\n", bytes);
		total += bytes;
		fwrite(p, 1, bytes, fp2);
	}
	*p = '\0';
	fclose(fp);
	fclose(fp2);

	printf("total: %d\n", total);

	// printf("%s", response);
}
