#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <pthread.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include "sem.h"
#include <signal.h>
#include <event.h>

void recv_fun(int fd,short ev,void *arg)
{
    if(ev&EV_READ)
    {
        char buf[128] = {0};
        int n = recv(fd,buf,127,0);
        if(n<=0)
        {
            close(fd);
            printf("client close\n");
            return;
        }
        printf("buf=%s\n",buf);
        send(fd,"ok",2,0);
    }
}

void accpet_fun(int fd,short ev,void *arg)
{
    struct event_base *base = (struct event_base*)arg;
    if(ev&EV_READ)
    {
        struct sockaddr_in caddr;
        unsigned int len = sizeof(caddr);
        int c = accept(fd,(struct sockaddr*)&caddr,&len);
        if(c<0)
        {
            return;
        }
        printf("accept c=%d\n",c);
        struct event *c_ev = event_new(base,c,EV_READ|EV_PERSIST, recv_fun,NULL);
        if(c_ev == NULL)
        {
            close(c);
            return;
        }
        event_add(c_ev,NULL);
    }
}

int create_socket(char const *ip,uint16_t const port)
{
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        return -1;
    }
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(ip);
    saddr.sin_port = htons(port);

    int res = bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
    if (res==-1)
    {
        return -1;
    }
    res = listen(sockfd,5);
    if(res==-1)
    {
        return -1;
    }
    return sockfd;
}

void create_pipe(char const*file)
{
    char *str = "mkfifo ";
    strcat(str,file);
    system(str);
}

void *fun(void *arg)
{
    int i=0;
    for (;i<10;i++)
    {
        printf("fun run\n");
        sleep(1);
    }
    pthread_exit("fun over");
}

int wg = 0;
pthread_mutex_t mut;
void *fun_(void *arg)
{
    int i=0;
    for(;i<1000;i++)
    {
        pthread_mutex_lock(&mut);
        printf("wg=%d\n",++wg);
        pthread_mutex_unlock(&mut);
    }
}

void *thread_fun(void *arg)
{
    fork();
    int i=0;
    for(;i<5;i++)
    {
        printf("fun:pid=%d\n",getpid());
        sleep(1);
    }
}

void Http(int const fd,int const connect)
{
    int size = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);

    char sendbuf[512] = {0};
    strcpy(sendbuf,"HTTP/1.1 200 OK\r\n");
    strcat(sendbuf,"Server:127.0.0.1\r\n");
    sprintf(sendbuf+ strlen(sendbuf),"Content-Length:%d\r\n",size);
    strcat(sendbuf,"\r\n");
    printf("send:\n%s\n",sendbuf);
    send(connect,sendbuf, strlen(sendbuf),0);
}

void Html(int const fd,int const connect)
{
    char data[512] = {0};
    int num = 0;
    while ((num= read(fd,data,512))>0)
    {
        send(connect,data,num,0);
    }
}

#define MAXFD 100
#define STDIN 0

void fds_init(int fds[])
{
    int i=0;
    for(;i<MAXFD;i++)
    {
        fds[i]=-1;
    }
}

void fds_add(int fds[],int fd)
{
    int i = 0;
    for(;i<MAXFD;i++)
    {
        if(fds[i]==-1)
        {
            fds[i] = fd;
            break;
        }
    }
}

void fds_del(int fds[],int fd)
{
    int i=0;
    for(;i<MAXFD;i++)
    {
        if(fds[i]==fd)
        {
            fds[i]=-1;
            break;
        }
    }
}

int new_accpet(int fds[],int sockfd)
{
    struct sockaddr_in caddr;
    unsigned int len = sizeof(caddr);
    int c = accept(sockfd,(struct sockaddr*)&caddr,&len);
    if(c==-1)
    {
        return -1;
    }

    printf("accpet c=%d\n",c);
    fds_add(fds,c);
    return 1;
}

void accpet_data(int fds[],int i)
{
    char buf[128] = {0};
    int res = recv(fds[i],buf,127,0);
    if(res<=0)
    {
        close(fds[i]);
        fds_del(fds,fds[i]);
        printf("one client over\n");
    } else
    {
        printf("buf(c=%d)=%s\n",fds[i],buf);
        send(fds[i],"ok",2,0);
    }
}

#define MAXFD_ 10
void fds_init_(struct pollfd fds[])
{
    int i=0;
    for(;i<MAXFD_;i++)
    {
        fds[i].fd=-1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }
}

void fds_add_(struct pollfd fds[],int fd)
{
    int i = 0;
    for(;i<MAXFD_;i++)
    {
        if(fds[i].fd==-1)
        {
            fds[i].fd = fd;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
            break;
        }
    }
}

void fds_del_(struct pollfd fds[],int i)
{
    fds[i].fd=-1;
    fds[i].events = 0;
    fds[i].revents = 0;
}

void epoll_add(int epfd,int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    if(epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev)==-1)
    {
        perror("epoll ctl add error");
    }
}

void epoll_del(int epfd,int fd)
{
    if(epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL)==-1)
    {
        perror("epoll ctl del error");
    }
}

void setnonblock(int fd)
{
    int oldfl = fcntl(fd,F_GETFL);
    int newfl = oldfl | O_NONBLOCK;
    if(fcntl(fd,F_SETFL,newfl)==-1)
    {
        perror("fcntl error");
    }
}

void epoll_add_(int epfd,int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;

    setnonblock(fd);
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev)==-1)
    {
        perror("epoll ctl add error");
    }
}

void epoll_del_(int epfd,int fd)
{
    if(epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL)==-1)
    {
        perror("epoll ctl del error");
    }
}

void signal_fun(int fd,short event,void *arg)
{
    if(event & EV_SIGNAL)
    {
        printf("sig=%d\n",fd);
    }
}

void timeout_fun(int fd,short event,void *arg)
{
    if(event & EV_TIMEOUT)
    {
        printf("time out\n");
    }
}

int main(int argc,char *argv[]) {

#if 0
    char *s = NULL;
    int n = 0;
    pid_t id = fork();
    assert(id!=-1);

    if(id==0)
    {
        s = "child";
        n = 3;
    }
    else
    {
        s = "parent";
        n = 7;
        int val = 0;
        wait(&val);
        if(WIFEXITED(val))
            printf("val=%d\n", WEXITSTATUS(val));
    }

    int i=0;
    for(;i<n;i++)
    {
        printf("s=%s,pid=%d,ppid=%d\n",s,getpid(),getppid());
        sleep(1);
    }
    exit(3);
#endif

#if 0
    int fpr = open("/home/xu/xjj.jpeg",O_RDONLY);
    int fpw = open("/home/xu/xjj2.jpg",O_WRONLY|O_CREAT,0600);

    if(fpr==-1||fpw==-1)
    {
        exit(0);
    }

    char buf[256] = {0};
    int num = 0;
    while((num = read(fpr,buf,256))>0)
    {
        write(fpw,buf,num);
    }
    close(fpr);
    close(fpw);
#endif

#if 0
    int fp = open("file.txt",O_WRONLY|O_CREAT,0600);
    assert(fp!=-1);

    printf("fp = %d\n",fp);
    write(fp,"wocao",5);
    close(fp);
#endif

#if 0
    int fp = open("file.txt",O_RDONLY);
    assert(fp!=-1);
    char buf[128] = {0};
    int n = read(fp,buf,127);
    printf("n = %d buf = %s\n",n,buf);
    close(fp);
#endif

#if 0
    printf("main pid = %d ppid=%d\n",getpid(),getppid());
    pid_t pid = fork();
    assert(pid!=-1);

    if(pid == 0)
    {
        printf("child pid = %d ppid=%d\n",getpid(),getppid());
        execl("/bin/ps","ps","-f",(char *)0);
        perror("error:");
        exit(0);
    }
    wait(NULL);
    exit(0);
#endif

#if 0
    int pid = 0;
    int sig = 0;
    sscanf(argv[1],"%d",&pid);
    sscanf(argv[2],"%d",&sig);

    if(kill(pid,sig)==-1)
    {
        perror("kill error:");
    }
    exit(0);
#endif

#if 0
    int fp = open("fifo",O_WRONLY);
    assert(fp!=-1);

    printf("fp=%d\n",fp);
    while (1)
    {
        printf("input:\n");
        char buf[128] = {0};
        fgets(buf,128,stdin);
        if(strncmp(buf,"end",3)==0)
        {
            break;
        }
        write(fp,buf,5);
    }
    close(fp);
#endif

#if 0
    int fp[2];
    assert(pipe(fp)!=-1);

    pid_t pid = fork();
    assert(pid!=-1);

    if(pid==0)
    {
        close(fp[1]);
        char buf[128] = {0};
        read(fp[0],buf,127);
        printf("child read:%s\n",buf);
        close(fp[0]);
    } else
    {
        close(fp[0]);
        write(fp[1],"hello",5);
        close(fp[1]);
    }
#endif

#if 0
    int i=0;
    sem_init();
    for(;i<5;i++) {
        sem_p();
        printf("A");
        fflush(stdout);
        int n = rand() % 3;
        sleep(n);
        printf("A");
        fflush(stdout);
        sem_v();
        n = rand() % 3;
        sleep(n);
    }
    sleep(10);
    sem_destory();
#endif

#if 0
    int shmid = shmget((key_t)1234,256,IPC_CREAT|0600);
    assert(shmid!=-1);

    char *s = (char*) shmat(shmid,NULL,0);
    assert(s!=(char*)-1);

    strcpy(s,"hello");
    shmdt(s);
#endif

#if 0
    struct mess{
        long type;
        char buf[128];
    };

    int msgid = msgget((key_t)1234,IPC_CREAT|0600);
    assert(msgid!=-1);

    struct mess data;
    data.type = 3;
    strcpy(data.buf,"hello3");
    msgsnd(msgid,&data,sizeof (data.type),0);
#endif

#if 0
    pthread_t id;
    pthread_create(&id,NULL,fun,NULL);

    int i=0;
    for(;i<5;i++)
    {
        printf("main run\n");
        sleep(1);
    }
    char *s = NULL;
    pthread_join(id,(void **)&s);
    printf("join:s=%s\n",s);
#endif

#if 0
    pthread_t id[5];
    pthread_mutex_init(&mut,NULL);
    int i=0;
    for(;i<5;i++)
    {
        pthread_create(&id[i],NULL,fun_,NULL);
    }
    for(i=0;i<5;i++)
    {
        pthread_join(id[i],NULL);
    }
    pthread_mutex_destroy(&mut);
#endif

#if 0
    pthread_t id;
    time_t begin,end;
    begin = time(NULL);
    pthread_create(&id,NULL,thread_fun,NULL);
    int i=0;
    for(;i<5;i++)
    {
        printf("main:pid%d\n",getpid());
        sleep(1);
    }
    end = time(NULL);
    printf("%ld",end-begin);
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    struct sockaddr_in caddr;
    while (1)
    {
        unsigned int len = sizeof(caddr);
        int c = accept(sockfd,(struct sockaddr*)&caddr,&len);
        if(c<0)
        {
            continue;
        }
        printf("accpet c=%d\n",c);
        while (1)
        {
            char buf[128] = {0};
            int n = recv(c,buf,127,0);
            if(n<=0)
            {
                break;
            }
            printf("buf=%s\n",buf);
            send(c,"ok",2,0);
        }
        close(c);
    }
#endif

#if 0
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd==-1)
    {
        return -1;
    }
    struct sockaddr_in saddr,caddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(6000);

    int res = bind(sockfd,(struct sockaddr*)&saddr,sizeof(saddr));
    assert(res!=-1);

    while (1)
    {
        unsigned int len = sizeof(caddr);
        char buf[128] = {0};
        recvfrom(sockfd,buf,127,0,(struct sockaddr*)&caddr,&len);
        printf("ip:%s,port:%d,buf=%s\n",
               inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port),buf);
        sendto(sockfd,"ok",2,0,(struct sockaddr*)&caddr, len);
    }
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",80);
    assert(sockfd!=-1);

    while (1)
    {
        struct sockaddr_in caddr;
        unsigned int len = sizeof(caddr);

        int c = accept(sockfd,(struct sockaddr*)&caddr,&len);
        if(c<0)
        {
            continue;
        }

        char buf[1024] = {0};
        int n = recv(c,buf,1023,0);
        printf("n=%d\n",n);
        printf("buf=%s\n",buf);

        char *s = strtok(buf," ");
        if(s==NULL)
        {
            close(c);
            continue;
        }
        printf("request method:\t%s\n",s);
        s = strtok(NULL," ");
        if(s==NULL)
        {
            close(c);
            continue;
        }

        printf("requested resource:\t%s\n",s);

        char path[128] = {"/home/xu/code/2"};
        strcat(path,s);

        int fd = open(path,O_RDONLY);
        if(fd==-1)
        {
            send(c,"404",3,0);
            close(c);
            continue;
        }

        Http(fd,c);
        Html(fd, c);
        close(fd);
        close(c);
    }
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    int fds[MAXFD];
    fds_init(fds);
//    memset(fds,-1, sizeof(fds));

    fds_add(fds,sockfd);
    fd_set fdset;

    while (1)
    {
        FD_ZERO(&fdset);
        int maxfd = -1;

        int i=0;
        for(;i<MAXFD;i++)
        {
            if(fds[i]!=-1)
            {
                FD_SET(fds[i],&fdset);
                if(maxfd<fds[i])
                {
                    maxfd = fds[i];
                }
            }
        }

        struct timeval tv = {5,0};

        int n = select(maxfd+1,&fdset,NULL,NULL,&tv);
        if(n==-1)
        {
            printf("select error\n");
            continue;
        }
        else if(n==0)
        {
            printf("time out\n");
            continue;
        } else
        {
            for(i=0;i<MAXFD;i++)
            {
                if(fds[i]==-1)
                {
                    continue;
                }
                if(FD_ISSET(fds[i],&fdset))
                {
                    if(fds[i]==sockfd)
                    {
                        new_accpet(fds,sockfd);
                    } else
                    {
                        accpet_data(fds,i);
                    }
                }
            }
        }
    }
#endif

#if 0
    int fd = STDIN;
    fd_set fdset;

    while (1)
    {
        FD_ZERO(&fdset);
        FD_SET(fd,&fdset);

        struct timeval tv = {5,0};
        int n = select(fd+1,&fdset,NULL,NULL,&tv);
        if(n==-1)
        {
            printf("select error\n");
            continue;
        } else if(n==0)
        {
            printf("time out\n");
            continue;
        } else
        {
            if(FD_ISSET(fd,&fdset))
            {
                char buf[128] = {0};
                read(fd,buf,127);
                printf("read:%s\n",buf);
            }
        }
    }
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    struct pollfd fds[MAXFD_];
    fds_init_(fds);

    fds_add_(fds,sockfd);

    while (1)
    {
        int n= poll(fds,MAXFD_,5000);
        if(n==-1)
        {
            continue;
        } else if(n==0)
        {
            printf("time out\n");
            continue;
        } else
        {
            int i = 0;
            for (;i<MAXFD_;i++)
            {
                if(fds[i].fd==-1)
                {
                    continue;
                }
                if(fds[i].revents & POLLIN)
                {
                    if(fds[i].fd == sockfd)
                    {
                        struct sockaddr_in caddr;
                        unsigned int len = sizeof (caddr);
                        int c = accept(sockfd,(struct sockaddr*)&caddr,&len);
                        if(c<0)
                        {
                            continue;
                        }
                        printf("accept c=%d\n",c);
                        fds_add_(fds,c);
                    } else
                    {
                        char buf[128] = {0};
                        int num = recv(fds[i].fd,buf,127,0);
                        if(num<=0)
                        {
                            close(fds[i].fd);
                            fds_del_(fds,i);
                            printf("cilent close\n");
                            continue;
                        }
                        printf("buf(%d)=%s\n",fds[i].fd,buf);
                        send(fds[i].fd,"ok",2,0);
                    }
                }
            }
        }
    }
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    int epfd = epoll_create(MAXFD_);
    assert(epfd!=-1);

    epoll_add(epfd,sockfd);

    struct epoll_event evs[MAXFD_];

    while (1)
    {
        int n = epoll_wait(epfd,evs,MAXFD_,5000);
        if(n==-1)
        {
            perror("epoll wait error");
            continue;
        } else if(n==0)
        {
            printf("time out\n");
            continue;
        } else
        {
            int i = 0;
            for(;i<n;i++)
            {
                int fd = evs[i].data.fd;

                if(evs[i].events&EPOLLIN)
                {
                    if(fd==sockfd)
                    {
                        struct sockaddr_in caddr;
                        unsigned int len = sizeof(caddr);
                        int c = accept(fd,(struct sockaddr*)&caddr,&len);
                        if(c<0)
                        {
                            continue;
                        }
                        printf("accept c=%d\n",c);
                        epoll_add(epfd,c);
                    } else
                    {
                        char buf[128] = {0};
                        int num = recv(fd,buf,127,0);
                        if(num<=0)
                        {
                            epoll_del(epfd,fd);
                            close(fd);
                            printf("client close\n");
                            continue;
                        } else
                        {
                            printf("buf(%d)=%s\n",fd,buf);
                            send(fd,"ok",2,0);
                        }
                    }
                }
            }
        }
    }
#endif

#if 0
    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    int epfd = epoll_create(MAXFD_);
    assert(epfd!=-1);

    epoll_add_(epfd,sockfd);

    struct epoll_event evs[MAXFD_];

    while (1)
    {
        int n = epoll_wait(epfd,evs,MAXFD_,5000);
        if(n==-1)
        {
            perror("epoll wait error");
            continue;
        } else if(n==0)
        {
            printf("time out\n");
            continue;
        } else
        {
            int i = 0;
            for(;i<n;i++)
            {
                int fd = evs[i].data.fd;

                if(evs[i].events&EPOLLIN)
                {
                    if(fd==sockfd)
                    {
                        struct sockaddr_in caddr;
                        unsigned int len = sizeof(caddr);
                        int c = accept(fd,(struct sockaddr*)&caddr,&len);
                        if(c<0)
                        {
                            continue;
                        }
                        printf("accept c=%d\n",c);
                        epoll_add_(epfd,c);
                    } else
                    {
                        while (1)
                        {
                            char buf[128] = {0};
                            int num = recv(fd,buf,1,0);
                            if(num == -1)
                            {
                                if(errno != EAGAIN && errno != EWOULDBLOCK)
                                {
                                    perror("recv error:");
                                } else
                                {
                                    send(fd,"ok",2,0);
                                }
                                break;
                            }
                            else if(num==0)
                            {
                                epoll_del_(epfd,fd);
                                close(fd);
                                printf("client close\n");
                                break;
                            } else
                            {
                                printf("rev:%s\n",buf);
                            }
                        }
                    }
                }
            }
        }
    }

#endif

#if 0
    struct event_base *base = event_init();
    assert(base != NULL);

    struct event *sig_ev = evsignal_new(base,SIGINT,signal_fun,NULL);
//    struct event *sig_ev = event_new(base,SIGINT,EV_SIGNAL|EV_PERSIST,signal_fun,NULL);
    assert(sig_ev!=NULL);
    event_add(sig_ev,NULL);

    struct event *timeout_ev = evtimer_new(base,timeout_fun,NULL);
//    struct event *timeout_ev = event_new(base,-1,EV_TIMEOUT,timeout_fun,NULL);
    struct timeval tv = {3,0};
    event_add(timeout_ev,&tv);

    event_base_dispatch(base);
    event_free(sig_ev);
    event_free(timeout_ev);
    event_base_free(base);
#endif

    int sockfd = create_socket("127.0.0.1",6000);
    assert(sockfd!=-1);

    struct event_base *base = event_init();
    assert(base!=NULL);

    struct event *sock_ev = event_new(base,sockfd,EV_READ|EV_PERSIST,accpet_fun,base);
    assert(sock_ev!=NULL);
    event_add(sock_ev,NULL);

    event_base_dispatch(base);
    event_free(sock_ev);
    event_base_free(base);
    exit(0);
}

