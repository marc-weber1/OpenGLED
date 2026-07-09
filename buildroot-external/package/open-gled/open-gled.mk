################################################################################
#
# open-gled
#
# Builds the OpenGLED source tree this buildroot-external tree lives in.
# SITE_METHOD = local rsyncs the repo on every (dir)clean+build, so keep the
# buildroot output directory OUTSIDE the repository (build-image.sh does).
#
################################################################################

OPEN_GLED_VERSION = local
OPEN_GLED_SITE = $(BR2_EXTERNAL_OPENGLED_PATH)/..
OPEN_GLED_SITE_METHOD = local
OPEN_GLED_DEPENDENCIES = alsa-lib mesa3d
OPEN_GLED_SUPPORTS_IN_SOURCE_BUILD = NO
# Vendored subprojects declare cmake_minimum_required < 3.10; keep CMake 4.x happy.
# BUILD_SHARED_LIBS=OFF keeps the vendored libs static (only the binary is installed).
OPEN_GLED_CONF_OPTS = -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=OFF

# The project has no install() rules; copy the binary ourselves
define OPEN_GLED_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(OPEN_GLED_BUILDDIR)/open_gled $(TARGET_DIR)/usr/bin/open_gled
endef

$(eval $(cmake-package))
