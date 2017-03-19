#include <unistd.h>
#include "gfserver.h"

//sb additional includes
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <math.h>
/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */
typedef struct gfserver_t{
	//char * server;
	//char * path;
	unsigned short port;
	int max_npending;
	//int status;
	ssize_t (*handlerfunc)(gfcontext_t *, char *, void*);
	void *handlerarg;
	//void (*writefunc)(void*, size_t, void *);
	//void *writearg;
	//size_t bytesreceived;
}gfserver_t;

typedef struct gfcontext_t{
	int sockfd;
	//gfstatus_t status;
	char fpath[256];
	char client_req_path[1000];
}gfcontext_t;

//SB FUNCTIONS
int gfc_get_path(gfcontext_t *gfc);
void gfc_create_path(gfcontext_t *gfc, char *path);

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){

	/*
	 * Sends to the client the Getfile header containing the appropriate
	 * status and file length for the given inputs.  This function should
	 * only be called from within a callback registered gfserver_set_handler.
	 */
	//send(newsockfd, (void*)temp_str, sizeof(temp_str), 0);3



	char *scheme = "GETFILE";
	char str_status[15] = "";
	//size of scheme + size of status + max size int 4294967295 (len = 10, add 5
	//for good measure) add 3 for each subelement of array
	char *header = (char *)malloc(strlen(scheme) + sizeof(str_status) + 15 + 3);

	//Create string from integer of file length
	//sprintf(int_str, "%zu", file_len);
	//Get a string status from macro definitions
	if (status == GF_OK){
		strcpy(str_status, "OK");
		sprintf(header, "%s %s %zu\r\n\r\n", scheme, str_status, file_len);
	}
	else if (status == GF_FILE_NOT_FOUND){
		strcpy(str_status, "FILE_NOT_FOUND");
		sprintf(header, "%s %s \r\n\r\n", scheme, str_status);
	}
	else{
		strcpy(str_status, "ERROR");
		sprintf(header, "%s %s \r\n\r\n", scheme, str_status);
	}

	printf("scheme = %s, status = %s, filelen = %zu\n", scheme, str_status, file_len);
	printf("gfserver.sendheaver = %s\n", header);
	return send(ctx->sockfd, (void*)header, strlen(header), 0);

}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
	/*
	 * Sends size bytes starting at the pointer data to the client
	 * This function should only be called from within a callback registered
	 * with gfserver_set_handler.  It returns once the data has been
	 * sent.
	 */

	ssize_t i = send(ctx->sockfd, (void*)data, len, 0);
	if (i == -1)
		perror("gfs_send: had an error sending\n");
	else
		printf("gfs_send: sent this size %zu\n", i);
	return i;
}

void gfs_abort(gfcontext_t *ctx){
	/*
	 * Aborts the connection to the client associated with the input
	 * gfcontext_t.
	 */
	close(ctx->sockfd);

}

gfserver_t* gfserver_create(){
	/*
	 * This function must be the first one called as part of
	 * setting up a server.  It returns a gfserver_t handle which should be
	 * passed into all subsequent library calls of the form gfserver_*.  It
	 * is not needed for the gfs_* call which are intended to be called from
	 * the handler callback.
	 */
	gfserver_t *gfs = (gfserver_t *)malloc(sizeof(gfserver_t));
	return gfs;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){

	/*
	 * Sets the port at which the server will listen for connections.
	 */
	gfs->port = port;
}
void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
	/*
	 * Sets the maximum number of pending connections which the server
	 * will tolerate before rejecting connection requests.
	 */

	gfs->max_npending = max_npending;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
	/*
	 * Sets the handler callback, a function that will be called for each each
	 * request.  As arguments, the receives
	 * - a gfcontext_t handle which it must pass into the gfs_* functions that
	 * 	 it calls as it handles the response.
	 * - the requested path
	 * - the pointer specified in the gfserver_set_handlerarg option.
	 * The handler should only return a negative value to signal an error.
	 */
	gfs->handlerfunc = handler;
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
	/*
	 * Sets the third argument for calls to the handler callback.
	 */
	gfs->handlerarg = malloc(sizeof(arg) + 1);
	gfs->handlerarg = arg;
}

void gfserver_serve(gfserver_t *gfs){
	/*
	 * Starts the server.  Does not return.
	 */
	  int sockfd, newsockfd;
	  socklen_t clilen;
	  struct sockaddr_in serv_addr, cli_addr;


	  sockfd = socket(AF_INET, SOCK_STREAM, 0);
	  if (sockfd < 0){
		  perror("ERROR opening socket");
	  }
	  bzero((char *) &serv_addr, sizeof(serv_addr));
	  serv_addr.sin_family = AF_INET;
	  serv_addr.sin_addr.s_addr = INADDR_ANY;
	  serv_addr.sin_port = htons(gfs->port);

	  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1){
		  perror("Error on setting options");
	   }
	  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
	          perror("ERROR on binding");
	  }
	  if (listen(sockfd,5) == -1){
		  perror("ERROR on listen");
	  }
	  clilen = sizeof(cli_addr);

	  while (1){
		  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		  if (newsockfd < 0){
			  perror("ERROR on accept");
		  }
		  gfcontext_t *gfc = (gfcontext_t *)malloc(sizeof(gfcontext_t));
		  gfc->sockfd = newsockfd;

		  //receieve fullpath GETFILE GET FILEPATH\r\n\r\n
		  recv(gfc->sockfd , (void*)gfc->client_req_path, 1000, 0);
		  //from client sent path extract FILEPATH
		  int malf_bool = gfc_get_path(gfc);
		  printf("SERVER:This is the client request %s\n", gfc->client_req_path);
		  printf("SERVER:This is the extracted filepath %s\n", gfc->fpath);
		  if (malf_bool == 0)
			  gfs->handlerfunc(gfc, gfc->fpath, gfs->handlerarg);
	  }
}

int gfc_get_path(gfcontext_t *gfc){
	/*
	 returns 0 if request is not malformed
	 returns -1 if request
	 */
	int ret_scanf;
	char temp_pot_fpath[256] ="";

	//request must end with \r\n\r\n or space
	ret_scanf = sscanf(gfc->client_req_path, "GETFILE GET %s\r\n\r\n", temp_pot_fpath);
	if (ret_scanf == EOF)
		ret_scanf = sscanf(gfc->client_req_path, "GETFILE GET %s ", temp_pot_fpath); //see uf ebds with space

	if (ret_scanf == EOF){
		printf("This was the malformed path '%s'\n", temp_pot_fpath);
		gfc_create_path(gfc, "");
		return -1;
	}
	else{
		//fpath start with "/"
		if (strncmp(temp_pot_fpath, (char *)"/", 1) == 0){
			gfc_create_path(gfc, temp_pot_fpath);
			printf("GFC-path sucessfuly set to '%s'\n", gfc->fpath);
			return 0;
		}
		else{
			gfc_create_path(gfc, "");
			perror("FPATH DID NOT BEGIN WIHT '/'\n");
			return -1;
		}
	}
	return 1;
}

void gfc_create_path(gfcontext_t *gfc, char *path){
	strcpy(gfc->fpath, path);
}
