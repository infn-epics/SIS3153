#!../../bin/linux-x86_64/SIS3153App

# BTF Configuration Startup Script
# Based on btf-conf.txt:
# pio:0x44200000
# scaler:0x38380000
# qdc965:0xb7640000
# qdc792:0xEE540000

#< envPaths

#cd "${TOP}"

## Register all support components
dbLoadDatabase "../../dbd/SIS3153App.dbd"
SIS3153App_registerRecordDeviceDriver pdbbase

###############################################
# Configure SIS3153 VME Interface
###############################################

# For Ethernet connection (comment out USB and uncomment below):
#asynReport 5
drvSIS3153EthConfigure("VME1", "192.168.189.92")
V965WrapperConfigure("V965Port", "VME1")
V965WrapperConfigure("V792Port", "VME1")

#asynReport 5, V965Port
#asynReport 1, VME1

#asynSetTraceMask V965Port -1 0x1F
###############################################
# Load databases for BTF devices
###############################################

# CAEN V965 QDC (16 channels)

dbLoadTemplate("../../db/CAEN965_channels.substitutions", "P=BTF:QDC965:,PORT=V965Port,BASE=0xee00")
dbLoadTemplate("../../db/CAEN792_channels.substitutions", "P=BTF:QDC792:,PORT=V792Port,BASE=0xaa00")

#scaler
#dbLoadRecords("../../db/CAENSCALER.template", "P=BTF:SCALER:,PORT=VME1,BASE=0x3838")

# ###############################################
# # IOC Initialization
# ###############################################

iocInit

## Start any sequence programs
# seq sncExample, "user=andreamichelotti"
