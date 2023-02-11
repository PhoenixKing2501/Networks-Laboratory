
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


#define CHUNKSIZE (1<<10)


void openapp(char *filename, int len) {
	char cmd[30];

	if(strcmp(filename + len - 4, ".pdf") == 0) {
		strcpy(cmd, "acrobat");
	}
	else if(strcmp(filename + len - 4, ".jpg") == 0) {
		strcpy(cmd, "firefox");
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

int parse_header(char *buf, int n)
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
			int status;
			sscanf(line, "%s %d %s", version, &status, msg);

			// printf("method: %s\n", method);
			// printf("path: %s\n", path);
			// printf("version: %s\n", version);

			// strcpy(req->method, method);
			// strcpy(req->path, path);

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

			// printf("key: %s\n", key);
			// printf("value: %s\n", value);

			if (strcmp(key, "Content-Length") == 0)
			{
				return atoi(value);
			}

			// if (strcmp(key, "Host") != 0)
			// {
			// 	flag = false;
			// 	break;
			// }
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
	while(1) {
		nchars = recv(sockfd, buff, CHUNKSIZE, 0);

		if(nchars <= 0) {	// server suddenly disconnects
			printf("Disconnected...\n");
			close(sockfd);
			free(htemp);
			exit(EXIT_FAILURE);
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

					content_len = parse_header(htemp, hsize);
					
					header_received = true;

					if(filename == NULL) break;
					
					FILE *fptr = fopen(filename, "w");
					fprintf(fptr, "%.*s", nchars-k-1, buff+k+1);	// skipping the '\n' in buff[k]
					total += nchars-k;
					fclose(fptr);
					break;
				} 
			}
		}
		else {
			if(filename == NULL) break;

			FILE *fptr = fopen(filename, "a");
			fprintf(fptr, "%.*s", nchars, buff);
			total += nchars;
			fclose(fptr);
			if(total >= content_len) break;
		}
	}
	htemp[hsize] = '\0';
    *header = htemp;

	if(filename != NULL) openapp(filename, strlen(filename));

	return hsize;
}

/*
	Function to send message in chunks of CHUNKSIZE.
*/
void givemessage(int sockfd, char *msg, int msgsize) {
	int k = 0;
	while(k < msgsize) {
		char *bufptr = msg + k;
		int size = (CHUNKSIZE < msgsize-k+1) ? CHUNKSIZE : (msgsize-k+1);
		send(sockfd, bufptr, size, 0);
		k += size;
	}
}

void sendfile(int sockfd, char *filename) {
	FILE *fptr = fopen(filename, "rb");
	char p[CHUNKSIZE + 1] = {0};
	int bytes = 0;
	while ((bytes = fread(p, 1, CHUNKSIZE, fptr)) > 0)
	{
		send(sockfd, p, bytes, 0);
		// fwrite(p, 1, bytes, fp2);
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
	char *st = url+7;
	char *end = st;

	while(*end != '/') end++;
	char * t_ip = malloc((end-st+1)*sizeof(char));

	int k = 0;
	while(st < end)
	{
		t_ip[k] = *st;
		k++; st++;
	}
    t_ip[k] = '\0';

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

	if(end == url+len) return 80;	// default port
    end++;      // otherwise *end == ':' so shift end once
	char port[7];
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


int createheader(char *ip, char *path, char **header, char *sendfilename) {

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
						"Host: %s\r\n"	
						"Accept: %s\r\n"
						"Accept-Language: en-us\r\n"
						"If-Modified-Since: %s\r\n"
						"Connection: close\r\n"
						"\r\n", path, ip, doctype, gtime);
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

		sprintf(htemp, "PUT %s HTTP/1.1\r\n"
						"Host: %s\r\n"	
						"Content-Type: %s\r\n"
						"Content-Length: %ld\r\n"
						"Content-Language: en-us\r\n"
						"Connection: close\r\n"
						"\r\n", path, ip, doctype, length);
	}


	int len = strlen(htemp);
	*header = malloc(len*sizeof(char));
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
	givemessage(sockfd, header, headerlen);

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
		printf("\nMyOwnBrowser>");
		line = NULL;
		getline(&line, &cmdsize, stdin); // Err handle later
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
			int headerlen = createheader(ip, serverpath, &header, NULL);

			// printf("%s  %d\n", ip, port);
			// printf("%s", header);


			int sockfd = send_request(header, headerlen, ip, port, NULL);

			char *filename = strrchr(serverpath, '/');
			if(filename == NULL) filename = serverpath;
			else filename++;

			char * response_header;
			int res_header_len = getresponse(sockfd, filename, &response_header);

			printf("%.*s", res_header_len, response_header);

			free(response_header);
			free(header);
			free(serverpath);
			free(ip);
			
		}

		else if(strcmp(cmd, "PUT") == 0) 
		{
			// char *url = strtok(NULL, " ");
			// char *filename = strtok(NULL, " ");
			if(url == NULL || filename == NULL) {
				printf("Missing arguments");
				free(line);
				continue;
			}
			
			char *ip;
			char *serverpath;
			int port = parseurl(url, strlen(url), &ip, &serverpath);
			char *header;
			int headerlen = createheader(ip, serverpath, &header, filename);

			// printf("%s  %d\n", ip, port);
			// printf("%s", header);


			int sockfd = send_request(header, headerlen, ip, port, filename);

			char *filename = strrchr(serverpath, '/');
			if(filename == NULL) filename = serverpath;
			else filename++;

			char * response_header;
			int res_header_len = getresponse(sockfd, NULL, &response_header);

			printf("%.*s", res_header_len, response_header);

			free(response_header);
			free(header);
			free(serverpath);
			free(ip);

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
