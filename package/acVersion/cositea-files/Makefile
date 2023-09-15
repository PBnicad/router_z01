#
# Copyright (C) 2019 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=cositea-files
PKG_VERSION:=1.0.0

PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/cositea-files
  SECTION:=base
  CATEGORY:=cositea
  TITLE:=Base filesystem for Cositea
endef

define Package/cositea-files/description
 This package contains a base configuration and system scripts for Cositea.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default
endef
Build/Compile = $(Build/Compile/Default)

define Package/cositea-files/install-overlay
    $(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,cositea-files))
