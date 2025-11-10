#ifndef V965_WRAPPER_H
#define V965_WRAPPER_H

#include <asynPortDriver.h>

// Parametri locali del wrapper
#define P_StartAcqString "START_ACQ"
#define P_StopAcqString  "STOP_ACQ"
#define P_DataReadyString "DATA_READY"
#define P_WaveformString  "QDC_WAVEFORM"

class V965Wrapper : public asynPortDriver {
public:
    V965Wrapper(const char *portName, const char *underlyingPortName);

    // Override da asynPortDriver
    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value) override;
    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value) override;
    asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                             const char **pptypeName, size_t *psize) override;

    void startAcquisition();
    void stopAcquisition();

private:
    // Connessione al driver sottostante
    asynUser *underlyingUser_;
    char *underlyingPortName_;

    asynInt32   *pInt32Iface_;
    void        *pInt32DrvPvt_;
    asynDrvUser *pDrvUserIface_;
    void        *pDrvUserDrvPvt_;

    // Parametri locali
    int paramStartAcq_;
    int paramStopAcq_;
    int paramDataReady_;
    int paramWaveform_;
};

#endif
