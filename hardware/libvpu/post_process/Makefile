LOCAL_PATH = $(shell pwd)

SUBDIRS = 
EXCLUDEDIRS = $(LOCAL_PATH)/enc $(LOCAL_PATH)
SUBDIRS_1 = $(shell find $(LOCAL_PATH) -maxdepth 1 -type d)
#SUBDIRS_2 = $(basename $(patsubst ./%,%,$(SUBDIRS_1)))
SUBDIRS += $(filter-out $(EXCLUDEDIRS),$(SUBDIRS_1))

all:
	@for dir in $(SUBDIRS);do $(MAKE) -C $$dir all || exit 1;done

clean:
	@for dir in $(SUBDIRS);do $(MAKE) -C $$dir clean || exit 1;done

.PHONY:all clean
