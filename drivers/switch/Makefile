# Switch Class Driver
obj-$(CONFIG_SWITCH)		+= switch_class.o
obj-$(CONFIG_SWITCH_GPIO)	+= switch_gpio.o
ifeq ($(CONFIG_7265C_V20),y)
obj-$(CONFIG_SWITCH_DOCK)	+= switch_dock_.o
obj-$(CONFIG_SWITCH_H2W)	+= switch_h2w_.o
else
obj-$(CONFIG_SWITCH_DOCK)	+= switch_dock.o
obj-$(CONFIG_SWITCH_H2W)	+= switch_h2w.o
endif

obj-$(CONFIG_SWITCH_HDMI)	+= switch_hdmi.o

