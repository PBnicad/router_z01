#
#
# Copyright (C) 2019 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=stp
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/stp
	SECTION:=base
	CATEGORY:=cositea
	TITLE:=Serial Transparent Proxy(STP)
	DEPENDS:=+libubox +libcurl +libuci +libpthread +libuci
endef

define Package/ddns_update

endef

define Package/stp/description
Serial Transparent Proxy(stp) is an application that forward traffic transparently between serial devices and udp/tcp devices.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/stp/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/stp $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/stp.config $(1)/etc/config/stp

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/stp.init $(1)/etc/init.d/stp
endef

$(eval $(call BuildPackage,stp))
