#
#
# Copyright (C) 2019 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=quectel
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/quectel
  SECTION:=base
  CATEGORY:=cositea
  TITLE:=quectel qmi
  DEPENDS:=+libubox +libcurl +libuci
endef

define Package/quectel/description
quectel qmi.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/quectel/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel $(1)/usr/bin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) files/quectel.config $(1)/etc/config/quectel

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/quectel.init $(1)/etc/init.d/quectel
endef

$(eval $(call BuildPackage,quectel))
