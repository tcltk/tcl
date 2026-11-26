#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define N 1000000
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
int main(){double bc=1e9,bk=1e9,bn=1e9;long r=0;
for(int it=0;it<5;it++){double t=now();unsigned char*s=malloc(N);memset(s,1,N);double t1=now();
s[0]=s[1]=0;for(long i=2;i*i<N;i++)if(s[i])for(long j=i*i;j<N;j+=i)s[j]=0;double t2=now();
long c=0;for(long i=0;i<N;i++)c+=s[i];double t3=now();r=c;
if(t1-t<bc)bc=t1-t;if(t2-t1<bk)bk=t2-t1;if(t3-t2<bn)bn=t3-t2;free(s);}
printf("%-32s %7.1f %7.1f %7.1f %7.1f -> %ld\n","C (gcc -O2)",bc,bk,bn,bc+bk+bn,r);}
