## 首先，我们先增加一个网桥mydocker，模仿docker
brctl addbr mydocker
brctl stp mydocker off
ifconfig mydocker 192.168.10.1/24 up #为网桥设置 IP 地址
 
## 接下来，我们要创建一个network namespace - ns1

# 增加一个 namesapce 命令为 ns1 （使用ip netns add命令）
ip netns add ns1
 
# 激活 namespace 中的 loopback，即127.0.0.1（使用ip netns exec ns1来操作ns1中的命令）
ip netns exec ns1 ip link set dev lo up
## 然后，我们需要增加一对虚拟网卡
 
# 增加一个 pair 虚拟网卡，注意其中的veth类型，其中一个网卡要按进容器中
ip link add veth-ns1 type veth peer name out
 
# 把 veth-ns1 按到namespace ns1中，这样容器中就会有一个新的网卡了
ip link set veth-ns1 netns ns1
 
# 把容器里的 veth-ns1改名为 eth0 （容器外会冲突，容器内就不会了）
ip netns exec ns1 ip link set dev veth-ns1 name eth0 

# 为容器中的网卡分配一个IP地址，并激活它
ip netns exec ns1 ifconfig eth0 192.168.10.11/24 up

# 上面我们把veth-ns1这个网卡按到了容器中，然后我们要把out添加上网桥上
brctl addif mydocker out

# 为容器增加一个路由规则，让容器可以访问外面的网络
ip netns exec ns1 ip route add default via 192.168.10.1