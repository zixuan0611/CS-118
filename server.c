#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>

#define MYPORT 8000
#define BACKLOG 10
#define OK "200 OK"
#define MOVED "300 Moved Permanently"
#define BADREQUEST "400 Bad Request"
#define NOTFOUND "404 Not Found"
#define NOTSUPPORT "500 HTTP Version Not Supported"
#define DEFAULT_TYPE "application/octet-stream"

void to_lower_case(char* s) {
	for ( ; *s; s++) 
		*s = tolower(*s);
}

void remove_carriage_return(char* s) {
    for (; *s; s++) {
        if (*s == '\r') {
        	*s = '\0';
        	s++;
        	*s = '\0';
        	break;
        }
    }   
}

char* get_file_format(char* filename)
{
    char* ext = strrchr(filename, '.');
    if (ext != NULL) {
	    ext++;
	    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
	    	return "text/html";
	   	else if (strcmp(ext, "txt") == 0)
	   		return "text/plain";
	    else if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0)
	    	return "image/jpeg"; 
	    else if (strcmp(ext, "png") == 0)
	    	return "image/png";
	    else if (strcmp(ext, "gif") == 0) 
	    	return "image/gif";
	}
    return DEFAULT_TYPE;
}

void send_response(int fd, char* protocol, char* header, char* content_type, char* body, int content_length){
	const long max_buffer_size = 4096; 
	char* buffer = malloc(sizeof(char) * max_buffer_size);
	memset(buffer, '\0', max_buffer_size);
	sprintf(buffer, "%s %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
		protocol, header, content_type, content_length);
	if (write(fd, buffer, strlen(buffer)) < 0) {
		perror("write");
	}
	if (write(fd, body, content_length) < 0) {
		perror("write");
	}
	free(buffer);
}

void default_index(int fd, char* protocol) {
	char format[10] = "text/html\0";
    char filedata[53] = "<html>\n<body>\n<h1>Hello, World!</h1>\n</body>\n</html>\0";
    long length = strlen(filedata);
    send_response(fd, protocol, OK, format, filedata, length);
}

void resp_404(int fd, char* protocol){
    char format[10] = "text/html\0";
    char filedata[59] = "<html>\n<body>\n<h1>404, Page Not Found</h1>\n</body>\n</html>\0";
    long length = strlen(filedata);
    send_response(fd, protocol, NOTFOUND, format, filedata, length);
}

void not_supported(int fd, char* protocol) {
	char format[10] = "text/html\0";
    char filedata[71] = "<html>\n<body>\n<h1>500, HTTP Version Not Supported</h1>\n</body>\n</html>\0";
    long length = strlen(filedata);
    send_response(fd, protocol, NOTSUPPORT, format, filedata, length);
}

void bad_request(int fd, char* protocol) {
	char format[10] = "text/html\0";
    char filedata[56] = "<html>\n<body>\n<h1>400, Bad Request</h1>\n</body>\n</html>\0";
    long length = strlen(filedata);
    send_response(fd, protocol, BADREQUEST, format, filedata, length);
}

FILE* open_file(char* filename) {
	DIR *directory;
	struct dirent *dir;

	char directory_path[2048];
	
	/*gets directory path*/
	if (getcwd(directory_path, sizeof(directory_path)) == NULL) {
       perror("getcwd() error");
       exit(1);
   	}

   	directory = opendir(directory_path);

   	if (directory) {
   		/*reads all files in directory*/
   		while ((dir = readdir(directory)) != NULL) {
   			/*only check regular files*/
   			if (dir->d_type == DT_REG) {
   				/*compares the filenames ignoring case*/
   				if (strcasecmp(filename, dir->d_name) == 0) {
   					char filepath[4096];
   					sprintf(filepath, "%s/%s", directory_path, dir->d_name);
   					
   					FILE* filefd = fopen(filepath, "rb");
   					return filefd;
   				}
   			}
   		}
   	}
   	return NULL;
} 

void handle_get(int fd, char* filename, char* protocol) {
	if (strcmp(filename, "") == 0) {
		default_index(fd, protocol);
		return; 
	}

    char* format = get_file_format(filename);
    
    long total_bytes;

    FILE *filefd = open_file(filename);

    if (filefd == NULL) {
		resp_404(fd, protocol);
		return;
    }

    if (fseek(filefd, 0L, SEEK_END) != 0){
		perror("fseek");
		return;
	};

	total_bytes = ftell(filefd);
	char* buffer = malloc(sizeof(char) * (total_bytes + 1));

	if (fseek(filefd, 0L, SEEK_SET) != 0){
		perror("fseek");
		return;
	}
	
	fread(buffer, sizeof(char), total_bytes, filefd);
			
	fclose(filefd);

	send_response(fd, protocol, OK, format, buffer, total_bytes);

	free(buffer);
}

void handle_request(int fd){
	const int buffer_size = 512000; //512kb
	char buffer[buffer_size];
	memset(buffer, '\0', buffer_size);

	int bytes_read = read(fd, buffer, buffer_size);
	
	if (bytes_read == -1) {
		perror("read");
		exit(1);
	}

	if (bytes_read > 0) {
		printf("%s\n", buffer);

		if (strstr(buffer, "HTTP") == NULL) {
			bad_request(fd, "HTTP/1.1");
			return;
		}

		const int method_size = 10;
		char *method = malloc(sizeof(char) * method_size);
		memset(method, '\0', method_size);

		const int filename_size = 128;
		char *filename = malloc(sizeof(char) * filename_size);
		memset(filename, '\0', filename_size);

		const int protocol_size = 20;
		char* protocol = malloc(sizeof(char) * protocol_size);
		memset(protocol, '\0', protocol_size);

		char* protocol_version = NULL;
		char* delimeter = "\n";
		char* header = strtok(buffer, delimeter);
		remove_carriage_return(header);
		delimeter = " /%20";

		char* token = strtok(header, delimeter);
		strcpy(method, token);

		token = strtok(NULL, delimeter);
		if (strcmp(token, "HTTP") == 0) {
				protocol_version = strtok(NULL, delimeter);
		}
		else {
			while (token != NULL) {
				strcat(filename, token);
				token = strtok(NULL, delimeter);
				if (strcmp(token, "HTTP") == 0) {
					protocol_version = strtok(NULL, delimeter);
					break;
				}
				strcat(filename, " ");
			}
		}

		if (protocol_version == NULL) {
			bad_request(fd, "HTTP/1.1");
			free(protocol);
			free(filename);
			free(method);
			return;
		}

		if (strcmp(protocol_version, "1.1") != 0 && strcmp(protocol_version, "1.0") != 0) {
			not_supported(fd, "HTTP/1.1");
			free(protocol);
			free(filename);
			free(method);
			return;
		}

		to_lower_case(filename);
		sprintf(protocol, "HTTP/%s", protocol_version);
		
		// printf("method=%s,filename=%s,protocol=%s", method, filename,protocol);
		
		if ((strcmp(method, "GET") == 0)) {
			handle_get(fd, filename, protocol);
		}
		else {
			bad_request(fd, protocol);
		}
		
		free(protocol);
		free(filename);
		free(method);
	
	}
}

int main(){

   	/*listen on sockfd, new connection on new_fd*/
   	int sockfd; 
	int new_fd; 
	struct sockaddr_in my_addr; /*my address*/
	struct sockaddr_in their_add; /*connector addr*/
	socklen_t sin_size;

	/*create a socket*/
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(MYPORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

	/*bind socket*/
	if (bind(sockfd, (struct sockaddr*) &my_addr, 
		sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	while(1) {
		sin_size = sizeof(struct sockaddr_in);

		if ((new_fd = accept(sockfd, (struct sockaddr*) &their_add, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		
		printf("Server: Got connection from %s\n", 
			inet_ntoa(their_add.sin_addr));
		
		int pid = fork();

		if (pid == -1) {
			perror("fork");
		}

		if (pid == 0) {
			handle_request(new_fd);
			close(new_fd);
			exit(0);
		}
		else {
			close(new_fd);
		}
	}

	return 0;
}







