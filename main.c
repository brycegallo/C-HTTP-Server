#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <zconf.h>
#include <zlib.h>

/* * * * HTTP Request
 * An HTTP request has three parts
 * 1. A request line
 * 2. Zero or more headers, each ending with a CRLF (\r\n)
 * 3. An optional response body
 * 
 * // Example Request: 
 * 	"GET /index.html HTTP/1.1\r\nHost: localhost:4221\r\nUser-Agent: curl/7.64.1\r\nAccept: * / *\r\n\r\n" 
 * 	(2 spaces added in Headers section to avoid messing with multi-line comments)
 * // Request Line
 * GET		// HTTP method
 * /index.html	// Request target
 * HTTP/1.1	// HTTP version
 * \r\n		// CRLF that marks the end of the request line
 * 
 * // Headers
 * Host: localhost:4221\r\n	// Header that specifies the server's host and port
 * User-Agent: curl/7.64.1\r\n	// Header that describes the client's user agent
 * Accept: * / *\r\n		// Header that specifies which media types the client can accept
 * \r\n				// CRLF that marks the end of the headers
 *
 * // Request body (empty in this case)
 *
 * */


/* * * * HTTP Response
 * An HTTP response has three parts:
 * 1. A status line
 * 2. Zero or more headers, each ending with a CRLF (\r\n)
 * 3. An optional response body
 *
 * // Example Response: "HTTP/1.1 200 OK\r\n\r\n"
 * // Status line
 * HTTP/1.1	// HTTP version
 * 200		// Status code
 * OK		// Optional reason phrase
 * \r\n		// CRLF that marks the end of the status line
 * 
 * // Headers (empty in this case)
 * \r\n		// CRLF that marks the end of the headers
 *
 * // Response body (empty in this case)
 *
 * */

/********************* CodeCrafters Values *********************/
const char* test_directory = "/tmp/data/codecrafters.io/http-server-tester/";
const char* type_octet = "\r\nContent-Type: application/octet-stream\r\n";
//const char* type_octet = "\r\nContent-Type: application/octet-stream\r\n";
const char* new_file_path_template = "/tmp/data/codecrafters.io/http-server-tester/%s";
/******************************  ******************************/

/********************* Reponse Definitions *********************/
// Define a 200 response indicating that the connection succeeded
const char* response_buffer_200_OK = "HTTP/1.1 200 OK\r\n\r\n";
// Define a 201 response indicating that the requested resource was created
const char* response_buffer_201_Created = "HTTP/1.1 201 Created\r\n\r\n";
// Define a 404 response indicating that the requested resource was not found
const char* response_buffer_404_NF = "HTTP/1.1 404 Not Found\r\n\r\n";
// Define an int with value 0 to be used when we don't want to include any flags
const int no_flags = 0;
/******************************  ******************************/

/********************** Reponse Building **********************/
const char* http_1_1 = "HTTP/1.1";

const char* status_code_200  = "200";
const char* status_code_201  = "201";
const char* status_code_404  = "404";

const char* reason_phrase_OK = " OK\r\n";
const char* reason_phrase_NF = " Not Found\r\n";
const char* reason_phrase_no = " \r\n";

const char* headers_none     = "\r\n";
const char* header_content_encoding = "\r\nContent-Encoding:";
const char* header_content_encoding_gzip = "\r\nContent-Encoding: gzip";

const char* text_plain = "Content-Type: text/plain\r\n";

const char* header_content_length = "\r\nContent-Length:";

const char* body_empty       = "";
/******************************  ******************************/

typedef struct buffer_struct {
    char* method;
    char* target;
    char* http_version;
    char* host;
    char* user_agent;
    char* accept_content_type;
    int*  content_encoding_active;
    int*  content_length;
    char* body;
} buffer_struct;


void disable_output_buffering(void) {
    // Disable output buffering
    setbuf(stdout, NULL);	
    setbuf(stderr, NULL);
}

// gzip_compress will take: a pointer to the body we want to compress, the size of the body, and the 
static char* gzip_deflate(char* input, size_t input_length, size_t* output_length) {
    printf("LOG____deflate\n");
    z_stream stream = {0};
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);

    size_t max_length = deflateBound(&stream, input_length);
    char* gzip_data = malloc(max_length);
    memset(gzip_data, 0, max_length);

    stream.next_in = (Bytef*)input;
    stream.avail_in = input_length;

    stream.next_out = (Bytef*)gzip_data;
    stream.avail_out = max_length;

    deflate(&stream, Z_FINISH);
    *output_length = stream.total_out;

    deflateEnd(&stream);

    return gzip_data;
}

void process_request_buffer(struct buffer_struct *request_buffer_struct, char request_buffer[1024]) {
    // Detect gzip as an accepted encoding
    if (strstr(request_buffer, "Accept-Encoding:") != NULL && strstr(request_buffer, "gzip") != NULL) {
	request_buffer_struct->content_encoding_active = (int*)1;
    } else {
	request_buffer_struct->content_encoding_active = (int*)0;
    }
    // Use strtok_r() for method, target, http_version, host, since these will always be present
    char* method = strtok_r(request_buffer, " ", &request_buffer); // first token will be request method
    char* target = strtok_r(request_buffer, " ", &request_buffer); // second token will be request target
    char* http_version = strtok_r(request_buffer, "\r\n", &request_buffer); // third token will be host
    char* host = strtok_r(request_buffer, "\r\n", &request_buffer); // third token will be host
    char* user_agent = strtok_r(request_buffer, "\r\n", &request_buffer);
    //	unfortunately user_agent for /user-agent requests will also have to be:
    //	body_length for post requests
    strtok_r(request_buffer, "\r\n", &request_buffer);
    char* body = strtok_r(request_buffer, "\r\n", &request_buffer);

    request_buffer_struct->method = method;
    request_buffer_struct->target = target;
    request_buffer_struct->http_version = http_version;
    request_buffer_struct->host = host;
    request_buffer_struct->user_agent = user_agent;
    request_buffer_struct->body = body;
    // Use regex for user-agent, accept
//    regex_t regex;
//    const size_t n_match = 10;
    //int regex_comp_result = regcomp(&regex, "^GET", 0);
    //int regex_comp_result = regcomp(&regex, "^GET", REG_EXTENDED | REG_ICASE);

    //int regex_comp_method_result = regcomp(&regex, "GET|POST", REG_EXTENDED | REG_ICASE);
//    int regex_comp_method_result = regcomp(&regex, "GET|POST", REG_EXTENDED | REG_ICASE);
//    regmatch_t p_match[n_match + 1];
//    int regexec_method_result = regexec(&regex, request_buffer, n_match, p_match, 0);
//    if (regexec_method_result == 0) {
//	printf("LOG____PRB()____REGEX MATCH\n");
//	for (size_t i = 0; p_match[i].rm_so != -1 && i < n_match; i++) {
//	//int i = 0;
//	char regex_buffer[256] = {0};
//	    //strncpy(regex_buffer, request_buffer_pointer + p_match[i].rm_so, p_match[i].rm_eo - p_match[i].rm_so);
//	    strncpy(regex_buffer, request_buffer + p_match[i].rm_so, p_match[i].rm_eo - p_match[i].rm_so);
//	    printf("LOG____PRB()____REGEX start %d, end %d: %s\n", p_match[i].rm_so, p_match[i].rm_eo, regex_buffer);
//	}
//    }
//    regfree(&regex);

//    printf("LOG____PRB()____Request Buffer: %s\n", request_buffer);
    // Array Contents:
    // array[0] = request method
    // array[1] = request target
    // array[2] = HTTP Version
    // array[3] = Host
    // array[4] = User-Agent	(optional - kind of)
    // array[5] = Accept	(optional)
    
}

void handle_request(char request_buffer[1024], int client_fd) {
    char* request_buffer_duplicate = strdup(request_buffer);

    struct buffer_struct request_buffer_struct;
    process_request_buffer(&request_buffer_struct, request_buffer_duplicate);

    char* request_method = request_buffer_struct.method;	
    char* request_target = request_buffer_struct.target;	

    printf("LOG____Request Buffer Struct->method: %s\n", request_buffer_struct.method);
    printf("LOG____Request Buffer Struct->target: %s\n", request_buffer_struct.target);
    printf("LOG____Request Buffer Struct->http_version: %s\n", request_buffer_struct.http_version);
    printf("LOG____Request Buffer Struct->host: %s\n", request_buffer_struct.host);

    int* content_encoding_active = request_buffer_struct.content_encoding_active;
    char* content_encoding;
    char response_buffer[1024]; // used for "echo" and "user-agent"
    

    //char* empty_response_template = "%s %s%s%s%s"; // used if request target is /
    char* response_template  = "HTTP/1.1 200 OK%s%sContent-Length: %zu\r\n\r\n%s";
    char* request_buffer_pointer = request_buffer;
    char* echo_message = request_target + 6;
    char* user_agent = request_buffer_struct.user_agent + 12;
    char* request_body = request_buffer_struct.body;
    char* file_name = request_target + 7;
    
    // Detect gzip as an accepted encoding
    if (strstr(request_buffer, "Accept-Encoding:") != NULL && strstr(request_buffer, "gzip") != NULL) {
	content_encoding = "\r\nContent-Encoding: gzip\r\n";
	printf("LOG____Client Accepts gzip\n");
    } else {
	printf("LOG____Client DOES NOT Accept gzip\n");
	content_encoding = "\r\n";
    }

    // Updated to use Empty Response Template
    if (!strcmp(request_target, "/")) {
	send(client_fd, response_buffer_200_OK, strlen(response_buffer_200_OK), no_flags);
    }
    else if (!strncmp(request_target, "/echo/", 6)) {    
	// No content_encoding
	if (!content_encoding_active) {
	    sprintf(response_buffer, response_template, content_encoding, text_plain, strlen(echo_message), echo_message);
	    send(client_fd, response_buffer, strlen(response_buffer), no_flags);
	}
	// content_encoding
	else {
	    uLong gzip_body_length;
	    char* gzip_response_body;
	    gzip_response_body = gzip_deflate(echo_message, strlen(echo_message), &gzip_body_length);
	    sprintf(response_buffer, response_template, content_encoding, text_plain, gzip_body_length, body_empty);
	    send(client_fd, response_buffer, strlen(response_buffer), no_flags);
	    send(client_fd, gzip_response_body, gzip_body_length, no_flags);
	}
    } 
    else if (!strcmp(request_target, "/user-agent")) {
	sprintf(response_buffer, response_template, content_encoding, text_plain, strlen(user_agent), user_agent);
	send(client_fd, response_buffer, strlen(response_buffer), no_flags);
    }
    else if (!strncmp(request_target, "/files/", 7)) {
	char file_path[1024];
	sprintf(file_path, new_file_path_template, file_name);
	printf("LOG____File Name%s\nLOG____File Path: %s\n", file_name, file_path);

	if (!strcmp(request_method, "GET")) {
	    FILE *file_fd = fopen(file_path, "r");
	
	    if (file_fd == NULL) {
		send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
	    }
	    else {
		char* file_read_buffer[1024] = {};
		int file_read_syze_bites = fread(file_read_buffer, 1, 1024, file_fd);
	    
		if (file_read_syze_bites > 0) {
		    sprintf(response_buffer, response_template, "", type_octet, file_read_syze_bites, file_read_buffer);
		    send(client_fd, response_buffer, strlen(response_buffer), no_flags);
		}
		else {
		    send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
		}
	    }
	    fclose(file_fd);
	}
	else if (!strcmp(request_method, "POST")) {
	    FILE *file_pointer = fopen(file_path, "w");
	    fprintf(file_pointer, request_body);
	    fclose(file_pointer);
	    send(client_fd, response_buffer_201_Created, strlen(response_buffer_201_Created), no_flags);
	}
    }
    else {
	send(client_fd, response_buffer_404_NF, strlen(response_buffer_404_NF), no_flags);
    }
    // All members of the struct buffer_struct assigned in process_request_buffer() will be invalid after this call
    free(request_buffer_duplicate);
}

// must take void* as an argument and return void*, to be made into a thread
void* handle_client(void* arg_client_fd) {
    int client_fd = *((int*)arg_client_fd);
    // Create a request buffer to accept the request to be received by recv()
    char request_buffer[1024];

    // recv(int socket, void *buffer, size_t length, int flags), returns ssize_t of message length
    recv(client_fd, request_buffer, 1024, no_flags);
    handle_request(request_buffer, client_fd);
}

int main(int argc, char **argv) {
//int main() {
    disable_output_buffering();
    
    int server_fd;

    // Creates a socket, server_fd is the socket's file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
	printf("LOG____Socket creation failed: %s...\n", strerror(errno));
	return 1;
    }
	
    // Tester restarts the program quite often, setting SO_REUSEADDR ensures we don't get 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
	printf("LOG____SO_REUSEADDR failed: %s \n", strerror(errno));
	return 1;
    }

    // Configure socket
    struct sockaddr_in serv_addr = { 
	.sin_family = AF_INET,
	.sin_port = htons(4221),
	.sin_addr = { htonl(INADDR_ANY) },
    };

    // Binds the socket created/configured above to a specified port
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
	printf("LOG____Bind failed: %s \n", strerror(errno));
	return 1;
    }

    int connection_backlog = 5; // number of pending client connections the server will allow to queue
    // Begin listening for incoming connections
    if (listen(server_fd, connection_backlog) != 0) {
	printf("LOG____Listen failed: %s \n", strerror(errno));
	return 1;
    }

    printf("LOG____Waiting for a client to connect...\n");

    // (1) malloc() here
    int* client_fd = malloc(sizeof(int));
    
    while (1) {
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	// Accept client connection, accept() will return file descriptor of the accepted socket
	*client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_len);
	// // theoretically, client_fd might be variable and could potentially require realloc(), but probably not
	printf("LOG____Client connected\n");

	// Create a thread for handle_client()
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, handle_client, (void*)client_fd);
	pthread_detach(thread_id);

    }
    if (client_fd) { free(client_fd); }
    
    // Close the server file descriptor
    close(server_fd);

    return 0;
}
