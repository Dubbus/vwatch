APP       := te_screen
APP_SRCS  := app.c te_model.c
# HRmax the TE model scales intensity by; set your own: make HR_MAX=195
HR_MAX    ?= 190
CFLAGS    += -DTE_HR_MAX=$(HR_MAX)
VWATCH_SDK ?= ../../sdk
include $(VWATCH_SDK)/sdk.mk
