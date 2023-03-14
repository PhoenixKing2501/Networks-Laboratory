/*
 ************************************************
 *                                              *
 * Group   : 42                                 *
 * Member 1: Utsav Basu (20CS30057)             *
 * Member 2: Anamitra Mukhopadhyay (20CS30064)  *
 *                                              *
 ************************************************
 */

#define _GNU_SOURCE

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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAXLINE (1 << 10)
#define CHUNKSIZE (1 << 10)
#define BACKLOG 50
#define SERV_PORT 20000

/**
 * @brief @Request struct
 * @param method method of request
 * @param path path of file
 * @param type type of file
 * @param length length of file
 * @note type is only used for GET request
 * @note length is only used for PUT request
 */
typedef struct _request
{
	char method[10];
	char path[100];
	char type[100];
	char time[100];
	int length;
} Request;

char *getDateNow()
{
	static char date[100];
	time_t now = time(NULL);
	struct tm *tm = gmtime(&now);
	strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);
	return date;
}

bool sendResponseHeader(int sockfd, char *raw_msg)
{
	char msg[1 << 11];
	sprintf(msg, raw_msg, getDateNow());
	printf("Sending response header...\n%s\n", msg);
	return send(sockfd, msg, strlen(msg), 0) >= 0;
}

/**
 * @brief parse http header
 * @param buf buffer
 * @param n length of buf
 * @param req request
 * @param req_time request time
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

		if (line_no == 0)
		{
			// parse first line
			char method[10], path[100], version[10];
			sscanf(line, "%s %s %[^\n]", method, path, version);

			strcpy(req->method, method);
			strcpy(req->path, path);

			if (strcmp(method, "GET") != 0 &&
				strcmp(method, "PUT") != 0)
			{
				flag = false;
				break;
			}
			if (strcmp(version, "HTTP/1.1") != 0)
			{
				flag = false;
				break;
			}
		}
		else
		{
			// parse other lines
			char key[100] = {0}, value[100] = {0};
			sscanf(line, "%[^:]: %[^\n]", key, value);

			if (*value == '\0')
			{
				flag = false;
				break;
			}

			if (strcmp(key, "Content-Type") == 0 ||
				strcmp(key, "Accept") == 0)
			{
				strcpy(req->type, value);
			}

			if (strcmp(key, "Content-Length") == 0)
			{
				req->length = atoi(value);
			}

			if (strcmp(key, "If-Modified-Since") == 0)
			{
				strcpy(req->time, value);
			}
		}

		++line_no;
	}

	return flag;
}

/**
 * @brief get request from client
 * @param sockfd socket file descriptor
 * @param req request
 * @return true if get request success
 * @return false if get request failed
 * @note if the request is PUT, the file will be written to disk
 * @note if the request is GET, the file will be sent to client
 */
bool getRequest(int sockfd, Request *req)
{
	int nchars;
	int hsize = 0;
	int maxlimit = 256;
	char buff[CHUNKSIZE];
	bool header_received = false;

	char *htemp = malloc((maxlimit + 2) * sizeof(char));

	int cnt = 0;
	int total = 0;
	while (true)
	{
		nchars = recv(sockfd, buff, CHUNKSIZE, 0);

		if (nchars <= 0)
		{ // client suddenly disconnects
			puts("Disconnected...");
			close(sockfd);
			free(htemp);
			exit(EXIT_FAILURE);
		}

		if (!header_received)
		{
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
					htemp = realloc(htemp, (maxlimit + 2) * sizeof(char));
				}
				if (cnt == 2)
				{
					htemp[hsize++] = '\n'; // put the \n in the header
					header_received = true;

					printf("Header received...\n%.*s\n", hsize, htemp);

					if (!parse_header(htemp, hsize, req))
					{
						printf("Invalid request...\n");
						free(htemp);
						return false;
					}

					if (strcmp(req->method, "GET") == 0)
					{
						// Check permissions
						struct stat st;
						if (stat(req->path, &st) < 0)
						{
							char response[] = "HTTP/1.1 404 Not Found\r\n"
											  "Connection: close\r\n"
											  "Date: %s\r\n"
											  "\r\n";
							sendResponseHeader(sockfd, response);
							close(sockfd);
							free(htemp);
							exit(EXIT_FAILURE);
						}

						if (!(st.st_mode & S_IREAD))
						{
							char response[] = "HTTP/1.1 403 Forbidden\r\n"
											  "Connection: close\r\n"
											  "Date: %s\r\n"
											  "\r\n";
							sendResponseHeader(sockfd, response);
							close(sockfd);
							free(htemp);
							exit(EXIT_FAILURE);
						}

						// Get modified time of file
						if (req->time[0] != '\0')
						{
							time_t file_time = timegm(gmtime(&st.st_mtime));

							struct tm tm;
							strptime(req->time, "%a, %d %b %Y %H:%M:%S %Z", &tm);
							time_t req_time = timegm(&tm);
							if (req_time >= file_time)
							{
								char response[] = "HTTP/1.1 304 Not Modified\r\n"
												  "Connection: close\r\n"
												  "Date: %s\r\n"
												  "\r\n";
								sendResponseHeader(sockfd, response);
								close(sockfd);
								free(htemp);
								exit(EXIT_FAILURE);
							}
						}

						free(htemp);
						return true;
					}
					else if (strcmp(req->method, "PUT") == 0)
					{
						FILE *fptr = fopen(req->path, "wb");
						if (fptr == NULL)
						{
							char response[] = "HTTP/1.1 403 Forbidden\r\n"
											  "Connection: close\r\n"
											  "Date: %s\r\n"
											  "\r\n";
							sendResponseHeader(sockfd, response);
							close(sockfd);
							free(htemp);
							exit(EXIT_FAILURE);
						}

						fwrite(buff + k + 1, sizeof(char), nchars - k - 1, fptr);
						total += nchars - k - 1;
						fclose(fptr);
					}

					break;
				}
			}
		}
		else
		{
			FILE *fptr = fopen(req->path, "ab");
			fwrite(buff, sizeof(char), nchars, fptr);
			total += nchars;
			fclose(fptr);
			if (total >= req->length)
				break;
		}
	}

	free(htemp);
	return true;
}

void getFileLastModified(char *path, char *date)
{
	struct stat buf;
	stat(path, &buf);
	struct tm *tm = gmtime(&buf.st_mtime);
	strftime(date, 100, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

void DatePlusDays(struct tm *date, int days)
{
	const time_t ONE_DAY = 24 * 60 * 60;
	// Seconds since start of epoch
	time_t date_seconds = mktime(date) + (days * ONE_DAY);
	*date = *gmtime(&date_seconds);
}

void logRequest(const Request *req, struct sockaddr_in *cli_addr)
{
	char date[100];
	time_t now = time(NULL);
	struct tm *tm = gmtime(&now);
	strftime(date, sizeof(date), "%d-%m-%y:%H-%M-%S", tm);

	FILE *fptr = fopen("AccessLog.txt", "a");
	fprintf(fptr, "%s:%s:%d:%s:%s\n",
			date,
			inet_ntoa(cli_addr->sin_addr),
			ntohs(cli_addr->sin_port),
			req->method,
			req->path);
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
	serv_addr.sin_port = htons(SERV_PORT);

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

			Request req = {0};
			bool valid = getRequest(newsockfd, &req);

			if (!valid)
			{
				char response[] = "HTTP/1.1 400 Bad Request\r\n"
								  "Connection: close\r\n"
								  "Date: %s\r\n"
								  "\r\n";
				sendResponseHeader(newsockfd, response);
				close(newsockfd);
				exit(EXIT_FAILURE);
			}

			if (strcmp(req.method, "GET") == 0)
			{
				FILE *fp = fopen(req.path, "rb");
				if (fp == NULL)
				{
					perror("Open file failed!\n");
					char response[] = "HTTP/1.1 404 Not Found\r\n"
									  "Connection: close\r\n"
									  "Date: %s\r\n"
									  "\r\n";
					sendResponseHeader(newsockfd, response);
					exit(EXIT_FAILURE);
				}

				// length of file
				fseek(fp, 0, SEEK_END);
				long length = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				char response[1 << 10];
				char date[100];
				getFileLastModified(req.path, date);
				sprintf(response, "HTTP/1.1 200 OK\r\n"
								  "Content-Type: %s\r\n"
								  "Content-Length: %ld\r\n"
								  "Cache-Control: no-store\r\n"
								  "Content-Language: en-us\r\n"
								  "Connection: close\r\n"
								  "Last-Modified: %s\r\n"
								  "Date: %%s\r\n"
								  "\r\n",
						req.type, length, date);

				if (!sendResponseHeader(newsockfd, response))
				{
					perror("Send Error!\n");
					fclose(fp);
					goto disconnect;
				}

				char p[CHUNKSIZE + 1] = {0};

				int bytes = 0, total = 0;
				while ((bytes = fread(p, 1, CHUNKSIZE, fp)) > 0)
				{
					total += bytes;
					if (send(newsockfd, p, bytes, 0) < 0)
					{
						perror("Send Error!\n");
						fclose(fp);
						goto disconnect;
					}
				}
				fclose(fp);
			}
			else if (strcmp(req.method, "PUT") == 0)
			{
				char gtime[100];
				time_t tim;
				time(&tim);
				struct tm t = *gmtime(&tim);
				DatePlusDays(&t, +3);
				strftime(gtime, sizeof(gtime),
						 "%a, %d %b %Y %H:%M:%S GMT", &t);

				char response[1 << 10];
				sprintf(response, "HTTP/1.1 200 OK\r\n"
								  "Cache-Control: no-store\r\n"
								  "Connection: close\r\n"
								  "Expires: %s\r\n"
								  "Date: %%s\r\n"
								  "\r\n",
						gtime);

				if (!sendResponseHeader(newsockfd, response))
				{
					perror("Send Error!\n");
					goto disconnect;
				}
			}
		disconnect:
			logRequest(&req, &cli_addr);

			close(newsockfd);
			exit(0);
		}
		close(newsockfd);
	}
}
