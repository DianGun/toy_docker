# DOCKER基础技术：LINUX CGROUP





> 前言：本文的大部分内容来自 [coolshell](https://coolshell.cn/articles/17049.html)，我仅仅处于学习与理解的目的，将文章重写了一遍





## 0X00 简介 什么是 CGROUP

在之前实现的 toy_docker 中，我们仅仅只是做到了一个环境的隔离，这还远远不够，除了环境的隔离，还要做到资源的限制。



通过 CGROUP 就能完成类似这样的功能。



当然除了资源的限制，CGROUP 还提供以下功能：



- **Resource limitation**: 限制资源使用，比如内存使用上限以及文件系统的缓存限制。
- **Prioritization**: 优先级控制，比如：CPU利用和磁盘IO吞吐。
- **Accounting**: 一些审计或一些统计，主要目的是为了计费。
- **Control**: 挂起进程，恢复执行进程。





那么 CGROUP 到底是怎么干的，我们先来点「感性」的认识



## 0X01 如何利用 CGROUP 



首先，Linux把CGroup这个事实现成了一个file system，你可以 mount。在我的Ubuntu 18.04下，你输入以下命令你就可以看到 cgroup 已为你 mount 好了。





```shell
➜  ~ mount -t cgroup
cgroup on /sys/fs/cgroup/systemd type cgroup (rw,nosuid,nodev,noexec,relatime,xattr,name=systemd)
cgroup on /sys/fs/cgroup/net_cls,net_prio type cgroup (rw,nosuid,nodev,noexec,relatime,net_cls,net_prio)
cgroup on /sys/fs/cgroup/pids type cgroup (rw,nosuid,nodev,noexec,relatime,pids)
cgroup on /sys/fs/cgroup/memory type cgroup (rw,nosuid,nodev,noexec,relatime,memory)
```



你可以到 /sys/fs/cgroup 的各个子目录下去 make 个 dir，你会发现，一旦你创建了一个子目录，这个子目录里又有很多文件了



```shell
➜  cpu mkdir aa
➜  aa ls
cgroup.clone_children  cpuacct.usage_percpu_sys   cpu.shares
cgroup.procs           cpuacct.usage_percpu_user  cpu.stat
cpuacct.stat           cpuacct.usage_sys          notify_on_release
cpuacct.usage          cpuacct.usage_user         tasks
cpuacct.usage_all      cpu.cfs_period_us
cpuacct.usage_percpu   cpu.cfs_quota_us
```



好了，我们来看几个示例。



### CPU 的限制



假设，我们有一个非常吃 CPU 的程序，叫 deadloop，其源码如下



```c
int main(void)
{
    int i = 0;
    for(;;) i++;
    return 0;
}
```



用 sudo 执行起来后，毫无疑问，CPU 被干到了 100%（下面是 top 命令的输出）

```shell
18626 tenshine  20   0    4380    748    680 R 100.0  0.0   0:28.53 cpu 
```



然后，我们这前不是在 /sys/fs/cgroup/cpu 下创建了一个 aa 的 group。我们先设置一下这个 group 的 cpu 利用的限制：



```shell
➜  aa cat cpu.cfs_quota_us
-1
➜  aa echo 20000 > cpu.cfs_quota_us
➜  aa cat cpu.cfs_quota_us
20000
```

我们看到，这个进程的 PID 是 19213，我们把这个进程加到这个 cgroup 中：



```shell
aa echo 19213 > tasks
```



cpu 的利用率立刻下降：



```shell
19213 tenshine  20   0    4380    776    708 R  20.5  0.0   1:04.09 cpu    
```

​    



下面是一个线程的例子（没有运行成功）：





```c
#define _GNU_SOURCE         /* See feature_test_macros(7) */
 
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
 
 
const int NUM_THREADS = 5;
 
void *thread_main(void *threadid)
{
    /* 把自己加入cgroup中（syscall(SYS_gettid)为得到线程的系统tid） */
    char cmd[128];
    sprintf(cmd, "echo %ld >> /sys/fs/cgroup/cpu/haoel/tasks", syscall(SYS_gettid));
    system(cmd); 
    sprintf(cmd, "echo %ld >> /sys/fs/cgroup/cpuset/haoel/tasks", syscall(SYS_gettid));
    system(cmd);
 
    long tid;
    tid = (long)threadid;
    printf("Hello World! It's me, thread #%ld, pid #%ld!\n", tid, syscall(SYS_gettid));
     
    int a=0; 
    while(1) {
        a++;
    }
    pthread_exit(NULL);
}
int main (int argc, char *argv[])
{
    int num_threads;
    if (argc > 1){
        num_threads = atoi(argv[1]);
    }
    if (num_threads<=0 || num_threads>=100){
        num_threads = NUM_THREADS;
    }
 
    /* 设置CPU利用率为50% */
    mkdir("/sys/fs/cgroup/cpu/haoel", 755);
    system("echo 50000 > /sys/fs/cgroup/cpu/haoel/cpu.cfs_quota_us");
 
    mkdir("/sys/fs/cgroup/cpuset/haoel", 755);
    /* 限制CPU只能使用#2核和#3核 */
    system("echo \"2,3\" > /sys/fs/cgroup/cpuset/haoel/cpuset.cpus");
 
    pthread_t* threads = (pthread_t*) malloc (sizeof(pthread_t)*num_threads);
    int rc;
    long t;
    for(t=0; t<num_threads; t++){
        printf("In main: creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, thread_main, (void *)t);
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
 
    /* Last thing that main() should do */
    pthread_exit(NULL);
    free(threads);
}
```







### 内存使用限制









### 磁盘I/O限制







## CGROUP 的子系统





## CGroup的术语



