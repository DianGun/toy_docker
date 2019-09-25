# docker 的基础技术(上)



> 看了「陈皓大佬」的***[文章](https://coolshell.cn/articles/17010.html)***，跟着「陈皓大佬」的文章，我们一起来实现一个 toy_docker。文章中的大部分内容来自「陈皓大佬」***[文章](https://coolshell.cn/articles/17010.html)***。我仅仅是为了学习与记忆，重新写了一遍。



## 简介



docker 的本质是**隔离**



举个简单的例子:



我们知道 Linux 有一个超级父进程，它的的 PID 为 1，如果我们能使一个进程的所有子进程认为它的父进程的 PID 是 1，那么最简单的隔离不就实现了吗？



当然这种隔离还远远不够，Linux Namespace 提供了对 UTS、IPC、mount、PID、network、User 等隔离机制。



| Namespace | 系统调用参数  | 隔离内容                   |
| --------- | ------------- | -------------------------- |
| UTS       | CLONE_NEWUTS  | 主机名与域名               |
| IPC       | CLONE_NEWIPC  | 信号量、消息队列和共享内存 |
| PID       | CLONE_NEWPID  | 进程编号                   |
| Mount     | CLONE_NEWNS   | 挂载点（文件系统）         |
| Network   | CLONE_NEWNET  | 网络设备、网络栈、端口等等 |
| User      | CLONE_NEWUSER | 用户和用户组               |



我们将通过三个函数:



+ clone() 实现线程的系统调用，用来创建一个新的进程，并可以通过设计上述参数达到隔离。
+ unshare() 使某进程脱离某个 namespace
+ setns() 把某进程加入到某个 namespace



实现**上述隔离**





## 一个 clone() 的例子





 ```c
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
 
/* 定义一个给 clone 用的栈，栈大小1M */
#define STACK_SIZE (1024 * 1024)
static char container_stack[STACK_SIZE];
 
char* const container_args[] = {
    "/bin/bash",
    NULL
};
 
int container_main(void* arg)
{
    printf("Container - inside the container!\n");
    /* 直接执行一个shell，以便我们观察这个进程空间里的资源是否被隔离了 */
    execv(container_args[0], container_args); 
    printf("Something's wrong!\n");
    return 1;
}
int main()
{
    printf("Parent - start a container!\n");
    /*
    	int clone(int (*fn)(void *), void *child_stack, int flags, void *arg);
    	调用clone函数，其中传出一个函数，还有一个栈空间的（为什么传尾指针，因为栈是反着的）
    	栈空间的基地址是高地址
    */
    int container_pid = clone(container_main, container_stack+STACK_SIZE, SIGCHLD, NULL);
    /* 等待子进程结束 */
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}
 ```



```shell
gcc clone.c -o clone -g
./clone
```



这仅仅是一个与 clone 的用法有关的例子，并没有什么特别之处。**父子进程的进程空间是没有什么差别的，父进程能访问到的子进程也能**。





接下来我们介绍 UTS Namespace



## UTS Namespace



从上表中，我们可以看到，UTS Namespace 主要用来隔离「主机名与域名」





用法如下（通过修改 clone 的参数）：





```c
int container_main(void* arg)
{
    printf("Container - inside the container!\n");
    sethostname("container",10); /* 设置hostname */
    execv(container_args[0], container_args);
    printf("Something's wrong!\n");
    return 1;
}
 
int main()
{
    printf("Parent - start a container!\n");
    int container_pid = clone(container_main, container_stack+STACK_SIZE, 
    // 注意这里需要 root 权限！！！                          
    CLONE_NEWUTS | SIGCHLD, NULL); /*启用CLONE_NEWUTS Namespace隔离 */
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}
```



编译运行后发现子进程的 hostname 与父进程的 hostname 不一样。



```shell
➜  code git:(master) ✗ gcc uts.c -o uts.out
➜  code git:(master) ✗ sudo ./uts.out
Parent - start a container!
container_pid 25506 
Container - inside the container!
root@container:~/Desktop/toy_docker/code# hostname
container
```



## IPC Namespace



IPC 全称 Inter-Process Communication，是 Unix/Linux 下进程通信的方法。显然，容器内与容器外的进程不能相互通信。所以我们必须将 IPC 隔离开，这样只有，在同一个 Namespace 下的进程才能相互通信。





**要启动 IPC 隔离，我们只需要在调用 clone 时加上 CLONE_NEWIPC 参数就可以了**

```c
int container_pid = clone(container_main, container_stack+STACK_SIZE, 
            CLONE_NEWUTS | CLONE_NEWIPC | SIGCHLD, NULL);
```







由于 IPC 的原理我不是很熟，**所以放弃陈皓大佬的相关叙述，会在其他的文章中好好学习一下消息队列**。





## PID Namespace



现在我们要实现当前进程的 ID 是 1，也就是超级父进程的 ID，实现也很简单（加上 CLONE_NEWPID）：



```c
    int container_pid = clone(container_main, container_stack+STACK_SIZE, 
            CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD, NULL); 
```



运行结果如下：



```shell
➜  code git:(master) ✗ sudo ./pid.out 
[sudo] tenshine 的密码： 
Parent - start a container!
container_pid 26866 
Container - inside the container!
root@container:~/Desktop/toy_docker/code# echo $$
1
```





但是，我们还是能看到其他的“容器”以外的进程。说明并没有完全隔离。这是因为，像 ps, top 这些命令会去读 /proc 文件系统，所以，因为 /proc 文件系统在父进程和子进程都是一样的，所以这些命令显示的东西都是一样的。



所以，我们还需要对**文件系统**进行隔离。





## Mount Namespace



我们要对重要的**文件系统**进行隔离，先拿上面那个例子开刀：



```c
int container_main(void* arg)
{
    printf("Container [%5d] - inside the container!\n", getpid());
    sethostname("container",10);
    /* 重新mount proc文件系统到 /proc下 */
    system("mount -t proc proc /proc");
    execv(container_args[0], container_args);
    printf("Something's wrong!\n");
    return 1;
}
 
int main()
{
    printf("Parent [%5d] - start a container!\n", getpid());
    /* 启用 Mount Namespace - 增加CLONE_NEWNS参数 */
    int container_pid = clone(container_main, container_stack+STACK_SIZE, 
            CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, NULL);
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}
```



运行结果如下：



```shell
➜  code git:(master) ✗ ./mount.out
Parent [ 6443] - start a container!
Container [    1] - inside the container!
root@container:~/Desktop/toy_docker/code# echo $$
1
root@container:~/Desktop/toy_docker/code# ps
  PID TTY          TIME CMD
    1 pts/0    00:00:00 bash
   11 pts/0    00:00:00 ps

```



上面，我们可以看到只有两个进程 ，而且 pid=1 的进程是我们的 /bin/bash。





这里多说一下，在通过 CLONE_NEWNS 创建 mount namespace 后，父进程会把自己的文件结构复制给子进程中。而子进程中新的 namespace 中的所有 **mount 操作都只影响自身的文件系统，而不对外界产生任何影响。这样可以做到比较严格地隔离**。





显然！我们还有其他的文件系统需要 mount。





## 模仿 docker 的文件系统



除了 /proc 需要重新挂载，显然其他的文件系统也需要重新挂载。**在这里我们模仿 docker 的文件系统**！



首先，我们需要一个 rootfs，也就是我们需要把我们要做的镜像中的那些命令什么的 copy 到一个 rootfs 的目录下，我们模仿 Linux 构建如下的目录：



```shell
➜  code git:(master) ✗ cd rootfs 
➜  rootfs git:(master) ✗ ls
bin  etc   lib    mnt  proc  run   sys  usr
dev  home  lib64  opt  root  sbin  tmp  var
```



然后我的 lib 中有：



```shell
➜  rootfs git:(master) ✗ cd lib
➜  lib git:(master) ✗ ls
x86_64-linux-gnu
➜  lib git:(master) ✗ cd x86_64-linux-gnu 
➜  x86_64-linux-gnu git:(master) ✗ ls
ld-2.27.so                 libnewt.so.0.52.20
ld-linux-x86-64.so.2       libnih.so.1
libacl.so.1                libnih.so.1.0.0
libacl.so.1.1.0            libnl-3.so.200
libaio.so.1                libnl-3.so.200.24.0
...
```



直接从本机 lib 中拿出来的





lib64 中有：



```shell
➜  rootfs git:(master) ✗ cd lib64 
➜  lib64 git:(master) ✗ ls
ld-linux-x86-64.so.2
```



也是直接从本机拿过来的。





最后为了模仿 docker 我们可以打开`cd /var/lib/docker/containers/<id>`



得到以下配置（conf 和 rootfs）在同一个目录下：

```shell
➜  conf git:(master) ✗ ls
hostname  hosts  resolv.conf
```



最后我们回到我们的代码上，挂载目录：



```c
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
 
#define STACK_SIZE (1024 * 1024)
 
static char container_stack[STACK_SIZE];
char* const container_args[] = {
    "/bin/bash",
    "-l",
    NULL
};
 
int container_main(void* arg)
{
    printf("Container [%5d] - inside the container!\n", getpid());
 
    //set hostname
    sethostname("container",10);
 
    //remount "/proc" to make sure the "top" and "ps" show container's information
    if (mount("proc", "rootfs/proc", "proc", 0, NULL) !=0 ) {
        perror("proc");
    }
    if (mount("sysfs", "rootfs/sys", "sysfs", 0, NULL)!=0) {
        perror("sys");
    }
    if (mount("none", "rootfs/tmp", "tmpfs", 0, NULL)!=0) {
        perror("tmp");
    }
    if (mount("udev", "rootfs/dev", "devtmpfs", 0, NULL)!=0) {
        perror("dev");
    }
    if (mount("devpts", "rootfs/dev/pts", "devpts", 0, NULL)!=0) {
        perror("dev/pts");
    }
    if (mount("shm", "rootfs/dev/shm", "tmpfs", 0, NULL)!=0) {
        perror("dev/shm");
    }
    if (mount("tmpfs", "rootfs/run", "tmpfs", 0, NULL)!=0) {
        perror("run");
    }
    /* 
     * 模仿Docker的从外向容器里mount相关的配置文件 
     * 你可以查看：/var/lib/docker/containers/<container_id>/目录，
     * 你会看到docker的这些文件的。
     */
    if (mount("conf/hosts", "rootfs/etc/hosts", "none", MS_BIND, NULL)!=0 ||
          mount("conf/hostname", "rootfs/etc/hostname", "none", MS_BIND, NULL)!=0 ||
          mount("conf/resolv.conf", "rootfs/etc/resolv.conf", "none", MS_BIND, NULL)!=0 ) {
        perror("conf");
    }
    /* 模仿docker run命令中的 -v, --volume=[] 参数干的事 */
    if (mount("/tmp/t1", "rootfs/mnt", "none", MS_BIND, NULL)!=0) {
        perror("mnt");
    }
 
    /* chroot 隔离目录 */
    if ( chdir("./rootfs") != 0 || chroot("./") != 0 ){
        perror("chdir/chroot");
    }
 
    execv(container_args[0], container_args);
    perror("exec");
    printf("Something's wrong!\n");
    return 1;
}
 
int main()
{
    printf("Parent [%5d] - start a container!\n", getpid());
    int container_pid = clone(container_main, container_stack+STACK_SIZE, 
            CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, NULL);
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}
```





这样就完成了一个简单的镜像！是不是很简单！！！陈皓大佬真的太牛逼了！




