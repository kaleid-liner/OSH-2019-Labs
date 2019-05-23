#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include "netutils.h"
#include "main.h"

#define PORT 8000

int parse_request(const char *req_str, request_t *req_info) {
    if (sscanf(req_str, "%s %s %s", req_info->method, req_info->uri, req_info->version) != 3) {
        fprintf(stderr, "malformed http request\n");
        return -1;
    }
}

void handle_request(const request_t *req) {
    if (strncmp(req->method, "GET", 3) != 0) {
        fprintf(stderr, "only support `GET` method");
    }

    char *abs_path = (char *)malloc(PATH_MAX);
    const char *rel_path = req->uri[0] == '/' ? req->uri + 1 : req->uri;
    char *cur_dir = (char *)malloc(PATH_MAX);

    realpath(rel_path, abs_path);
    getcwd(cur_dir, PATH_MAX);

    if (strncmp(cur_dir, abs_path, strlen(cur_dir)) != 0) {
        send_response(req->connfd, ISE, content_500, strlen(content_500));
    } 

    FILE *file = fopen(abs_path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            send_response(req->connfd, NF, content_404, strlen(content_404));
        } else {
            send_response(req->connfd, ISE, content_500, strlen(content_500));
        }
    }

    send_file_response(req->connfd, file);
    free(abs_path);
    free(cur_dir);
}

void send_response(int connfd, status_t status, 
                   const char *content, size_t content_length) {
    char buf[128];

    if (status == NF) {
        sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
    } else if (status == OK) {
        sprintf(buf, "HTTP/1.0 200 OK\r\n");
    } else {
        sprintf(buf, "HTTP/1.0 500 Internal Servere Error\r\n");
    }

    sprintf(buf, "%sContent-Length: %lu\r\n\r\n", buf, content_length);

    size_t buf_len = strlen(buf);

    if (rio_writen(connfd, buf, buf_len) < buf_len) {
        fprintf(stderr, "error while sending response");
        return;
    }
    if (rio_writen(connfd, content, content_length) < content_length) {
        fprintf(stderr, "error while sending response");
    }
}

void send_file_response(int connfd, FILE *file) {
    fseek(file, 0L, SEEK_END);
    __off_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    
    char header[64];
    char *buf = (char *)malloc(BUF_SIZE);
    sprintf(header, "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n", file_size);
    rio_writen(connfd, header, strlen(header));

    size_t readn;
    while ((readn = fread(buf, 1, BUF_SIZE, file)) > 0) {
        rio_writen(connfd, buf, readn);
    }

    free(buf);
}

void server(int connfd) {
    char *header = (char *)malloc(MAX_HEADER);
    char *buf = (char *)malloc(MAX_HEADER);

    size_t readn = readlinefd(connfd, header);
    size_t buf_read_n;
    while ((buf_read_n = readlinefd(connfd, buf))) {
        if (buf_read_n <= 2) {
            break;
        }
    }

    header[readn] = 0;
    request_t req_info;
    if (parse_request(header, &req_info) < 0) {
        send_response(connfd, ISE, content_500, strlen(content_500));
        return;
    }

    req_info.connfd = connfd;
    handle_request(&req_info);

    close(connfd);
    free(header);
    free(buf);
}

int main() {
    int listenfd = open_listenfd(PORT);
    
    struct sockaddr_in clnt_addr;
    socklen_t addr_len = sizeof(clnt_addr);

    while (1) {
        int connfd = accept(listenfd, (struct sockaddr *)&clnt_addr, &addr_len);
        server(connfd);
    }

    close(listenfd);
}