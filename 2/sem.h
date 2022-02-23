//
// Created by xu on 2022/2/17.
//

#ifndef INC_2_SEM_H
#define INC_2_SEM_H

#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
union semun
{
    int val;
};

void sem_init();
void sem_p();
void sem_v();
void sem_destory();

#endif //INC_2_SEM_H
