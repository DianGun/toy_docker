# DOCKER基础技术：AUFS





> 本文的大部分内容来自 [coolshell](https://coolshell.cn/articles/17061.html)，我只是为了理解与学习，将这篇内容重新写了一遍



## 0X00 简介 AUFS 是什么



AUFS 是一种 Union FIle System，可以用来把不同物理位置的东西合并到同一个目录下面（后面会给具体例子！），为了显示自己很牛逼，A 从原来的 Another 变成了 之后的 Advance





**接下来通过一个例子学习 AUFS**



现在我们有三个文件夹（如下所示），mnt 是空文件夹：



```shell
➜  mount : ✗ tree
.
├── A
│   ├── a
│   └── c
├── B
│   ├── b
│   └── c
└── mnt
```



现在我们通过 aufs 将 A B 合并在一起：

```shell
mount git:(master) ✗ sudo mount -t aufs -o br=./A:./B none ./mnt
```

现在再看 mnt 文件夹：



```shell
➜  mnt ls
a  b  c
```



这就是 AUFS 啦！



## 0X01 AUFS 的一些小疑问



在 0X00 中你会发现两个文件夹中有一个文件名字一样：



在多次测试后我们会发现：



`可见，如果有重复的文件名，在mount命令行上，越往前的就优先级越高`





## 0X02 如何使用——AUFS 的几个例子



首先来看最基础的用法：



```shell
 mount -t aufs -o br=./A:./B none ./mnt
```



前面的 -t -o 基本不变，br 就是不同的目录，中间用 : 分隔。none 是表示没有任何与之相关的设备





### 添加每个分支的权限



可在每个分支的后面添加=<权限>，比如：



```shell
mount -t aufs -o br=./A=rw:./B none ./mnt
```





- rw 表示可写可读 read-write。
- ro 表示 read-only，如果你不指权限，那么除了第一个外 ro 是默认值，对于 ro 分支，其永远不会收到写操作，也不会收到查找 whiteout 的操作。
- rr 表示 real-read-only，与 read-only 不同的是，rr 标记的是天生就是只读的分支，这样，AUFS 可以提高性能，比如不再设置 inotify 来检查文件变动通知。



### 解释 whiteout



whiteout 就是隐藏文件的意思。



举个例子：



```shell
➜  mount git:(master) ✗ tree
.
├── A
│   ├── a
│   └── c
├── B
│   ├── b
│   └── c
└── mnt
```



现在要给 mnt 联合挂载：



```shell
➜  mount git:(master) ✗ sudo mount -t aufs -o br=./A=rw:./B=ro none ./mnt
```



如果删除掉 b，但是 B 是只读的，会发生什么？



```shell
➜  mnt rm b
➜  mnt ls
a  c
```



b 没有了，但是 B 里面还是有 b。



这个操作的本质是：



**在上层的可写的目录下建立对应的 whiteout 隐藏文件来实现的**。



所以 rm b 等价于：



现在我们在权限为 rw 的 test 目录下建个 whiteout 的隐藏文件 .wh.b，你就会发现 ./mnt/b这个文件就消失了：



```shell
➜  mnt ls
a  b  c
➜  A touch  .wh.b
➜  mount cd mnt 
➜  mnt ls
a  c
```



这就是 whiteout。





## 0X03  一些高级操作



### 通过添加 **udba** 参数选择是否监听源文件



+ **udba=none** – 设置上这个参数后，AUFS 会运转的更快，因为那些不在 mount 目录里发生的修改，aufs 不会同步过来了，所以会有数据出错的问题。



+ **udba=reval** – 设置上这个参数后，AUFS 会去查文件有没有被更新，如果有的话，就会把修改拉到 mount 目录内。



+ **udba=notify** – 这个参数会让 AUFS 为所有的 branch 注册 inotify，这样可以让 AUFS 在更新文件修改的性能更高一些。



## 选择保存文件的目录





如果 br 中只有一个 rw 的目录，那么所有的修改就会保存在那。如果有多个 rw 的 br 会怎么样？



**aufs提供了一个叫create的参数可以供你来配置相当的创建策略，下面有几个例子**



+ **create=rr | round−robin** 轮询。下面的示例可以看到，新创建的文件轮流写到三个目录中



```shell
hchen$ sudo mount -t aufs  -o dirs=./1=rw:./2=rw:./3=rw -o create=rr none ./mnt
hchen$ touch ./mnt/a ./mnt/b ./mnt/c
hchen$ tree
.
├── 1
│   └── a
├── 2
│   └── c
└── 3
    └── b
```





+ **create=mfs[:second] | most−free−space[:second]** 选一个可用空间最好的分支。可以指定一个检查可用磁盘空间的时间。





+ **create=mfsrr:low[:second]** 选一个空间大于low的branch，如果空间小于low了，那么aufs会使用 round-robin 方式。





就学习了这么多！完结撒花！









