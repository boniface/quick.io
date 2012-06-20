#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

int get_conn() {
	int sockfd = 0;
	struct sockaddr_in serv_addr; 

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error : Could not create socket\n");
		return 0;
	} 

	memset(&serv_addr, '0', sizeof(serv_addr)); 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5000);

	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
		printf("inet_pton error occured\n");
		return 0;
	} 
	
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
	   printf("Error : Connect Failed: %s\n", strerror(errno));
	   return 0;
	}
	
	return sockfd;
}

int main(int argc, char *argv[]){
	int sock = get_conn();
	write(sock, "test", 4);
	
	char buff[1000];
	int len = read(sock, buff, sizeof(buff)-1);
	buff[len] = 0;
	
	printf("%s\n", buff);

	return 0;
}