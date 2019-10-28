#!/bin/bash

# 创建网桥的脚本，需要 root 权限
# 使用这个脚本创建的网桥，在我的电脑上会出现 dns 解析失败，希望有大佬能告诉我为什么
# 网桥配的 ip 地址要与内部网关的默认路由一致！！！

sudo brctl addbr mydocker
# 网桥配的 ip 地址要与内部网关的默认路由一致！！！
sudo ip addr add 172.17.0.11/24 dev mydocker
sudo ip link set mydocker address 00:0a:e7:2c:44:2a
sudo ip link set dev mydocker up
# 不知道为什么 dns 会失败，希望有大佬能告诉我！！！
