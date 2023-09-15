## 编译

**注意：**

**需要联网，安装编译会下载一些包，需要 *科学上网* 环境否则会编译失败**

在编译前，需要准备一台Linux，BSD或者MacOSX电脑，或者虚拟机，Windows 10系统支持wsl可以直接运行ubuntu。推荐使用Ubuntu 18.04及以上版本
（使用windows的wsl ubuntu时版本过高（22.04）可能会有gcc版本的问题，建议使用低版本）

**新建一个非root用户进行操作**

**安装依赖软件包**

```
sudo apt-get install build-essential subversion libncurses5-dev zlib1g-dev gawk gcc-multilib flex git gettext libssl-dev unzip python
```

**克隆源码**

克隆openwrt-18.06分支
```
git clone -b openwrt-18.06 https://github.com/openwrt/openwrt.git 
```

**安装openwrt的feeds**

进入openwrt文件夹

```
cd openwrt
```

执行以下指令

```
./scripts/feeds update -a
./scripts/feeds install -a
```
**固件编译**
执行
```
make menuconfig
```
依次选择：
Target System(MediaTek Ralink MIPS) --->
Subtarget(Mt76X8 based boards) --->
Target Profile(COSITEA Z01) --->（找不到COSITEA Z01先暂时任意选一个其他的代替）

**编译**
执行
```
make V=s
```

**配置**
如果Target Profile没有COSITEA Z01:
1.将Z01.dts添加到target/linux/ramips/dts/目录下
2.添加下列代码到openwrt/target/linux/ramips/image/mt76x8.mk
```
define Device/cositea_z01
  DTS :=Z01
  IMAGE_SIZE := $(ralink_default_fw_size_8M)
  DEVICE_TITLE := COSITEA Z01
endef
TARGET_DEVICES += cositea_z01
```
3.修改openwrt/target/linux/ramips/base-files/lib/upgrade/platform.sh添加支持COSITEA Z01固件升级。（将以下代码添加到TPlink之前）

```
cf-wr800n|\
cositea,z01|\
cs-qr10|\
```

**如果在前面make menuconfig时Target Profile没有COSITEA Z01：**

1.创建 openwrt/target/linux/ramips/dts/Z01.dts

```
/dts-v1/;
#include <dt-bindings/input/input.h>
#include <dt-bindings/gpio/gpio.h>

#include "mt7628an.dtsi"

/ {
	compatible = "cositea,z01", "mediatek,mt7628an-soc";
	model = "COSITEA Z01";

	chosen {
		bootargs = "console=ttyS0,115200";
	};

	memory@0 {
		device_type = "memory";
		reg = <0x0 0x4000000>;
	};

	gpio-keys-polled {
		compatible = "gpio-keys-polled";
		poll-interval = <20>;

		reset {
			label = "reset";
			gpios = <&gpio1 6 GPIO_ACTIVE_LOW>;
			linux,code = <KEY_RESTART>;
		};
	};

	gpio-leds {
		compatible = "gpio-leds";

		wan {
			label = "z01:blue:wan";
			gpios = <&gpio1 11 GPIO_ACTIVE_LOW>;
		};

		wifi {
			label = "z01:blue:wifi";
			gpios = <&gpio1 12 GPIO_ACTIVE_LOW>;
		};
	};
};

&pinctrl {
	state_default: pinctrl0 {
		gpio {
			ralink,group = "wled_an", "p0led_an", "refclk";
			ralink,function = "gpio";
		};
	};
};

&spi0 {
	status = "okay";

	flash@0 {
		compatible = "jedec,spi-nor";
		reg = <0>;
		spi-max-frequency = <10000000>;
		m25p,chunked-io = <32>;

		partitions {
			compatible = "fixed-partitions";
			#address-cells = <1>;
			#size-cells = <1>;

			partition@0 {
				label = "u-boot";
				reg = <0x0 0x30000>;
				read-only;
			};

			partition@30000 {
				label = "u-boot-env";
				reg = <0x30000 0x10000>;
				read-only;
			};

			factory: partition@40000 {
				label = "factory";
				reg = <0x40000 0x10000>;
			};

			partition@50000 {
				compatible = "denx,uimage";
				label = "firmware";
				reg = <0x50000 0x7b0000>;
			};
		};
	};
};

&wmac {
	status = "okay";
	ralink,mtd-eeprom = <&factory 0x4>;
};

&ethernet {
	mtd-mac-address = <&factory 0x4>;
	mediatek,portmap = "wllll";
};

&uart1 {
	status = "okay";
};
```

2.修改openwrt/target/linux/ramips/image/mt76x8.mk，将以下内容添加到最后面，修改这一步后就能在target profile中看到

```
define Device/cositea_z01
	DTS := Z01
	IMAGE_SIZE := $(ralink_default_fw_size_8M)
	DEVICE_TITLE := COSITEA Z01
endef
TARGET_DEVICES += cositea_z01
```

3.修改openwrt/target/linux/ramips/base-files/lib/upgrade/platform.sh添加支持COSITEA Z01固件升级。

```
	cf-wr800n|\
	cositea,z01|\
	cs-qr10|\
```

将以上内容添加到`tplink,archer-c20-v5`之前

**第二次编译make menuconfig**

```
Base system (不用配置)
Administration (不用配置)
Boot Loaders (不用配置)
cositea --->(选择自己添加的模块进行编译,选择luci app后会自动选择)
Development (不用配置)
Extra packages (不用配置)
Firmware (不用配置)
Fonts (不用配置)
Kernel modules (重点配置,有时候需要额外命令配置make kernel_menuconfig)
{
     USB Support  --->
          <*> kmod-usb-ohci
          <*> kmod-usb-serial (虚拟机调试时可选)
          <*>   kmod-usb-serial-ch341 (虚拟机调试时可选)
          <*> kmod-usb2
}
Languages (不用配置)
Libraries  (重点配置,不出意外都是自动选择了)
{

}
LuCI (重点配置)
{
1. Collections
    <*> luci......(不配置则无法通过浏览器访问)
2. Modules
   Translations  --->(语言支持,默认支持语言需要修改代码,或者自动匹配)
        <*> English (en) 
        <*> Chinese (zh-cn)
3. Applications (添加服务内容,和cositea --->(选择自己添加的模块进行编译)关联)
        <*> luci-app-mqtt
        <*> luci-app-stp 
        <*> luci-app-travelmate
        <*> luci-app-acmanage
4. Themes (主题配置)
        <*> luci-theme-material
}
Mail  (不用配置)
Multimedia (不用配置)
Network (重点配置)
{
     WWAN  --->
<*> uqmi......................... Control utility for mobilebroa
     <*> mosquitto-client-nossl
}
Sound (不用配置)
Utilities (重点配置)
{
     <*> dmesg (调试时使用)
}
Xorg (不用配置)
```

## 通过web烧录

按下复位键后再通电源, 大概五六秒以上wan灯亮起. 使用网线电脑和板子网口直连. 电脑设置静态ip 192.168.1.x, 打开浏览器访问192.168.1.1, 选择固件烧录

## 构建应用包

openwrt的应用包格式为ipk

### 构建一个示例hello应用包

注意事项：如果在windows编辑文本，要注意文件格式要为`unix`格式，很多文件不兼容

**包构建定义**

创建`Makefile`:

```makefile
#
#
# Copyright (C) 2018 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=hello
PKG_VERSION:=0.0.2
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/hello
  SECTION:=base
# 定义在make menuconfig的第一级菜单的myPackage
# 如果要定义在其他菜单目录可以查看openwrt的package源码参考。
  CATEGORY:=myPackage
# 在make menuconfig会看到 [ ]  hello (A daemon hello)
  TITLE:=A daemon hello
# 依赖的库，这是一些常用的库，实际这个demo没有用到
#  DEPENDS:=+libuci +libjson-c +libpthread +libubox +libmosquitto-nossl +libcurl
  DEPENDS:=+libuci
endef

# 包描述，在make menuconfig里面查看help时显示的内容。
define Package/hello/description
A daemon program which is communicated with cloud.
endef

define Build/Prepare
# 创建目录进行拷贝源码编译
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

# 定义包安装规则
define Package/hello/install
# $(PKG_BUILD_DIR)/hello是程序编译产物，这里告诉包管理将程序安装到哪个目录
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hello $(1)/usr/bin

# files/hello.config 告诉包管理器这个文件安装到哪，这个文件其实就是给 files/hello.init 这个脚本使用的。
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/hello.config $(1)/etc/config/hello

# files/hello.init 初始化脚本，安装到/etc/init.d/目录。注意，在这个目录的脚本，当路由启动后会执行里面的所有脚本，程序的自启动在这个脚本完成
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/hello.init $(1)/etc/init.d/hello
endef

# 
$(eval $(call BuildPackage,hello))

```

**编写源码**

创建一个文件夹`src`，这个是在前面的`Makefile`的`define Build/Prepare`定义的源码目录

在`src`里创建`main.c`

```c
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    //打印传递的参数，/etc/init.d/hello这个脚本会将读取到的配置文件参数传递进来
    for(int i=0; i<argc; i++)
    {
        printf("%s\n", argv[i]);
    }
    while(1)
    {
            sleep(1);
            printf("hello world!\n");
    }
 
    return 0;   
}
```

在`src`里创建`Makefile`用来编译源码

```makefile
LDFLAGS += -luci

#这个是编译出来程序的名字，要注意这个名字和包定义Makefile的PKG_BUILD_DIR安装的程序的名字要一样
PROC=hello
SRC=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

all : $(PROC)
	@echo "compile done"

$(PROC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -fr $(PROC) $(OBJS)
```

**UCI配置文件和脚本**

创建文件夹`files`

在`files`里创建文件`hello.config`,这个文件就是前面`Makefile`的`Package/hello/install`定义的。

这个配置文件和openwrt的uci功能关联，先学会怎么用。后面再了解uci。

```
config hello 'hello'
    option enable '1'
    option cus '123456'
    option hello 'hello'
```

`files`里创建`hello.init`,这个文件也是前面的`Makefile`的`Package/hello/install`定义的

```
#!/bin/sh /etc/rc.common
#
# This is free software, licensed under the GNU General Public License v3.
# See /LICENSE for more information.
#

# 定义自启动优先级，越低启动顺序越靠后
START=99
# 好像是启动进程守护来着
USE_PROCD=1

append_parm() {
    local section="$1"
    local option="$2"
    local switch="$3"
    local default="$4"
    local _loctmp
    config_get _loctmp "$section" "$option"
    [ -n "$_loctmp" -o -n "$default" ] || return 0
    procd_append_param command "$switch" "${_loctmp:-$default}"
}

start_instance() {
    local enable
        # 获取配置文件里的 enable 配置，如果是1才继续，否则退出，不运行程序
    config_get_bool enable $1 enable
    [ "$enable" = "1" ] || return 0

    procd_open_instance
    procd_set_param respawn
    procd_set_param stderr 1
    procd_set_param command /usr/bin/hello
        # 定义配置文件里的配置以什么参数传递到程序     
 append_parm $1 cus "-e"
    append_parm $1 hello "-h"
    procd_close_instance
}

service_triggers()
{
    procd_add_reload_trigger "hello"
}

# 系统启动后会自动执行这个方法
start_service() {
        # 加载配置文件 hello
    config_load hello
        # 主要看 start_instance 方法，这个hello是配置文件里定义的config hello
    config_foreach start_instance hello
}
```

**目录结构**

```
files

​	hello.config

​	hello.init

src

​	main.c

​	Makefile

Makefile
```

将整个文件夹放到源码目录的`package`的目录下

终端输入`make menuconfig`

在myPackage----->在hello中能看到

**编译**

```
//make package/包名/compile
make package/hello/compile	V=s
```

清除编译

```
//make package/包名/clean
make package/hello/clean
```

在`bin/packages/mipsel_24kc/base/hello_0.0.1-1_mipsel_24kc.ipk`可以查看到应用包

**安装**

有两种方式，一种是直接编译进固件，另一种是单独一应用包(ipk)的方式安装

这里只举例应用宝的安装方式

开启一个tftpd服务器，将应用包放到服务器

将`hello_0.0.1-1_mipsel_24kc.ipk`复制到服务器对应目录下重命名为`hello.ipk`

进入开发板终端执行命令：

```
tftp -g -r hello.ipk 192.xxx.xxx.xxx(电脑ip)
```

传输完成后安装

```
opkg install hello.ipk
```

以下命令控制

```
hello start
hello stop
hello restart
hello enable
hello disable
```

