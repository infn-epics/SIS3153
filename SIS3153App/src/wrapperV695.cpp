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

namespace {
struct UnderlyingUserContext {
    asynUser *user;
};
}

// Costruttore
V965Wrapper::V965Wrapper(const char *portName, const char *underlyingPortName,drvSIS3153* underlyingDriver)
    : asynPortDriver(portName,
                     1, // maxAddr
                     10, // nParams
                     asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask,
                     asynInt32Mask | asynInt32ArrayMask,
                     0, 1, 0, 0),
      underlyingUser_(nullptr),
      underlyingPortName_(epicsStrDup(underlyingPortName)),
      underlyingDriver_(underlyingDriver),
      fifoUser_(nullptr),
      fifoUserBLT_(nullptr),
      pInt32Iface_(nullptr),
      pInt32DrvPvt_(nullptr),
      pDrvUserIface_(nullptr),
      pDrvUserDrvPvt_(nullptr)
      
{   
    acquiring_=false;
    this->wrapperPortName_=epicsStrDup(portName);
    // Crea parametri locali
    createParam(P_StartAcqString, asynParamInt32, &paramStartAcq_);
    createParam(P_StopAcqString,  asynParamInt32, &paramStopAcq_);
    createParam(P_DataReadyString, asynParamInt32, &paramDataReady_);
    createParam("A32BLT32",  asynParamInt32Array, &paramWaveform_);
    createParam("A32D32",  asynParamInt32, &paramA32D32_);
    createParam("A32D16",  asynParamInt32, &paramA32D16_);
    createParam("DIAG_DESYNC_COUNT", asynParamInt32, &paramDesyncCount_);
    createParam("DIAG_RESYNC_COUNT", asynParamInt32, &paramResyncCount_);
    createParam("DIAG_DISCARDED_WORDS", asynParamInt32, &paramDiscardedWords_);
    createParam("DIAG_BLT_ERROR_COUNT", asynParamInt32, &paramBLTErrorCount_);

    setIntegerParam(paramDesyncCount_, 0);
    setIntegerParam(paramResyncCount_, 0);
    setIntegerParam(paramDiscardedWords_, 0);
    setIntegerParam(paramBLTErrorCount_, 0);
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

    pInt32ArrayIface_ = nullptr;
    pInt32ArrayDrvPvt_ = nullptr;
    
    
    asynUser *wrapperUser = pasynManager->createAsynUser(0, 0);
    pasynManager->connectDevice(wrapperUser, wrapperPortName_, 0);
    //printf("Chiamo findInterface(asynInt32Array) su wrapperUser_ %p\n", wrapperUser);
    asynInterface *iface = pasynManager->findInterface(wrapperUser, asynInt32ArrayType, 1);


   // asynInterface *iface = pasynManager->findInterface(underlyingUser_, asynInt32ArrayType, 1);
    if (iface) {
        pInt32ArrayIface_ = (asynInt32Array*)iface->pinterface;
        pInt32ArrayDrvPvt_ = iface->drvPvt;
        printf("trovato interfaccia array %p\n",pInt32ArrayDrvPvt_);
    }
    else {printf("ERROR non ho trovato interfaccia array\n");}

    initUnderlyingArrayInterface();




    printf("V965Wrapper: connected to underlying port '%s'\n", underlyingPortName_);
}

void V965Wrapper::initUnderlyingArrayInterface()
{
    if (underlyingAsynUser_) return;
    underlyingAsynUser_ = pasynManager->createAsynUser(nullptr, nullptr);
    if (pasynManager->connectDevice(underlyingAsynUser_, underlyingPortName_, 0) != asynSuccess) {
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
              "Wrapper: cannot connect underlyingAsynUser to port %s\n",
              underlyingPortName_);
    underlyingAsynUser_ = nullptr; // lasciamo l’oggetto (non va rilasciato)
    return;
}
    asynInterface* iface = pasynManager->findInterface(underlyingAsynUser_,
                                                      asynInt32ArrayType, 1);
    if (iface) {
        underlyingArrayIface_ = (asynInt32Array*)iface->pinterface;
        underlyingArrayDrvPvt_ = iface->drvPvt;
    }
   

}
//readIn32Array
asynStatus V965Wrapper::readInt32Array(asynUser* u,
                                       epicsInt32* value,
                                       size_t maxElems,
                                       size_t* nRead)
{
    if(u->reason != paramWaveform_)
        return asynError;

    int addr = *(int*)u->drvUser;

    unsigned int got = 0;
    printf("AHO PRIMA DI LEGGERE BLT dal wrapper\n");
    asynStatus lockStatus = pasynManager->lockPort(underlyingUser_);
    if (lockStatus != asynSuccess) {
        *nRead = 0;
        return lockStatus;
    }
    asynStatus st = underlyingDriver_->doBLT32Read(addr,
                                                   reinterpret_cast<uint32_t*>(value),
                                                   maxElems,
                                                   &got);
    pasynManager->unlockPort(underlyingUser_);

    *nRead = got;
    return st;
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

    // Hardware records keep a wrapper-local reason.  Their separate
    // underlying asynUser carries the reason/address expected by VME1.
    if (!pInt32Iface_ || !pasynUser->drvUser) return asynError;
    UnderlyingUserContext *context =
        static_cast<UnderlyingUserContext*>(pasynUser->drvUser);

    asynStatus st = pasynManager->lockPort(context->user);
    if (st != asynSuccess) return st;
    st = pInt32Iface_->write(pInt32DrvPvt_, context->user, value);
    pasynManager->unlockPort(context->user);

    if (st != asynSuccess && context->user->errorMessage[0] != '\0') {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                      "%s", context->user->errorMessage);
    }
    return st;
}

// -------------------- READ INT32 --------------------
asynStatus V965Wrapper::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int reason = pasynUser->reason;

    if (reason == paramDesyncCount_ || reason == paramResyncCount_
        || reason == paramDiscardedWords_ || reason == paramBLTErrorCount_) {
        return getIntegerParam(reason, value);
    }

    if (reason == paramDataReady_) {
        *value = 0; // oppure stato attuale
        return asynSuccess;
    }

    if (!pInt32Iface_ || !pasynUser->drvUser) return asynError;
    UnderlyingUserContext *context =
        static_cast<UnderlyingUserContext*>(pasynUser->drvUser);

    asynStatus st = pasynManager->lockPort(context->user);
    if (st != asynSuccess) return st;
    st = pInt32Iface_->read(pInt32DrvPvt_, context->user, value);
    pasynManager->unlockPort(context->user);

    if (st != asynSuccess && context->user->errorMessage[0] != '\0') {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                      "%s", context->user->errorMessage);
    }
    return st;
}

// -------------------- DRVUSER CREATE --------------------
asynStatus V965Wrapper::drvUserCreate(asynUser *pasynUser,
                                      const char *drvInfo,
                                      const char **pptypeName,
                                      size_t *psize)
{
    const char *functionName = "V965Wrapper::drvUserCreate";
    if (!drvInfo)
        return asynError;
    std::string s(drvInfo);
    size_t pos = s.find(' ');
    std::string paramName = (pos==std::string::npos) ? s : s.substr(0,pos);

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
    if (strcmp(drvInfo, "DIAG_DESYNC_COUNT") == 0
        || strcmp(drvInfo, "DIAG_RESYNC_COUNT") == 0
        || strcmp(drvInfo, "DIAG_DISCARDED_WORDS") == 0
        || strcmp(drvInfo, "DIAG_BLT_ERROR_COUNT") == 0) {
        return asynPortDriver::drvUserCreate(
            pasynUser, drvInfo, pptypeName, psize);
    }
    if (strncmp(drvInfo, "A32BLT32",8) == 0) 
    {
        printf("TROVATO A32BLT32\n");
        pasynUser->reason = paramWaveform_;
        fifoUserBLT_ = pasynUser;
         // Ricava l'indirizzo dalla stringa drvInfo, ad es. "A32BLT32 0xee000000"
        const char* addrStr = strchr(drvInfo, ' ');
        if (addrStr) {
            unsigned int addr = 0;
            sscanf(addrStr + 1, "%x", &addr);  // legge l'esadecimale dopo lo spazio
            int* addrPtr = new int(addr);
            pasynUser->drvUser = addrPtr;      // lo memorizza in drvUser
            printf("fifoUserBLT addr = 0x%X\n", *addrPtr);
        } else {
            printf("ATTENZIONE: drvInfo non contiene indirizzo\n");
        }
        printf("paramWaveform_ = %d\n", paramWaveform_);
        printf("PV reason = %d\n", pasynUser->reason);

        if (pptypeName) *pptypeName = drvInfo; // o la = drvInfo; stringa corretta nella tua asyn
        if (psize) *psize = sizeof(epicsInt32);

        printf("drvInfo %s, %s: mapped %s -> reason=%d addr=%p\n",drvInfo,
                 functionName, paramName.c_str(), pasynUser->reason, pasynUser->drvUser);
        
        return asynSuccess;
    }
    
    
    if (drvInfo && strlen(drvInfo) >= 4) 
    {
        const char *suffix = drvInfo + strlen(drvInfo) - 4;
        // Se gli ultimi 4 caratteri sono "0000" → FIFO
        if (strcmp(suffix, "0000") == 0) {
            // Assigned below to the dedicated underlying asynUser.
        }
    }
    int wrapperReason = -1;
    if (paramName == "A32D16") {
        wrapperReason = paramA32D16_;
    } else if (paramName == "A32D32") {
        wrapperReason = paramA32D32_;
    } else {
        printf("ERR: unsupported wrapper drvInfo '%s'\n", drvInfo);
        return asynError;
    }

    if (!pDrvUserIface_) {
        printf("ERR: underlying driver has no drvUser interface\n");
        return asynError;
    }

    UnderlyingUserContext *context = new UnderlyingUserContext{nullptr};
    context->user = pasynManager->createAsynUser(nullptr, nullptr);
    if (!context->user) {
        delete context;
        return asynError;
    }

    asynStatus status = pasynManager->connectDevice(
        context->user, underlyingPortName_, 0);
    if (status == asynSuccess) {
        status = pDrvUserIface_->create(
            pDrvUserDrvPvt_, context->user, drvInfo, pptypeName, psize);
    }
    if (status != asynSuccess) {
        pasynManager->freeAsynUser(context->user);
        delete context;
        return status;
    }

    pasynUser->reason = wrapperReason;
    pasynUser->drvUser = context;

    const char *suffix = drvInfo + strlen(drvInfo) - 4;
    if (wrapperReason == paramA32D32_ && strcmp(suffix, "0000") == 0) {
        fifoUser_ = context->user;
    }

    return asynSuccess;
}

asynStatus V965Wrapper::drvUserDestroy(asynUser *pasynUser)
{
    if (pasynUser->reason == paramA32D16_
        || pasynUser->reason == paramA32D32_) {
        UnderlyingUserContext *context =
            static_cast<UnderlyingUserContext*>(pasynUser->drvUser);
        if (context) {
            if (fifoUser_ == context->user) fifoUser_ = nullptr;
            pDrvUserIface_->destroy(pDrvUserDrvPvt_, context->user);
            pasynManager->freeAsynUser(context->user);
            delete context;
            pasynUser->drvUser = nullptr;
        }
        return asynSuccess;
    }

    if (pasynUser->reason == paramWaveform_ && pasynUser->drvUser) {
        delete static_cast<int*>(pasynUser->drvUser);
        pasynUser->drvUser = nullptr;
        if (fifoUserBLT_ == pasynUser) fifoUserBLT_ = nullptr;
        return asynSuccess;
    }

    return asynPortDriver::drvUserDestroy(pasynUser);
}



void V965Wrapper::acquisitionLoopC(void *arg)
{
    V965Wrapper *pThis = static_cast<V965Wrapper*>(arg);
    pThis->acquisitionLoop();
}


void V965Wrapper::startAcquisition()
{
    printf(">>> Start acquisition loop\n");
    // avvia thread o logica di lettura FIFO
    if (acquiring_) return;  // già attivo

    stopEvent_ = epicsEventCreate(epicsEventEmpty);
    acquiring_.store(true);
    acquisitionThreadId_ = epicsThreadCreate(
        "V965AcqThread",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        &V965Wrapper::acquisitionLoopC,
        this
    );
    printf(">>> Acquisition started\n");
}

void V965Wrapper::stopAcquisition()
{
    if (!acquiring_) return;

    //acquiring_ = false;
    acquiring_.store(false);
    epicsEventSignal(stopEvent_);       // segnala il thread di terminare

    // writeInt32() is called with the asyn port locked.  The acquisition
    // thread may need the same lock to publish its final diagnostics, so do
    // not hold it while waiting for the thread to exit.
    unlock();
    while (threadIsRunning_) 
    {
        epicsThreadSleep(0.005);
    }
    lock();
    epicsEventDestroy(stopEvent_);
    stopEvent_ = nullptr;

    printf(">>> Acquisition stopped\n");
}

std::vector<uint32_t> V965Wrapper::readScalerValue(const std::string& addr)
{
    std::vector<uint32_t> counters(32, 0);
    return counters;
    const std::string drvInfo =
        addr.empty() ? "A32D32 0x38380004" : addr;

    asynUser *scalerUser = pasynManager->createAsynUser(nullptr, nullptr);
    if (!scalerUser) 
    {
        printf("Scaler read '%s': createAsynUser failed\n", drvInfo.c_str());
        return counters;
    }

    asynStatus st = pasynManager->connectDevice(
        scalerUser, underlyingPortName_, 0);
    if (st != asynSuccess) 
    {
        printf("Scaler read '%s': connectDevice failed, status=%d\n",
               drvInfo.c_str(), static_cast<int>(st));
        pasynManager->freeAsynUser(scalerUser);
        return counters;
    }

    st = pDrvUserIface_->create(
        pDrvUserDrvPvt_, scalerUser, drvInfo.c_str(), nullptr, nullptr);
    epicsInt32 value = 0;

    if (st == asynSuccess) 
    {
        st = pInt32Iface_->read(pInt32DrvPvt_, scalerUser, &value);

        printf("Scaler read '%s': value=0x%08X, return code=%d",
               drvInfo.c_str(), static_cast<unsigned>(value),
               static_cast<int>(st));
        if (st != asynSuccess && scalerUser->errorMessage[0] != '\0') {
            printf(", %s", scalerUser->errorMessage);
        }
        printf("\n");

        pDrvUserIface_->destroy(pDrvUserDrvPvt_, scalerUser);
    } 
    else 
    {
        printf("Scaler read '%s': drvUser create failed, status=%d\n",
               drvInfo.c_str(), static_cast<int>(st));
    }

    pasynManager->freeAsynUser(scalerUser);
    return counters;
}
void V965Wrapper::pushOnMemcached(const std::vector<uint32_t>& data)
{


    memcached_st *memc = memcached_create(NULL);

    memcached_server_st *servers = NULL;
    servers = memcached_server_list_append(servers, "127.0.0.1", 11211, NULL);
    memcached_server_push(memc, servers);
    const char *key = "mykey";
    memcached_return rc = memcached_set(
        memc,
        key,
        strlen(key),
        reinterpret_cast<const char*>(data.data()),
        data.size() * sizeof(uint32_t),
        0,      // expiration
        0       // flags
    );

    if (rc != MEMCACHED_SUCCESS) {
         printf("Errore: %s\n", memcached_strerror(memc, rc));
    }
    memcached_server_list_free(servers);
    memcached_free(memc);
}

void V965Wrapper::publishDiagnostics()
{
    lock();
    setIntegerParam(paramDesyncCount_, desyncCount_);
    setIntegerParam(paramResyncCount_, resyncCount_);
    setIntegerParam(paramDiscardedWords_, discardedWords_);
    setIntegerParam(paramBLTErrorCount_, bltErrorCount_);
    callParamCallbacks();
    unlock();
}

void V965Wrapper::ScalerDiagnostic()
{
    // Ricerca diagnostica del registro identificativo (+0x004).
    // I quattro rotary noti fissano 0x3838; SW_A16 e J_A11
    // selezionano uno dei 32 blocchi allineati a 0x800.
    for (uint32_t lowAddress = 0;lowAddress <= 0xF800 && acquiring_.load();lowAddress += 0x0800)
    {
        const uint32_t registerAddress = 0x38380004u | lowAddress;
        char drvInfo[32];
        epicsSnprintf(drvInfo, sizeof(drvInfo),"A32D32 0x%08X", registerAddress);
        readScalerValue(std::string(drvInfo));
    }
     for (uint32_t lowAddress = 0;
       lowAddress <= 0xF800 && acquiring_.load();
       lowAddress += 0x0800)
  {
      const uint32_t registerAddress = 0x00380004u | lowAddress;

      char drvInfo[32];
      epicsSnprintf(
          drvInfo,
          sizeof(drvInfo),
          "A24D32 0x%08X",
          registerAddress);

      readScalerValue(std::string(drvInfo));
  }
  //  Per A16:

  for (uint32_t lowAddress = 0;
       lowAddress <= 0xF800 && acquiring_.load();
       lowAddress += 0x0800)
  {
      const uint32_t registerAddress = 0x00000004u | lowAddress;

      char drvInfo[32];
      epicsSnprintf(
          drvInfo,
          sizeof(drvInfo),
          "A16D32 0x%08X",
          registerAddress);

      readScalerValue(std::string(drvInfo));
  }


    return;
}

void V965Wrapper::acquisitionLoop()
{
    
        
    printf(">>> Acquisition loop started\n");
    threadIsRunning_ = true;
    int headers=0,eob=0;
    int datawc=0;
    unsigned int consecutiveD32Errors = 0;
    unsigned int discardedDataWords = 0;
    bool resynchronizing = false;
    constexpr unsigned int adcCount = 32;
    unsigned int addr = *(int*)fifoUserBLT_->drvUser;

    //ScalerDiagnostic();
    //return;

    while (acquiring_.load()) 
    {
        std::vector<uint32_t> scalerValues = readScalerValue();
        //continue; // Rimuovi questa riga se vuoi leggere i dati BLT
        //epicsThreadSleep(2.0);
        // 1. Leggi dati dal QDC tramite il driver sottostante
        epicsInt32 value;
        unsigned int gotWords=0;
        bool publishEvent = false;
        bool fastResync = false;
        
        if (fifoUser_ && pInt32Iface_) 
        {
            asynStatus readStatus = pasynManager->lockPort(fifoUser_);
            bool vmePortLocked = (readStatus == asynSuccess);
            if (vmePortLocked) {
                readStatus = pInt32Iface_->read(
                    pInt32DrvPvt_, fifoUser_, &value);
            }
            if (readStatus != asynSuccess) {
                if (vmePortLocked) pasynManager->unlockPort(fifoUser_);
                ++consecutiveD32Errors;
                const unsigned int fifoAddress = fifoUser_->drvUser
                    ? static_cast<unsigned int>(*static_cast<int*>(fifoUser_->drvUser))
                    : 0u;
                fprintf(stderr,
                        "D32 FIFO read failed #%u: asynStatus=%d "
                        "reason=%d addr=0x%08X: %s\n",
                        consecutiveD32Errors,
                        static_cast<int>(readStatus),
                        fifoUser_->reason,
                        fifoAddress,
                        fifoUser_->errorMessage[0] != '\0'
                            ? fifoUser_->errorMessage
                            : "no driver error message");
                fflush(stderr);

                // Rate-limit the diagnostic, but let StopAcq wake the thread
                // immediately instead of waiting for the full timeout.
                if (epicsEventWaitWithTimeout(stopEvent_, 5.0) == epicsEventWaitOK) {
                    break;
                }
                continue;
            }
            if (consecutiveD32Errors != 0) {
                printf("D32 FIFO read recovered after %u consecutive errors\n",
                       consecutiveD32Errors);
                consecutiveD32Errors = 0;
            }
            int tipo=(value & 0x7000000) >> 24;
            int evCounterNum=0, numEvents=0;
            // Keep the underlying VME port locked only across the atomic
            // HEADER -> BLT sequence.  Other word types need no second read.
            if (tipo != 2) {
                pasynManager->unlockPort(fifoUser_);
            }
            //printf("read %x tipo ",tipo);
            switch(tipo)
            {
                case 0 :{
                     ++datawc;
                     ++discardedDataWords;
                     ++discardedWords_;
                     fastResync = true;
                     if (!resynchronizing) {
                         resynchronizing = true;
                         ++desyncCount_;
                         publishDiagnostics();
                         fprintf(stderr,
                                 "FIFO DESYNC: unexpected DATA word "
                                 "0x%08X outside a BLT; draining to EOB\n",
                                 static_cast<unsigned int>(value));
                         fflush(stderr);
                     }
                     break;
                }
                case 2 : {
                    numEvents = (value & 0x3F00) >> 8;
                    //printf("HEADER. words: %d\n",numEvents);
                    
                    gotWords = 0;
                    
                    asynStatus st= underlyingDriver_->doBLT32Read(addr, reinterpret_cast<unsigned int*>(bltBuffer), numEvents, &gotWords);
                    pasynManager->unlockPort(fifoUser_);

                    if (resynchronizing) {
                        ++resyncCount_;
                        publishDiagnostics();
                        fprintf(stderr,
                                "FIFO RESYNC: reached next HEADER after "
                                "%u discarded DATA words (EOB was missing)\n",
                                discardedDataWords);
                        fflush(stderr);
                        resynchronizing = false;
                        discardedDataWords = 0;
                    }
                    // printf("read %d word\n",gotWords);                  
                      if (st == asynSuccess && gotWords == static_cast<unsigned int>(numEvents)
                          && gotWords > 0)
                      {
                        epicsInt32 adcValues[32];

                        // -1 indica che il canale/range non era presente nell'evento.
                        std::fill_n(adcValues, 32, static_cast<epicsInt32>(-1));

                        for (unsigned int i = 0; i < gotWords; ++i) 
                        {
                            uint32_t word =
                                static_cast<uint32_t>(bltBuffer[i]);

                            // Solo se non viene già fatto in doBLT32Read().
                            //word = __builtin_bswap32(word);

                            unsigned int type = (word >> 24) & 0x7;

                            if (type != 0) {
                                // Scarta header, EOB, NOT VALID DATUM e word riservate.
                                continue;
                            }

                            unsigned int channel   = (word >> 17) & 0xF;
                            unsigned int range     = (word >> 16) & 0x1;
                            unsigned int underflow = (word >> 13) & 0x1;
                            unsigned int overflow  = (word >> 12) & 0x1;
                            unsigned int adcValue  = word & 0xFFF;

                            unsigned int outputIndex = channel + range * 16;

                            adcValues[outputIndex] =
                                static_cast<epicsInt32>(adcValue);

                           /* printf(
                                "DATA ch=%u range=%s adc=%u "
                                "underflow=%u overflow=%u index=%u\n",
                                channel,
                                range == 0 ? "HIGH" : "LOW",
                                adcValue,
                                underflow,
                                overflow,
                                outputIndex);
                                */
                        }

                        // Da questo punto bltBuffer contiene solo valori ADC.
                        //std::copy_n(adcValues, 32, bltBuffer);
                        std::copy(adcValues, adcValues + 32, bltBuffer);

                        //unsigned int adcCount = 32;
                        

                        doCallbacksInt32Array(bltBuffer, adcCount, paramWaveform_, 0);
                        publishEvent = true;
                       
                    } else if (st != asynSuccess
                               || gotWords != static_cast<unsigned int>(numEvents)) {
                        resynchronizing = true;
                        discardedDataWords = gotWords;
                        discardedWords_ += static_cast<epicsInt32>(gotWords);
                        ++desyncCount_;
                        ++bltErrorCount_;
                        publishDiagnostics();
                        fprintf(stderr,
                                "FIFO DESYNC after BLT: header=0x%08X "
                                "requested=%d got=%u asynStatus=%d; "
                                "draining to EOB\n",
                                static_cast<unsigned int>(value),
                                numEvents,
                                gotWords,
                                static_cast<int>(st));
                        fflush(stderr);
                    }
                    epicsThreadSleep(0.001);
                    headers++;
                    datawc=0;
                    break;
                }
                case 4 : {
                    evCounterNum= value & 0xFFFFFF;
                    // printf("EOB eventCounter %d \n",evCounterNum);
                    eob++;
                    if (resynchronizing) {
                        ++resyncCount_;
                        publishDiagnostics();
                        fprintf(stderr,
                                "FIFO RESYNC complete at EOB event=%d after "
                                "%u discarded DATA words\n",
                                evCounterNum,
                                discardedDataWords);
                        fflush(stderr);
                        resynchronizing = false;
                        discardedDataWords = 0;
                    }
                    datawc=0;
                    break;
                }
                case 6 :{
                    //printf("Not valid datum. Maybe FIFO empty\n");
                    if (resynchronizing) {
                        ++resyncCount_;
                        publishDiagnostics();
                        fprintf(stderr,
                                "FIFO RESYNC: FIFO became empty after "
                                "%u discarded DATA words\n",
                                discardedDataWords);
                        fflush(stderr);
                        resynchronizing = false;
                        discardedDataWords = 0;
                    }
                    epicsThreadSleep(0.001);
                    break;
                }
                default: printf("ERROR: type %d\n",tipo); break;
            }
                   
            if (publishEvent)
            {
                
                std::vector<uint32_t> result;
                result.reserve(scalerValues.size() + adcCount);  // evita riallocazioni
                result.insert(result.end(), scalerValues.begin(), scalerValues.end());
                //result.insert(result.end(), bltBuffer, bltBuffer + gotWords);
                result.insert(result.end(), bltBuffer, bltBuffer + adcCount); 
                for (int32_t x : result) {
                    std::cout << x << " ";
                }
                //std::cout << std::endl << "Size: " << result.size() << std::endl; 
                pushOnMemcached(result);       
            }

            // While out of phase, consume the remaining words without the
            // normal delay.  A V965 event has a bounded number of DATA words,
            // so EOB (or the next HEADER/FIFO-empty marker) restores alignment.
            if (fastResync) 
            {
                continue;
            }
        }

        // 2. Attendi per un piccolo intervallo oppure stop
        if (epicsEventWaitWithTimeout(stopEvent_, 0.001) == epicsEventWaitOK) {
            // evento di stop ricevuto
            break;
        }
    }

    printf(">>> Acquisition loop stopped : headers=%d, eob=%d\n",headers,eob);
    threadIsRunning_ = false;
}


/* =========================
   iocsh registration
   ========================= */

/* funzione di configurazione chiamabile da st.cmd */
extern "C" int V965WrapperConfigure(const char *portName, const char *underlyingPortName)
{
     drvSIS3153 *drv = nullptr;

    auto it = drvTable.find(underlyingPortName);
    if (it != drvTable.end()) {
        drv = it->second;
    }
    else {
        printf("V965WrapperConfigure ERROR: underlying driver '%s' not found\n",
               underlyingPortName);
        return -1;
    }
    new V965Wrapper(portName, underlyingPortName,drv);
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
