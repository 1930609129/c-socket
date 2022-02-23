#include <stdio.h>

int add(int x,int y)
{
    int tmp=0;
    tmp = x+y;
    return tmp;
}
int main()
{
    int a=2,b=3;
    int d = add(a,b);
    printf("%d\n",d);
}
