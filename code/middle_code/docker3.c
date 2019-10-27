// 添加 CLONE_NEWUSER
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

int container_main(void *arg)
{
    printf("Container [%5d] - inside the container!\n", getpid());

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

    /* 等待父进程通知后再往下执行（进程间的同步） */
    char ch;
    // 关闭写端
    close(pipefd[1]);
    // 父进程关闭写端的时候，子进程就不会阻塞了
    read(pipefd[0], &ch, 1);

    //set hostname
    sethostname("container", 10);

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

    int container_pid = clone(container_main, container_stack + STACK_SIZE,
                              CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWNET | SIGCHLD, NULL);

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

    printf("Parent [%5d] - user/group mapping done!\n", getpid());

    /* 通知子进程 */
    close(pipefd[1]);
    waitpid(container_pid, NULL, 0);
    printf("Parent - container stopped!\n");
    return 0;
}