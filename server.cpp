//
// Created by jacky1 on 2021/8/7.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>
#include "threadpool.h"

#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000

#define OPEN_MAX 5000

static struct sockaddr_in cliaddr, servaddr;

int main(int argc, char *argv[])
{
    int i, listenfd, connfd, sockfd;
    int  n, num = 0;
    ssize_t nready, efd, res;
    socklen_t clilen;



    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));      //端口复用
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    Bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    Listen(listenfd, 20);

    efd = epoll_create(OPEN_MAX);               //创建epoll模型, efd指向红黑树根节点
    if (efd == -1)
        perr_exit("epoll_create error");

    struct epoll_event tep, ep[OPEN_MAX];       //tep: epoll_ctl参数  ep[] : epoll_wait参数

    tep.events = EPOLLIN;
    tep.data.fd = listenfd;           //指定lfd的监听时间为"读"

    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);    //将lfd及对应的结构体设置到树上,efd可找到该树
    if (res == -1)
        perr_exit("epoll_ctl error");

    ThreadPool threadPool(20);
    threadPool.start();

    while (1){
        /*epoll为server阻塞监听事件, ep为struct epoll_event类型数组, OPEN_MAX为数组容量, -1表永久阻塞*/
        nready = epoll_wait(efd, ep, OPEN_MAX, -1);
        if (nready == -1)
            perr_exit("epoll_wait error");

        for (i = 0; i < nready; i++) {
            threadPool.appendWork([=, &tep,&ep](){
                if (!(ep[i].events & EPOLLIN))      //如果不是"读"事件, 继续循环
                    return;

                char buf[MAXLINE],str[INET_ADDRSTRLEN];
                if (ep[i].data.fd == listenfd) {    //判断满足事件的fd是不是lfd
                    socklen_t clilen = sizeof(cliaddr);
                    int connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);    //接受链接

                    printf("received from %s at PORT %d\n",
                           inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
                           ntohs(cliaddr.sin_port));
                    // printf("cfd %d---client %d\n", connfd, ++num);

                    tep.events = EPOLLIN; tep.data.fd = connfd;
                    ssize_t res = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep);      //加入红黑树
                    if (res == -1)
                        perr_exit("epoll_ctl error");

                } else {                                                    //不是lfd,
                    int sockfd = ep[i].data.fd;
                    int n = Read(sockfd, buf, MAXLINE);

                    if (n == 0) {                                           //读到0,说明客户端关闭链接
                        int res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);  //将该文件描述符从红黑树摘除
                        if (res == -1)
                            perr_exit("epoll_ctl error");
                        Close(sockfd);                                      //关闭与该客户端的链接
                        printf("client[%d] closed connection\n", sockfd);

                    } else if (n < 0) {                                     //出错
                        perror("read n < 0 error: ");
                        int res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);  //摘除节点
                        Close(sockfd);

                    } else {                                                //实际读到了字节数
                        for (int j = 0; j < n;j++)
                            buf[j] = toupper(buf[j]);                       //转大写,写回给客户端

                        Write(STDOUT_FILENO, buf, n);
                        Writen(sockfd, buf, n);
                    }
                }
            });
        }
    }

    threadPool.stop();
    Close(listenfd);
    Close(efd);

    return 0;
}

