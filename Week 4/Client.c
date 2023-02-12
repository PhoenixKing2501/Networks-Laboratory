
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>


#define CHUNKSIZE (1<<10)


void openapp(char *filename, int len) {
	char cmd[30];

	if(strcmp(filename + len - 4, ".pdf") == 0) {
		strcpy(cmd, "evince");
	}
	else if(strcmp(filename + len - 4, ".jpg") == 0) {
		strcpy(cmd, "eog");
	}
	else if(strcmp(filename + len - 5, ".html") == 0) {
		strcpy(cmd, "firefox");
	}
	else {
		strcpy(cmd, "gedit");
	}
	
	char *args[] = {cmd, filename, NULL};

	int pid = fork();
    if(pid == 0) {   // child
        if(execvp(args[0], args) == -1)
		{
            perror("exec:error");
        }
        exit(EXIT_SUCCESS);
    }
    if(pid < 0) {
        perror("fork:error");
        exit(EXIT_FAILURE);
    }
}

int parse_header(char *buf, int n, int *status)
{
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
			char version[20], msg[100];
			sscanf(line, "%s %d %s", version, status, msg);

		}
		else
		{
			// parse other lines
			char key[100], value[100];
			sscanf(line, "%[^:]: %[^\n]", key, value);

			if (strcmp(key, "Content-Length") == 0)
			{
				return atoi(value);
			}

		}

		++line_no;
	}

	return -1;
}


int getresponse(int sockfd, char *filename, char **header) {
	int nchars;
	int hsize = 0;
	int maxlimit = 256;
	char buff[CHUNKSIZE];
	bool header_received = false;


	char *htemp = (char *)malloc((maxlimit+2)*sizeof(char));

	int cnt = 0;
	int total = 0;
	int content_len = 0;
	int status = 0;

	struct pollfd fdset[1]={{.fd=sockfd, .events=POLLIN}};
	int ret;

	while(1) {
		ret = poll(fdset, 1, 3000);
		if(ret < 0) {
			perror("poll:error");
		}
		if(ret == 0) {
			puts("REQUEST TIMEOUT");
			*header = NULL;
			return 0;
		}

		nchars = recv(sockfd, buff, CHUNKSIZE, 0);

		if(nchars <= 0) {	// server suddenly disconnects
			// close(sockfd);
			free(htemp);
			puts("Disconnected...");
			*header = NULL;
			return 0;
			// exit(EXIT_FAILURE);
		}

		if(!header_received)
		{
			int k = 0;
			while(k < nchars) {
				if(buff[k] == '\r') {cnt++;}
				else if(buff[k] != '\n' && buff[k] != '\r' && cnt == 1) {cnt--;}
				htemp[hsize++] = buff[k++];
				if(hsize == maxlimit) { // maxlimit is doubled when about to overflow
					maxlimit *= 2;
					htemp = (char *)realloc(htemp, (maxlimit+2)*sizeof(char));
				}
				if(cnt == 2) { 
					htemp[hsize++] = '\n';	// put the \n in the header

					content_len = parse_header(htemp, hsize, &status);
					
					header_received = true;

					switch(status)
					{
						case 200:
							puts("200 OK");
						break;

						case 400:
							puts("Error 400 Bad Request");
						break;

						case 403:
							puts("Error 403 Forbidden");
						break;

						case 404:
							puts("Error 404 Not Found");
						break;

						default:
							printf("%d Unkown Error\n", status);
					}

					if(status != 200 || filename == NULL) {
						*header = htemp;
						return hsize;
					}
					
					FILE *fptr = fopen(filename, "wb");
					fwrite(buff+k+1, sizeof(char), nchars-k-1, fptr);
					total += nchars-k;
					fclose(fptr);
					break;
				} 
			}
		}
		else {
			if(status != 200 || filename == NULL) break;

			FILE *fptr = fopen(filename, "ab");
			fwrite(buff, sizeof(char), nchars, fptr);
			total += nchars;
			fclose(fptr);
			if(total >= content_len) break;
		}
	}
	// htemp[hsize] = '\0';
    *header = htemp;

	if(filename != NULL) openapp(filename, strlen(filename));

	return hsize;
}



void sendfile(int sockfd, char *filename) {
	FILE *fptr = fopen(filename, "rb");
	char p[CHUNKSIZE + 1] = {0};
	int bytes = 0;
	while ((bytes = fread(p, 1, CHUNKSIZE, fptr)) > 0)
	{
		send(sockfd, p, bytes, 0);
	}
	fclose(fptr);
}

/*
	Function to create a TCP socket
	Returns the socket file descriptor
*/
int createsocket() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Cannot create socket\n");
		exit(EXIT_FAILURE);
	}
	return sockfd;
}



int parseurl(char *url, int len, char **ip, char **path) {
	char port[7] = {0};
	char *st = strstr(url, "://") + 3;
	char *end = st;

	while(*end != ':' && *end != '/') end++;
	char * t_ip = malloc((end-st+1)*sizeof(char));
	int k = 0;
	while(st < end)
	{
		t_ip[k] = *st;
		k++; st++;
	}
    t_ip[k] = '\0';

	if(*end == ':') {
		while(*end != '/') end++;
		st++;	// start from next to ':'
		k = 0;
		while(st < end) {
			port[k] = *st;
			k++; st++;
		}
		port[k] = '\0';
	}

	while(*end != ':' && end < url+len) end++;
	char * t_path = malloc((end-st+1)*sizeof(char));
	k = 0;
	while(st < end)
	{
		t_path[k] = *st;
		k++; st++;
	}
    t_path[k] = '\0';

    *ip = t_ip;
    *path = t_path;

	if(end == url+len) {
		if(port[0] == '\0') return 80;	// default port
		else return atoi(port);
	}
    end++;      // otherwise *end == ':' so shift end once
	k = 0;
	while(end < url+len) 
	{
		port[k] = *end;
		k++; end++;
	}
	port[k] = '\0';
	return atoi(port);
}


void DatePlusDays( struct tm* date, int days )
{
    const time_t ONE_DAY = 24 * 60 * 60 ;

    // Seconds since start of epoch
    time_t date_seconds = mktime( date ) + (days * ONE_DAY) ;

    *date = *gmtime( &date_seconds ) ;
}


int createheader(char *ip, int port, char *path, char **header, char *sendfilename) {

	char doctype[20];
	char htemp[256];
	

	if(sendfilename == NULL)	// GET
	{
		char gtime[100];
		time_t tim;
		time(&tim);
		struct tm t = *gmtime(&tim);
		DatePlusDays(&t, -2);
		strftime(gtime, sizeof(gtime), "%a, %d %b %Y %H:%M:%S GMT", &t);

		int pathlen = strlen(path);
		if(strcmp(path + pathlen - 4, ".pdf") == 0) {
			strcpy(doctype, "application/pdf");
		}
		else if(strcmp(path + pathlen - 4, ".jpg") == 0) {
			strcpy(doctype, "image/jpeg");
		}
		else if(strcmp(path + pathlen - 5, ".html") == 0) {
			strcpy(doctype, "text/html");
		}
		else {
			strcpy(doctype, "text/*");
		}
		
		sprintf(htemp, "GET %s HTTP/1.1\r\n"
						"Host: %s:%d\r\n"	
						"Accept: %s\r\n"
						"Accept-Language: en-us\r\n"
						"If-Modified-Since: %s\r\n"
						"Connection: close\r\n"
						"\r\n", path, ip, port, doctype, gtime);
	}

	else
	{
		int fnamelen = strlen(sendfilename);
		if(strcmp(sendfilename + fnamelen - 4, ".pdf") == 0) {
			strcpy(doctype, "application/pdf");
		}
		else if(strcmp(sendfilename + fnamelen - 4, ".jpg") == 0) {
			strcpy(doctype, "image/jpeg");
		}
		else if(strcmp(sendfilename + fnamelen - 5, ".html") == 0) {
			strcpy(doctype, "text/html");
		}
		else {
			strcpy(doctype, "text/*");
		}

		FILE *fp = fopen(sendfilename, "rb");
		if (fp == NULL)
		{
			perror("Open file failed!\n");
			exit(EXIT_FAILURE);
		}

		// length of file
		fseek(fp, 0, SEEK_END);
		long length = ftell(fp);
		
		fclose(fp);

		sprintf(htemp, "PUT %s/%s HTTP/1.1\r\n"
						"Host: %s:%d\r\n"	
						"Content-Type: %s\r\n"
						"Content-Length: %ld\r\n"
						"Content-Language: en-us\r\n"
						"Connection: close\r\n"
						"\r\n", path, sendfilename, ip, port, doctype, length);
	}


	int len = strlen(htemp);
	*header = malloc((len+1)*sizeof(char));
	strcpy(*header, htemp);

	return len;
}

int send_request(char *header, int headerlen, char *ip, int port, char *filename) {

	int sockfd = createsocket();

	struct sockaddr_in servaddr;

	// Server Information
	servaddr.sin_family = AF_INET;
	inet_aton(ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);
	
	if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		perror("Unable to connect to server\n");
		exit(EXIT_FAILURE);
	}

	// send the header
	if(send(sockfd, header, headerlen, 0) < 0){
		perror("send:error");
		exit(EXIT_FAILURE);
	}

	if(filename != NULL) {	// Send file for PUT request
		sendfile(sockfd, filename);
	}

	return sockfd;
}


// Main function

int main() {
	char *line;
	char cmd[10], url[100], filename[100]; 
	size_t cmdsize;
	while(1) {
		printf("\nMyOwnBrowser> ");
		line = NULL;
		getline(&line, &cmdsize, stdin); // Err handle later
		if(line == NULL) continue;
		sscanf(line, " %s %s %s", cmd, url, filename);

		// char * token = strtok(cmd, " ");
		if(strcmp(cmd, "GET") == 0) 
		{
			// char *url = strtok(NULL, " ");
			if(url == NULL) {
				printf("Missing arguments");
				free(line);
				continue;
			}

			char *ip;
			char *serverpath;
			int port = parseurl(url, strlen(url), &ip, &serverpath);
			char *header;
			int headerlen = createheader(ip, port, serverpath, &header, NULL);

			// printf("%s  %d\n", ip, port);
			printf("Request Sent:\n%s", header);


			int sockfd = send_request(header, headerlen, ip, port, NULL);

			char *filename = strrchr(serverpath, '/');
			if(filename == NULL) filename = serverpath;
			else filename++;

			char * response_header;
			int res_header_len = getresponse(sockfd, filename, &response_header);

			printf("Response Received:\n%.*s", res_header_len, response_header);

			if(response_header) free(response_header);
			free(header);
			free(serverpath);
			free(ip);

			close(sockfd);
			
		}

		else if(strcmp(cmd, "PUT") == 0) 
		{
			if(url == NULL || filename == NULL) {
				printf("Missing arguments");
				free(line);
				continue;
			}
			
			char *ip;
			char *serverpath;
			int port = parseurl(url, strlen(url), &ip, &serverpath);
			char *header;
			int headerlen = createheader(ip, port, serverpath, &header, filename);

			// printf("%s  %d\n", ip, port);
			printf("Request Sent:\n%s", header);


			int sockfd = send_request(header, headerlen, ip, port, filename);

			char *filename = strrchr(serverpath, '/');
			if(filename == NULL) filename = serverpath;
			else filename++;

			char * response_header;
			int res_header_len = getresponse(sockfd, NULL, &response_header);

			printf("Response Received:\n%.*s", res_header_len, response_header);

			if(response_header) free(response_header);
			free(header);
			free(serverpath);
			free(ip);

			close(sockfd);
		}

		else if(strcmp(cmd, "QUIT") == 0) 
		{
			free(line);
			break;
		}

		else 
		{
			printf("Invalid command");
		}

		free(line);
	}

	
	return 0;
}
