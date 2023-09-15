#
#
# Copyright (C) 2019 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=uart2tcp
PKG_VERSION:=1.0.2
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/uart2tcp
  SECTION:=base
  CATEGORY:=cositea
  TITLE:=UART to TCP tool
  DEPENDS:=+libubox +libcurl +libjson-c +libuci
endef

define Package/uart2tcp/description
UART to TCP tool use to bridge Bluetooth and Cloud.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/uart2tcp/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart2tcp $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/uart2tcp.config $(1)/etc/config/uart2tcp

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/uart2tcp.init $(1)/etc/init.d/uart2tcp
endef

$(eval $(call BuildPackage,uart2tcp))
