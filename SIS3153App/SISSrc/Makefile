TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Build an IOC support library
LIBRARY_IOC += SIS3150

INC += sis3150usb_vme.h
INC += sis3150usb_vme_calls.h
INC += sis3153ETH_vme_class.h
INC += vme_interface_class.h

USR_CFLAGS += -DINSTDIR -Dunix
USR_CXXFLAGS += -Dunix

# Compile and add code to the support library
SIS3150_SRCS += sis3150usb_vme_linux.c
SIS3150_SRCS += sis3150usb_vme_calls.c
SIS3150_SRCS += sis3150usb_utils.c
SIS3150_SRCS += usrpadaptor.cpp
SIS3150_SRCS += sis3153ETH_vme_class.cpp

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

# Explicit dependency needed for generated header file
devSIS3153Version$(DEP): $(COMMON_DIR)/$(GENVERSION)
