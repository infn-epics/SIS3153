/*
 * drvSIS3153.cpp
 *
 * Driver for SIS 3153 USB/Ethernet to VME interface
 *
 * Author: Mark Rivers
 * Modified: Andrea Michelotti - Added Ethernet support
 *
 * Created October 5, 2024
 * Modified October 20, 2025
 */

#include "drvSIS3153.h"

static const char *driverName="drvSIS3153";

std::map<std::string, drvSIS3153*> drvTable;
drvSIS3153::drvSIS3153(const char *portName, const char *connectionType, const char *ipAddress)
   : asynPortDriver(portName,
                    1, /* maxAddr */
                    asynInt32Mask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask,  /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver blocks and it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0), /* Default stack size*/
    devHandle_(NULL),
    ethDevice_(NULL)
{
    const char *functionName = "drvSIS3153";
    int status;
    unsigned int found;

    createParam(P_A16D8_String,  asynParamInt32, &P_A16D8);
    createParam(P_A16D16_String, asynParamInt32, &P_A16D16);
    createParam(P_A16D32_String, asynParamInt32, &P_A16D32);
    createParam(P_A24D8_String,  asynParamInt32, &P_A24D8);
    createParam(P_A24D16_String, asynParamInt32, &P_A24D16);
    createParam(P_A24D32_String, asynParamInt32, &P_A24D32);
    createParam(P_A32D8_String,  asynParamInt32, &P_A32D8);
    createParam(P_A32D16_String, asynParamInt32, &P_A32D16);
    createParam(P_A32D32_String, asynParamInt32, &P_A32D32);
    P_A32BLT32 = -1;
    createParam(P_A32BLT32_String, asynParamInt32Array, &P_A32BLT32);
    
    printf("Driver crea PV array %s, index=%d\n", P_A32BLT32_String, P_A32BLT32);

    // Determine connection type
    if (strcmp(connectionType, "USB") == 0) {
        connType_ = CONNECTION_TYPE_USB;
        
        status = FindAll_SIS3150USB_Devices(&devStruct_, &found, 1);
        printf("FindAll_SIS3150USB_Devices found %d devices, status=%d\n", found, status);
        if (status) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling FindAll_SIS3150USB_Devices = %d\n", 
            driverName, functionName, status);
            return;
        }

        printf("path: %s\n", devStruct_.cDName);
        printf("vendor: 0x%04X\n", devStruct_.idVendor);
        printf("product: 0x%04X\n", devStruct_.idProduct);

        status = Sis3150usb_OpenDriver_And_Download_FX2_Setup ((PCHAR)devStruct_.cDName, &devStruct_);
        if (status) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling Sis3150usb_OpenDriver_And_Download_FX2_Setup = %d\n", 
            driverName, functionName, status);
            return;
        }
        devHandle_ = devStruct_.hDev;
        printf("serial #: %04d\n", devStruct_.idSerNo);
        printf("firmware: 0x%04x\n", devStruct_.idFirmwareVersion);
        printf("handle: %p\n", devHandle_);
        printf("USB connection established\n");
        
    } else if (strcmp(connectionType, "ETHERNET") == 0) {
        connType_ = CONNECTION_TYPE_ETHERNET;
        
        if (ipAddress == NULL || strlen(ipAddress) == 0) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s IP address required for Ethernet connection\n",
                driverName, functionName);
            return;
        }
        
        strncpy(ipAddress_, ipAddress, sizeof(ipAddress_) - 1);
        ipAddress_[sizeof(ipAddress_) - 1] = '\0';
        sis3153eth(&ethDevice_, (char *)ipAddress_);
        if (ethDevice_ == NULL) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s error creating sis3153eth object\n",
                driverName, functionName);
            return;
        }
        
        status = ethDevice_->vmeopen();
        if (status != 0) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s error opening Ethernet VME connection, status=0x%x\n",
                driverName, functionName, status);
            delete ethDevice_;
            ethDevice_ = NULL;
            return;
        }
        printf("Ethernet connection established to %s\n", ipAddress_);
        
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s invalid connection type '%s', must be 'USB' or 'ETHERNET'\n",
            driverName, functionName, connectionType);
        return;
    }
}

asynStatus drvSIS3153::doBLT32Read(int addr, uint32_t* buffer, size_t nWords, unsigned int* got)
{
    int status = ethDevice_->vme_A32BLT32_read(addr, buffer, nWords, got);
    return (status == 0) ? asynSuccess : asynError;
}

asynStatus drvSIS3153::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements,size_t *nIn)
{
    const char* functionName = "readInt32Array";
    printf("Inside %s\n",functionName);
    int function = pasynUser->reason;

    if (function == P_A32BLT32)
    {
         /* Get the VME address which is in pasynUser->drvUser */
        int addr = *(int *)pasynUser->drvUser;

        unsigned int gotWords = 0;

        int status = ethDevice_->vme_A32BLT32_read(
                         addr,
                         reinterpret_cast<unsigned int*>(value),
                         nElements,
                         &gotWords);

        if(status != 0)
        {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s::%s BLT read error status=0x%x\n",
                      driverName, functionName, status);
            return asynError;
        }

        return asynSuccess;
    }

    // Tutto il resto passa alla base (default -> errore)
    return asynPortDriver::readInt32Array(pasynUser, value, nElements,nIn);
}



asynStatus drvSIS3153::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    int status = 0;
    int addr;
    const char *paramName;
    const char* functionName = "readInt32";

    /* Get the VME address which is in pasynUser->drvUser */
    addr = *(int *)pasynUser->drvUser;
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    if (connType_ == CONNECTION_TYPE_USB) {
        if (function == P_A16D8) {
            u_int8_t vme_data;
            status = vme_A16D8_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A16D16) {
            u_int16_t vme_data;
            status = vme_A16D16_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A16D32) {
            u_int32_t vme_data;
            status = vme_A16D32_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D8) {
            u_int8_t vme_data;
            status = vme_A24D8_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D16) {
            u_int16_t vme_data;
            status = vme_A24D16_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D32) {
            u_int32_t vme_data;
            status = vme_A24D32_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D8) {
            u_int8_t vme_data;
            status = vme_A32D8_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D16) {
            u_int16_t vme_data;
            status = vme_A32D16_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D32) {
            u_int32_t vme_data;
            status = vme_A32D32_read(devHandle_, addr, &vme_data);
            *value = vme_data;
        }
    } else { // CONNECTION_TYPE_ETHERNET
        if (function == P_A16D8) {
            u_int8_t vme_data;
            status = ethDevice_->vme_A16D8_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A16D16) {
            u_int16_t vme_data;
            status = ethDevice_->vme_A16D16_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A16D32) {
            u_int32_t vme_data;
            status = ethDevice_->vme_A16D32_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D8) {
            u_int8_t vme_data;
            status = ethDevice_->vme_A24D8_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D16) {
            u_int16_t vme_data;
            status = ethDevice_->vme_A24D16_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A24D32) {
            u_int32_t vme_data;
            status = ethDevice_->vme_A24D32_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D8) {
            u_int8_t vme_data;
            status = ethDevice_->vme_A32D8_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D16) {
            u_int16_t vme_data;
            status = ethDevice_->vme_A32D16_read(addr, &vme_data);
            *value = vme_data;
        }
        else if (function == P_A32D32) {
            u_int32_t vme_data;
            status = ethDevice_->vme_A32D32_read(addr, &vme_data);
            *value = vme_data;
        }
    }

    if (status)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s:%s: status=%d, function=%d, name=%s, addr=0x%x, value=%d",
                  driverName, functionName, status, function, paramName, addr, *value);
    //else
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, name=%s, status=%d, addr=0x%x, value=%d\n",
              driverName, functionName, function, paramName, status, addr, *value);
    return status ? asynError : asynSuccess;
}

asynStatus drvSIS3153::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int status = 0;
    int addr;
    const char *paramName;
    const char* functionName = "writeInt32";

    /* Get the VME address which is in pasynUser->drvUser */
    addr = *(int *)pasynUser->drvUser;
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    if (connType_ == CONNECTION_TYPE_USB) {
        if (function == P_A16D8) {
            u_int8_t vme_data = value;
            status = vme_A16D8_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A16D16) {
            u_int16_t vme_data = value;
            status = vme_A16D16_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A16D32) {
            u_int32_t vme_data = value;
            status = vme_A16D32_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A24D8) {
            u_int8_t vme_data = value;
            status = vme_A24D8_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A24D16) {
            u_int16_t vme_data = value;
            status = vme_A24D16_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A24D32) {
            u_int32_t vme_data = value;
            status = vme_A24D32_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A32D8) {
            u_int8_t vme_data = value;
            status = vme_A32D8_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A32D16) {
            u_int16_t vme_data = value;
            status = vme_A32D16_write(devHandle_, addr, vme_data);
        }
        else if (function == P_A32D32) {
            u_int32_t vme_data = value;
            status = vme_A32D32_write(devHandle_, addr, vme_data);
        }
    } else { // CONNECTION_TYPE_ETHERNET
        if (function == P_A16D8) {
            u_int8_t vme_data = value;
            status = ethDevice_->vme_A16D8_write(addr, vme_data);
        }
        else if (function == P_A16D16) {
            u_int16_t vme_data = value;
            status = ethDevice_->vme_A16D16_write(addr, vme_data);
        }
        else if (function == P_A16D32) {
            u_int32_t vme_data = value;
            status = ethDevice_->vme_A16D32_write(addr, vme_data);
        }
        else if (function == P_A24D8) {
            u_int8_t vme_data = value;
            status = ethDevice_->vme_A24D8_write(addr, vme_data);
        }
        else if (function == P_A24D16) {
            u_int16_t vme_data = value;
            status = ethDevice_->vme_A24D16_write(addr, vme_data);
        }
        else if (function == P_A24D32) {
            u_int32_t vme_data = value;
            status = ethDevice_->vme_A24D32_write(addr, vme_data);
        }
        else if (function == P_A32D8) {
            u_int8_t vme_data = value;
            status = ethDevice_->vme_A32D8_write(addr, vme_data);
        }
        else if (function == P_A32D16) {
            u_int16_t vme_data = value;
            status = ethDevice_->vme_A32D16_write(addr, vme_data);
        }
        else if (function == P_A32D32) {
            u_int32_t vme_data = value;
            status = ethDevice_->vme_A32D32_write(addr, vme_data);
        }
    }

    if (status)
        //epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,        
                  "%s:%s: status=%d, function=%d, name=%s, addr=0x%x, value=%d\n",
                  driverName, functionName, status, function, paramName, addr, value);
    else
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s:%s: function=%d, name=%s, addr=0x%x, value=%d\n",
              driverName, functionName, function, paramName, addr, value);
    return status ? asynError : asynSuccess;
}




asynStatus drvSIS3153::drvUserCreate(asynUser *pasynUser, 
                                     const char *drvInfo,
                                     const char **pptypeName,
                                     size_t *psize)
{
    // Caso: il drvInfo NON appartiene all’underlying
    // SINTASSI underlying = "<ParamName> <Address>"
    // Se non c’è spazio → non è dell’underlying → rifiutalo senza errori
    if (!strchr(drvInfo, ' ')) {
        static const char *underlyingNames[] = {
        P_A16D8_String, P_A16D16_String, P_A16D32_String,
        P_A24D8_String, P_A24D16_String, P_A24D32_String,
        P_A32D8_String, P_A32D16_String, P_A32D32_String,
        P_A32BLT32_String, // se l'hai definito
        nullptr
    };
    for (const char **p = underlyingNames; *p; ++p) 
    {
        if (strcmp(drvInfo, *p) == 0) {
            // è un parametro dell'underlying: lascia che la base lo risolva
            return asynPortDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
        }
    }
        return asynError;   // lascia che il wrapper lo gestisca
    }

    // DA QUI IN POI: sintassi valida per underlying
    std::string drvString = drvInfo;
    size_t pos = drvString.find(" ");

    std::string addrString = drvString.substr(pos + 1);
    int *pAddr = new(int);

    try {
        *pAddr = (int)std::stol(addrString, 0, 0);
    } catch (...) {
        return asynError;
    }

    pasynUser->drvUser = pAddr;

    std::string newDrvInfo = drvString.substr(0, pos);
    return asynPortDriver::drvUserCreate(pasynUser, newDrvInfo.c_str(),
                                         pptypeName, psize);
}




asynStatus drvSIS3153::drvUserDestroy(asynUser *pasynUser)
{
    if (pasynUser->drvUser) {
        delete (int *)(pasynUser->drvUser);
    }
    pasynUser->drvUser = NULL;
    return asynSuccess;
}

/* Configuration routines.  Called directly, or from the iocsh function below */

extern "C" {

/** USB Configuration command for SIS3153 */
int drvSIS3153Configure(const char *portName)
{
    new drvSIS3153(portName, "USB", NULL);
    return(asynSuccess);
}

/** Ethernet Configuration command for SIS3153 
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] ipAddress The IP address of the SIS3153 device (e.g., "192.168.1.100") */
int drvSIS3153EthConfigure(const char *portName, const char *ipAddress)
{
    drvSIS3153 *drv = new drvSIS3153(portName, "ETHERNET", ipAddress);
    drvTable[portName] = drv;
    return(asynSuccess);
}


/* EPICS iocsh shell commands */

// USB configuration
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg * const initArgs[] = {&initArg0};
static const iocshFuncDef initFuncDef = {"drvSIS3153Configure", 1, initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    drvSIS3153Configure(args[0].sval);
}

// Ethernet configuration
static const iocshArg initEthArg0 = { "portName",iocshArgString};
static const iocshArg initEthArg1 = { "ipAddress",iocshArgString};
static const iocshArg * const initEthArgs[] = {&initEthArg0, &initEthArg1};
static const iocshFuncDef initEthFuncDef = {"drvSIS3153EthConfigure", 2, initEthArgs};
static void initEthCallFunc(const iocshArgBuf *args)
{
    drvSIS3153EthConfigure(args[0].sval, args[1].sval);
}

void drvSIS3153Register(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
    iocshRegister(&initEthFuncDef, initEthCallFunc);
}

epicsExportRegistrar(drvSIS3153Register);

}
