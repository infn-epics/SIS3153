#ifndef DRVSIS3153_H
#define DRVSIS3153_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <map>

#include <iocsh.h>

#include <asynPortDriver.h>

#include <sis3150usb_vme.h>
#include <sis3150usb_vme_calls.h>
#include <sis3153ETH_vme_class.h>

#include <epicsExport.h>

// Connection types
typedef enum {
    CONNECTION_TYPE_USB,
    CONNECTION_TYPE_ETHERNET
} ConnectionType;

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */
#define P_A16D8_String                "A16D8"                  /* asynInt32,    r/w */
#define P_A16D16_String               "A16D16"                 /* asynInt32,    r/w */
#define P_A16D32_String               "A16D32"                 /* asynInt32,    r/w */
#define P_A24D8_String                "A24D8"                  /* asynInt32,    r/w */
#define P_A24D16_String               "A24D16"                 /* asynInt32,    r/w */
#define P_A24D32_String               "A24D32"                 /* asynInt32,    r/w */
#define P_A32D8_String                "A32D8"                  /* asynInt32,    r/w */
#define P_A32D16_String               "A32D16"                 /* asynInt32,    r/w */
#define P_A32D32_String               "A32D32"                 /* asynInt32,    r/w */
#define P_A32BLT32_String             "A32BLT32"



class drvSIS3153 : public asynPortDriver {
public:
    drvSIS3153(const char *portName, const char *connectionType, const char *ipAddress);
    asynStatus doBLT32Read(int addr, uint32_t* buffer, size_t nWords, unsigned int* got);
    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements,size_t *nIn);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    virtual asynStatus drvUserDestroy(asynUser *pasynUser);


protected:
    /** Values used for pasynUser->reason, and indexes into the parameter library. */
    int P_A16D8;
    int P_A16D16;
    int P_A16D32;
    int P_A24D8;
    int P_A24D16;
    int P_A24D32;
    int P_A32D8;
    int P_A32D16;
    int P_A32D32;
    int P_A32BLT32;


private:
    ConnectionType connType_;
    // USB members
    struct SIS3150USB_Device_Struct devStruct_;
    struct usb_dev_handle *devHandle_;
    // Ethernet members
    sis3153eth *ethDevice_;
    char ipAddress_[32];
};

extern  std::map<std::string, drvSIS3153*> drvTable;
#endif