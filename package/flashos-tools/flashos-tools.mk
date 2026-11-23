################################################################################
#
# flashos-tools
#
################################################################################

FLASHOS_TOOLS_VERSION = 1.0
FLASHOS_TOOLS_SITE = $(BR2_EXTERNAL_FLASHOS_PATH)/src/flashsh
FLASHOS_TOOLS_SITE_METHOD = local
FLASHOS_TOOLS_LICENSE = MIT
FLASHOS_TOOLS_DEPENDENCIES = xlib_libX11

define FLASHOS_TOOLS_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) all
endef

define FLASHOS_TOOLS_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D) DESTDIR=$(TARGET_DIR) install
endef

$(eval $(generic-package))
