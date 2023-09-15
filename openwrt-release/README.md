编译指导  
========  

编译准备  
--------  

- 环境  

  在编译前，需要准备一台Linux，BSD或者MacOSX电脑，或者虚拟机，Windows 10系统支持wsl，可以直接运行ubuntu。推荐使用Ubuntu 16.04及以上版本。  

- 创建非root用户  

  环境搭建完毕以后，需要创建一个普通用户编译OpenWRT，注意不能使用root权限编译，否则编译失败。  

- 依赖软件包  

  ```  
  sudo apt-get install build-essential subversion libncurses5-dev zlib1g-dev gawk gcc-multilib flex git-core gettext libssl-dev unzip python
  ```  

- 克隆源码  

  ```  
  git clone https://gitlab.com/cositea/openwrt.git
  ```  
- 安装OpenWRT的feeds  

  克隆源码后，进入OpenWRT的根目录，执行以下指令：  
  ```  
  ./scripts/feeds update -a
  ./scripts/feeds install -a
  ```  

固件编译  
--------  

- 选择目标平台  

  执行`make menuconfig`，依次选择：  
  ```  
    Target System (MediaTek Ralink MIPS)  --->
    Subtarget (MT76x8 based boards)  --->
    Target Profile (COSITEA Z01)  --->
  ```  
  可以根据需要选择其他软件包，如Luci等，选择完毕后，保存退出。  

- 编译  

  执行`make V=s`  
