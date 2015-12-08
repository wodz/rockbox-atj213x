#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

UNWINDLIB_DIR = $(ROOTDIR)/lib/mipsunwinder
UNWINDLIB_SRC = $(call preprocess, $(UNWINDLIB_DIR)/SOURCES)
UNWINDLIB_OBJ := $(call c2obj, $(UNWINDLIB_SRC))

OTHER_SRC += $(UNWINDLIB_SRC)

UNWINDLIB = $(BUILDDIR)/lib/libunwinder.a
CORE_LIBS += $(UNWINDLIB)

INCLUDES += -I$(UNWINDLIB_DIR)

$(UNWINDLIB): $(UNWINDLIB_OBJ)
	$(SILENT)$(shell rm -f $@)
	$(call PRINTS,AR $(@F))$(AR) rcs $@ $^ >/dev/null
