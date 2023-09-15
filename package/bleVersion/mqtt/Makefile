#
#
# Copyright (C) 2018 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=cositea-mqtt
PKG_VERSION:=1.0.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/cositea-mqtt
  SECTION:=base
  CATEGORY:=cositea
  TITLE:=A daemon communicates with AC cloud
  DEPENDS:=+libuci +libjson-c +libpthread +libubox +libmosquitto-nossl +libcurl
endef

define Package/cositea-mqtt/description
A daemon program which is communicated with cloud.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/cositea-mqtt/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cositea_mqtt $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/cositea_mqtt.config $(1)/etc/config/cositea_mqtt

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/cositea_mqtt.init $(1)/etc/init.d/cositea_mqtt
endef

$(eval $(call BuildPackage,cositea-mqtt))
