#
#
# Copyright (C) 2018 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=acmanage
PKG_VERSION:=1.2.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/acmanage
  SECTION:=base
  CATEGORY:=cositea
  TITLE:=A daemon communicates with AC cloud
  DEPENDS:=+libuci +libjson-c +libpthread +libubox +libmosquitto-nossl +libcurl
endef

define Package/acmanage/description
A daemon program which is communicated with cloud.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/acmanage/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/acmanage $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/acmanage.config $(1)/etc/config/acmanage

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/acmanage.init $(1)/etc/init.d/acmanage
endef

$(eval $(call BuildPackage,acmanage))
