#include <stdio.h>
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
int main() {
#if 0
    int fp = open("/home/xu/code/2/cmake-build-debug/fifo",O_RDONLY);
    assert(fp!=-1);

    printf("fp=%d\n",fp);
    while (1)
    {
        char buf[128]={0};
        if(read(fp,buf,127)==0)
        {
            break;
        }
        printf("read:%s\n",buf);
    }
    close(fp);
#endif

#if 0
    int shmid = shmget((key_t)1234,256,IPC_CREAT|0600);
    assert(shmid!=-1);

    char *s = (char*) shmat(shmid,NULL,0);
    assert(s!=(char *)-1);

    printf("%s\n",s);
    shmdt(s);
#endif

#if 0
    struct mess
    {
        long type;
        char buf[128];
    };

    int msgid = msgget((key_t)1234,IPC_CREAT|0600);
    assert(msgid!=-1);

    struct mess data;
    msgrcv(msgid,&data,128,0,0);
    printf("read mess:%s\n",data.buf);
#endif

#if 1
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd==-1)
    {
        return -1;
    }
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(6000);

    int res = connect(sockfd,(struct sockaddr*)&saddr, sizeof(saddr));
    assert(res!=-1);

    while (1)
    {
        printf("input:\n");
        char buf[128] = {0};
        fgets(buf,127,stdin);
        if(strncmp(buf,"end",3)==0)
        {
            break;
        }
        send(sockfd,buf, strlen(buf),0);
        memset(buf,0,128);
        recv(sockfd,buf,127,0);
        printf("read:%s\n",buf);
    }
    close(sockfd);
#endif

#if 0
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd==-1)
    {
        return -1;
    }
    struct sockaddr_in saddr;
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    saddr.sin_port = htons(6000);

    while (1)
    {
        char buf[128] = {0};
        printf("input:\n");
        scanf("%s",buf);
        if(strcmp(buf,"end")==0)
        {
            break;
        }
        sendto(sockfd,buf, strlen(buf),0,(struct sockaddr*)&saddr, sizeof(saddr));
        memset(buf,0,128);
        unsigned int len = sizeof(saddr);
        recvfrom(sockfd,buf,127,0,(struct sockaddr*)&saddr,&len);
        printf("buf=%s\n",buf);
    }
    close(sockfd);
#endif
}
