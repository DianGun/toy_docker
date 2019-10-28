# 代码以及使用



> 代码在 ubuntu18.04 下可以跑通，其他操作系统没有测试





## 0X00 目录代码介绍



+ `middle_code` 看耗叔博客写的中间代码，可省略



+ `create_bridge.sh` 创建一个网桥！但是这样创建的网桥能够 ping 1.1.1.1。 但是 ping baidu.com 会有 dns 解析失败，最终我也不知道是什么原因，希望大佬能够带带我，告诉我为什么。所以，我最后使用的网桥是 docker0，dns 解析不会失败，可以使用 apt install 



+ `toy_docker.c` 这是最终代码，实现了`README.md` 中提到的 3 个功能。注意！默认路由，要和 docker0 的地址一致



+ `ubuntu18.tar.gz` 这是 toy_docker 需要的镜像，解压到当前目录就可以，其中的 share 就是共享文件夹



## 0X03 最后



扫个码我们做朋友吧！**顺便点个 star 呗！**

![](../images/wx.jpg)

