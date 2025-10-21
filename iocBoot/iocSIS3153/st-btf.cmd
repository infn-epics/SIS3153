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

# For USB connection:
# drvSIS3153Configure("VME1")

# For Ethernet connection (comment out USB and uncomment below):
drvSIS3153EthConfigure("VME1", "192.168.189.92")

###############################################
# Load databases for BTF devices
###############################################

# CAEN V965 QDC (16 channels)
# Base address: 0xb7640000 from btf-conf.txt
dbLoadRecords("../../db/CAEN965.template", "P=BTF:QDC965:,PORT=VME1,BASE=0xee00,CHAN=0")

# CAEN V792 QDC (32 channels)  
# Base address: 0xEE540000 from btf-conf.txt
# dbLoadTemplate("../../db/CAEN792.template", "P=BTF:QDC792:,PORT=VME1,BASE=0xEE540000")

# Optional: Add PIO and Scaler databases when available
# dbLoadRecords("db/PIO.db", "P=BTF:PIO:,PORT=VME1,BASE=0x44200000")
# dbLoadRecords("db/Scaler.db", "P=BTF:SCALER:,PORT=VME1,BASE=0x38380000")

###############################################
# Initialize boards
###############################################

# Set up CAEN V965 QDC
# Software reset
# dbpf BTF:QDC965:SoftwareReset 1
# epicsThreadSleep 1

# # Initialize V965: Clear data, enable auto-increment, accept overrange, disable threshold
# dbpf BTF:QDC965:BitSet2 0x5C14
# # Clear test mode, offline, test acquisition
# dbpf BTF:QDC965:BitClear2 0x0047

# # Set crate number (adjust as needed)
# dbpf BTF:QDC965:CrateSelect 0

# # Set pedestals (example value)
# dbpf BTF:QDC965:IPedestal 255

# # Set thresholds for all channels (example: 10 for low, 100 for high)
# dbpf BTF:QDC965:Ch0ThresholdLow 10
# dbpf BTF:QDC965:Ch0ThresholdHigh 100
# # Repeat for other channels as needed...

# # Set up CAEN V792 QDC
# # Software reset
# dbpf BTF:QDC792:SoftwareReset 1
# epicsThreadSleep 1

# # Initialize V792
# dbpf BTF:QDC792:BitSet2 0x5C14
# dbpf BTF:QDC792:BitClear2 0x0047

# # Set crate number
# dbpf BTF:QDC792:CrateSelect 0

# # Set pedestals
# dbpf BTF:QDC792:IPedestal 255

# # Set thresholds for all channels (example: 10)
# dbpf BTF:QDC792:Ch0Threshold 10
# # Repeat for other channels as needed...

# ###############################################
# # IOC Initialization
# ###############################################

iocInit

## Start any sequence programs
# seq sncExample, "user=andreamichelotti"
