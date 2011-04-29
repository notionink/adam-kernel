batt-objs := smart_battery.o
batt-objs += odm_i2c.o
batt-objs += odm_sbi.o

ifeq (CONFIG_USE_BQ24745_CHARGER, y)
batt-objs += bq24745.o 
ccflags-y += -__USE_BQ24745_SMART_CHARGER__
endif

obj-y += batt.o
