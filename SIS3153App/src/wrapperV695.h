#ifndef V965_WRAPPER_H
#define V965_WRAPPER_H
#include <libmemcached/memcached.h>
#include <asynPortDriver.h>
#include "drvSIS3153.h"
#include <atomic>
#include <vector>
#include <iostream>
#include <cstdint>

// Parametri locali del wrapper
#define P_StartAcqString "START_ACQ"
#define P_StopAcqString  "STOP_ACQ"
#define P_DataReadyString "DATA_READY"
#define P_WaveformString  "QDC_WAVEFORM"

class V965Wrapper : public asynPortDriver {
public:
    V965Wrapper(const char *portName, const char *underlyingPortName,drvSIS3153* underlyingDriver);
    void initUnderlyingArrayInterface();
    // Override da asynPortDriver
    virtual asynStatus readInt32Array(asynUser *pasynUser,epicsInt32 *value,size_t maxElems,size_t *nIn) override;

    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value) override;
    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value) override;
    asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                             const char **pptypeName, size_t *psize) override;
    

    std::vector<uint32_t> readScalerValue();
    void startAcquisition();
    void stopAcquisition();
    void acquisitionLoop();
    static void acquisitionLoopC(void *arg);
    bool isWrapperInfo(const char* drvInfo);
    void pushOnMemcached(const std::vector<uint32_t>& data);

private:
    
    
    // Connessione al driver sottostante
    
    asynUser *underlyingUser_;
    char *underlyingPortName_;
    drvSIS3153 * underlyingDriver_;
    asynUser *fifoUser_, *fifoUserBLT_;
    const char* wrapperPortName_;
    asynInt32   *pInt32Iface_;
    void        *pInt32DrvPvt_;

    asynInt32Array *pInt32ArrayIface_;
    void *pInt32ArrayDrvPvt_;

    asynUser* underlyingAsynUser_ = nullptr;
    asynInt32Array* underlyingArrayIface_ = nullptr;
    void* underlyingArrayDrvPvt_ = nullptr;

    // Buffer temporaneo per leggere i dati BLT
    static const size_t MAX_BLT_WORDS = 1024;
    epicsInt32 bltBuffer[MAX_BLT_WORDS];


    asynDrvUser *pDrvUserIface_;
    void        *pDrvUserDrvPvt_;

    //parametri loop acquisizione
    epicsThreadId acquisitionThreadId_;
    epicsEventId stopEvent_;
    std::atomic<bool> acquiring_;
    bool threadIsRunning_;
    // Parametri locali
    int paramStartAcq_;
    int paramStopAcq_;
    int paramDataReady_;
    int paramWaveform_;
    int paramA32D32_;
    int paramA32D16_;
};

#endif
