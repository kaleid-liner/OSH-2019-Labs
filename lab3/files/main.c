#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "netutils.h"
#include "main.h"

#define PORT 8000

#define min(a, b) ((a) < (b) ? (a) : (b))

int parse_request(const char *req_str, request_t *req_info) {
    if (sscanf(req_str, "%s %s %s", req_info->method, req_info->uri, req_info->version) != 3) {
        fprintf(stderr, "malformed http request\n");
        return -1;
    }
}

void handle_request(const request_t *req) {
    if (strncmp(req->method, "GET", 3) != 0) {
        fprintf(stderr, "only support `GET` method\n");
        return;
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
    free(abs_path);
    free(cur_dir);
    if (file == NULL) {
        if (errno == ENOENT) {
            send_response(req->connfd, NF, content_404, strlen(content_404));
        } else {
            send_response(req->connfd, ISE, content_500, strlen(content_500));
        }
    }

    send_file_response(req->connfd, file);
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
        fprintf(stderr, "error while sending response\n");
        return;
    }
    if (rio_writen(connfd, content, content_length) < content_length) {
        fprintf(stderr, "error while sending response\n");
    }
}

void send_file_response(int connfd, FILE *file) {
    fseek(file, 0L, SEEK_END);
    __off_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    if (file_size < 0) file_size = LONG_MAX;
    
    char header[64];
    char *buf = (char *)malloc(min(BUF_SIZE, file_size));
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

    fprintf(stderr, "start reading from %d\n", connfd);

    size_t readn = readlinefd(connfd, header);
    size_t buf_read_n;
    while ((buf_read_n = readlinefd(connfd, buf))) {
        if (buf_read_n <= 2) {
            break;
        }
    }

    free(buf);

    header[readn] = 0;
    request_t req_info;
    if (parse_request(header, &req_info) < 0) {
        send_response(connfd, ISE, content_500, strlen(content_500));
        return;
    }

    free(header);

    req_info.connfd = connfd;
    handle_request(&req_info);

    close(connfd);
}

void *thread(void *args) {
    struct epoll_event *events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * 64);
    struct epoll_event ev;
    int epollfd = ((struct thread_args*)args)->epollfd;
    int listenfd = ((struct thread_args*)args)->listenfd;

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);

    int nfds;
    while ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) > 0) {
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listenfd) {
                int connfd = accept(listenfd, (struct sockaddr *)&clnt_addr, &clnt_addr_len);
                if (connfd < 0) {
                    fprintf(stderr, "error while accepting listenfd\n");
                    continue;
                }
                setnonblocking(connfd);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) < 0) {
                    fprintf(stderr, "error while adding connfd to epoll inst\n");
                    continue;
                }
            } else {
                server(events[n].data.fd);
                epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, &ev);
            }
        }
    }

    free(events);
}

void setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
      
    fcntl(fd, F_SETFL, new_option);
}

int main() {
    int listenfd = open_listenfd(PORT);

    pthread_t threads[THREAD_NUM];
    
    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        fprintf(stderr, "error while creating epoll fd\n");
        exit(1);
    }

    struct thread_args targs;
    targs.epollfd = epollfd;
    targs.listenfd = listenfd;

    struct epoll_event epevent;
    epevent.events = EPOLLIN | EPOLLET;
    epevent.data.fd = listenfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &epevent) < 0) {
        fprintf(stderr, "error while adding listen fd to epoll inst\n");
        exit(1);
    }

    for (int i = 0; i < THREAD_NUM; ++i) {
        if (pthread_create(&threads[i], NULL, thread, &targs) < 0) {
            fprintf(stderr, "error while creating %d thread\n", i);
            exit(1);
        }
    }

    thread(&targs);

    close(epollfd);
    close(listenfd);
}