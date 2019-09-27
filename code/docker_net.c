// 添加 CLONE_NEWUSER
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)

static char container_stack[STACK_SIZE];
char *const container_args[] = {
    "/bin/bash",
    NULL};

int pipefd[2];

void set_map(char *file, int inside_id, int outside_id, int len)
{
    FILE *mapfd = fopen(file, "w");
    if (NULL == mapfd)
    {
        perror("open file error");
        return;
    }
    fprintf(mapfd, "%d %d %d", inside_id, outside_id, len);
    fclose(mapfd);
}

void set_uid_map(pid_t pid, int inside_id, int outside_id, int len)
{
    char file[256];
    sprintf(file, "/proc/%d/uid_map", pid);
    set_map(file, inside_id, outside_id, len);
}

void set_gid_map(pid_t pid, int inside_id, int outside_id, int len)
{
    char file[256];
    sprintf(file, "/proc/%d/gid_map", pid);
    set_map(file, inside_id, outside_id, len);
}
void child_set_net() {

    // 激活 namespace 中的 loopback
    system("ip link set dev lo up", 0);

    // 修改设备名字
    system("ip link set dev in name eth0");

    // 分配 ip
    system("ifconfig eth0 172.17.0.100/24 up");

    // 为容器增加一个路由规则，让容器可以访问外面的网络
    system("ip route add default via 172.17.0.10");
}


void sys(const char *command, int net_namespace)
{
    char buf[255];
    sprintf(buf, command, net_namespace);
    system(buf);
}

void set_net(pid_t container_pid)
{
    // 增加一个 pair 虚拟网卡，注意其中的veth类型，其中一个网卡要按进容器中
    sys("ip link add out type veth peer name in", 0);

    // 把 in 按到 namespace 中，这样容器中就会有一个新的网卡了
    sys("ip link set in netns %d", container_pid);

    // ip
    system("ifconfig out 172.17.0.10");

    // 上面我们把 in 这个网卡按到了容器中，然后我们要把 out 添加上网桥上
    system("brctl addif docker0 out");
}

int container_main(void *arg)
{

    // PID namespaces
    /* 等待父进程通知后再往下执行（进程间的同步） pid 的映射 */
    char ch;
    // 关闭写端
    close(pipefd[1]);
    // 父进程关闭写端的时候，子进程就不会阻塞了
    read(pipefd[0], &ch, 1);

    printf("Container [%5d] - inside the container!\n", getpid());

    // Mount namespaces
    //remount "/proc" to make sure the "top" and "ps" show container's information
    if (mount("proc", "rootfs/proc", "proc", 0, NULL) != 0)
    {
        perror("proc");
    }
    if (mount("sysfs", "rootfs/sys", "sysfs", 0, NULL) != 0)
    {
        perror("sys");
    }
    if (mount("none", "rootfs/tmp", "tmpfs", 0, NULL) != 0)
    {
        perror("tmp");
    }
    if (mount("udev", "rootfs/dev", "devtmpfs", 0, NULL) != 0)
    {
        perror("dev");
    }
    if (mount("devpts", "rootfs/dev/pts", "devpts", 0, NULL) != 0)
    {
        perror("dev/pts");
    }
    if (mount("shm", "rootfs/dev/shm", "tmpfs", 0, NULL) != 0)
    {
        perror("dev/shm");
    }
    if (mount("tmpfs", "rootfs/run", "tmpfs", 0, NULL) != 0)
    {
        perror("run");
    }
    /* 
     * 模仿Docker的从外向容器里mount相关的配置文件 
     * 你可以查看：/var/lib/docker/containers/<container_id>/目录，
     * 你会看到docker的这些文件的。
     */
    if (mount("conf/hosts", "rootfs/etc/hosts", "none", MS_BIND, NULL) != 0 ||
        mount("conf/hostname", "rootfs/etc/hostname", "none", MS_BIND, NULL) != 0 ||
        mount("conf/resolv.conf", "rootfs/etc/resolv.conf", "none", MS_BIND, NULL) != 0)
    {
        perror("conf");
    }
    /* 模仿docker run命令中的 -v, --volume=[] 参数干的事 */
    if (mount("/tmp/t1", "rootfs/mnt", "none", MS_BIND, NULL) != 0)
    {
        perror("mnt");
    }
    /* chroot 隔离目录 */
    if (chdir("./rootfs") != 0 || chroot("./") != 0)
    {
        perror("chdir/chroot");
    }

    // UTS namespaces
    // set hostname
    sethostname("mydocker_ts", 10);
    child_set_net();

    execv(container_args[0], container_args);
    perror("exec");
    printf("Something's wrong!\n");
    return 1;
}

int main()
{
    const int gid = getgid(), uid = getuid();

    printf("Parent: eUID = %ld;  eGID = %ld, UID=%ld, GID=%ld\n",
           (long)geteuid(), (long)getegid(), (long)getuid(), (long)getgid());

    pipe(pipefd);

    printf("Parent [%5d] - start a container!\n", getpid());

    // 要启动IPC隔离，我们只需要在调用 clone 时加上 CLONE_NEWIPC 参数就可以了。
    int container_pid = clone(container_main, container_stack + STACK_SIZE,
                              CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWNET | SIGCHLD, NULL);

    printf("Parent [%5d] - Container [%5d]!\n", getpid(), container_pid);

    //To map the uid/gid,
    //   we need edit the /proc/PID/uid_map (or /proc/PID/gid_map) in parent
    //The file format is
    //   ID-inside-ns   ID-outside-ns   length
    //if no mapping,
    //   the uid will be taken from /proc/sys/kernel/overflowuid
    //   the gid will be taken from /proc/sys/kernel/overflowgid
    set_uid_map(container_pid, 0, uid, 1);
    set_gid_map(container_pid, 0, gid, 1);

    /* network namespace */
    set_net(container_pid);

    printf("Parent [%5d] - user/group mapping done!\n", getpid());

    /* 通知子进程 */
    close(pipefd[1]);
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}