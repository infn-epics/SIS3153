// wrapper.cpp
//
// Esempio di wrapper che incapsula un driver esistente (es. drvSIS3153).
// Compilalo insieme al resto del modulo, poi istanzia con:
//   V965WrapperConfigure("V965Port", "SIS3153Port");
//
// NOTE:
// - Adatta i nomi dei parametri e le interfacce asyn che usi (asynInt32, asynInt32Array, asynOctet, ...)
// - Qui mostro l'uso di pasynManager->connectDevice() per parlare con il driver sottostante.

#include "wrapperV695.h"
#include <epicsThread.h>
#include <epicsString.h>
#include <iocsh.h>       // Per iocshArg, iocshFuncDef, iocshRegister
#include <epicsExport.h> // Per epicsExportRegistrar
#include <stdio.h>
#include <string.h>

// Costruttore
V965Wrapper::V965Wrapper(const char *portName, const char *underlyingPortName)
    : asynPortDriver(portName,
                     1, // maxAddr
                     4, // nParams
                     asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask,
                     asynInt32Mask | asynInt32ArrayMask,
                     0, 1, 0, 0),
      underlyingUser_(nullptr),
      underlyingPortName_(epicsStrDup(underlyingPortName)),
      pInt32Iface_(nullptr),
      pInt32DrvPvt_(nullptr),
      pDrvUserIface_(nullptr),
      pDrvUserDrvPvt_(nullptr)
{
    // Crea parametri locali
    createParam(P_StartAcqString, asynParamInt32, &paramStartAcq_);
    createParam(P_StopAcqString,  asynParamInt32, &paramStopAcq_);
    createParam(P_DataReadyString, asynParamInt32, &paramDataReady_);
    createParam(P_WaveformString,  asynParamInt32Array, &paramWaveform_);

    // Ottieni asynUser per il port sottostante
    underlyingUser_ = pasynManager->createAsynUser(nullptr, nullptr);
    if (!underlyingUser_) {
        printf("ERR: createAsynUser() failed\n");
        return;
    }
    
    asynStatus status = pasynManager->connectDevice(underlyingUser_, underlyingPortName_, 0);
    if (status != asynSuccess) {
        printf("ERR: connectDevice('%s') failed\n", underlyingPortName_);
        return;
    }

    // Interfaccia asynInt32
    asynInterface *pif = pasynManager->findInterface(underlyingUser_, asynInt32Type, 1);
    if (pif) {
        pInt32Iface_ = (asynInt32 *)pif->pinterface;
        pInt32DrvPvt_ = pif->drvPvt;
    } else {
        printf("WARN: %s has no asynInt32 interface\n", underlyingPortName_);
    }

    // Interfaccia asynDrvUser
    pif = pasynManager->findInterface(underlyingUser_, asynDrvUserType, 1);
    if (pif) {
        pDrvUserIface_ = (asynDrvUser *)pif->pinterface;
        pDrvUserDrvPvt_ = pif->drvPvt;
    } else {
        printf("WARN: %s has no asynDrvUser interface\n", underlyingPortName_);
    }

    printf("V965Wrapper: connected to underlying port '%s'\n", underlyingPortName_);
}

// -------------------- WRITE INT32 --------------------
asynStatus V965Wrapper::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int reason = pasynUser->reason;

    if (reason == paramStartAcq_) {
        startAcquisition();
        return asynSuccess;
    }
    if (reason == paramStopAcq_) {
        stopAcquisition();
        return asynSuccess;
    }

    // Inoltra al driver sottostante
    if (!pInt32Iface_ || !pDrvUserIface_) return asynError;

    // Creazione temporanea del drvUser
    const char *typeName = nullptr;
    size_t size = 0;
    asynStatus st = pDrvUserIface_->create(pDrvUserDrvPvt_, pasynUser,
                                        static_cast<const char*>(pasynUser->userData), &typeName, &size);
    if (st != asynSuccess) return st;

    st = pInt32Iface_->write(pInt32DrvPvt_, pasynUser, value);

    pDrvUserIface_->destroy(pDrvUserDrvPvt_, pasynUser);
    return st;
}

// -------------------- READ INT32 --------------------
asynStatus V965Wrapper::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int reason = pasynUser->reason;

    if (reason == paramDataReady_) {
        *value = 0; // oppure stato attuale
        return asynSuccess;
    }

    if (!pInt32Iface_ || !pDrvUserIface_) return asynError;

    const char *typeName = nullptr;
    size_t size = 0;
        

    asynStatus st = pDrvUserIface_->create(pDrvUserDrvPvt_, pasynUser,static_cast<const char*>(pasynUser->userData), &typeName, &size);
    if (st != asynSuccess) return st;

    st = pInt32Iface_->read(pInt32DrvPvt_, pasynUser, value);

    pDrvUserIface_->destroy(pDrvUserDrvPvt_, pasynUser);
    return st;
}

// -------------------- DRVUSER CREATE --------------------
asynStatus V965Wrapper::drvUserCreate(asynUser *pasynUser,
                                      const char *drvInfo,
                                      const char **pptypeName,
                                      size_t *psize)
{
    if (!drvInfo)
        return asynError;
    // Memorizza il drvInfo nella userData del pasynUser
    pasynUser->userData = (void*)drvInfo;
    if (strcmp(drvInfo, P_StartAcqString) == 0) {
        pasynUser->reason = paramStartAcq_;
        return asynSuccess;
    }
    if (strcmp(drvInfo, P_StopAcqString) == 0) {
        pasynUser->reason = paramStopAcq_;
        return asynSuccess;
    }
    if (strcmp(drvInfo, P_DataReadyString) == 0) {
        pasynUser->reason = paramDataReady_;
        return asynSuccess;
    }
    if (strcmp(drvInfo, P_WaveformString) == 0) {
        pasynUser->reason = paramWaveform_;
        return asynSuccess;
    }

    // Altrimenti passa al driver sottostante
    if (pDrvUserIface_) {
        return pDrvUserIface_->create(pDrvUserDrvPvt_, pasynUser,
                                      drvInfo, pptypeName, psize);
    }

    printf("ERR: underlying driver has no drvUser interface\n");
    return asynError;
}

void V965Wrapper::startAcquisition()
{
    printf(">>> Start acquisition loop\n");
    // avvia thread o logica di lettura FIFO
}

void V965Wrapper::stopAcquisition()
{
    printf(">>> Stop acquisition loop\n");
    // ferma thread o logica FIFO
}



/* =========================
   iocsh registration
   ========================= */

/* funzione di configurazione chiamabile da st.cmd */
extern "C" int V965WrapperConfigure(const char *portName, const char *underlyingPortName)
{
    new V965Wrapper(portName, underlyingPortName);
    return 0;
}

/* iocsh wrappers */
static const iocshArg confArg0 = {"portName", iocshArgString};
static const iocshArg confArg1 = {"underlyingPortName", iocshArgString};
static const iocshArg * const confArgs[2] = {&confArg0, &confArg1};
static const iocshFuncDef confFuncDef = {"V965WrapperConfigure", 2, confArgs};

static void confCallFunc(const iocshArgBuf *args)
{
    V965WrapperConfigure(args[0].sval, args[1].sval);
}

static void V965WrapperRegister(void)
{
    iocshRegister(&confFuncDef, confCallFunc);
}

epicsExportRegistrar(V965WrapperRegister);
