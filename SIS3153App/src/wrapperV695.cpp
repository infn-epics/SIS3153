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
V965Wrapper::V965Wrapper(const char *portName, const char *underlyingPortName,drvSIS3153* underlyingDriver)
    : asynPortDriver(portName,
                     1, // maxAddr
                     6, // nParams
                     asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask,
                     asynInt32Mask | asynInt32ArrayMask,
                     0, 1, 0, 0),
      underlyingUser_(nullptr),
      underlyingPortName_(epicsStrDup(underlyingPortName)),
      underlyingDriver_(underlyingDriver),
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
    asynStatus st = underlyingDriver_->doBLT32Read(addr,
                                                   reinterpret_cast<uint32_t*>(value),
                                                   maxElems,
                                                   &got);

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
    const char *functionName = "V965Wrapper::drvUserCreate";
    if (!drvInfo)
        return asynError;
    std::string s(drvInfo);
    size_t pos = s.find(' ');
    std::string paramName = (pos==std::string::npos) ? s : s.substr(0,pos);

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
            fifoUser_ = pasynUser;
        }
    }
    // Altrimenti passa al driver sottostante
    if (pDrvUserIface_) {
        return pDrvUserIface_->create(pDrvUserDrvPvt_, pasynUser,
                                      drvInfo, pptypeName, psize);
    }

    printf("ERR: underlying driver has no drvUser interface\n");
    return asynError;
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
    while (threadIsRunning_) 
    {
        epicsThreadSleep(0.005);
    }
    epicsEventDestroy(stopEvent_);
    stopEvent_ = nullptr;

    printf(">>> Acquisition stopped\n");
}

std::vector<uint32_t> V965Wrapper::readScalerValue()
{
    
    std::vector<uint32_t> counters(32, 0);
    asynUser *scalerUser_ = pasynManager->createAsynUser(nullptr, nullptr);
    pasynManager->connectDevice( scalerUser_, underlyingPortName_, 0);
    //printf("Before create scalerUser=%p reason=%d drvUser=%p\n", scalerUser_, scalerUser_->reason, scalerUser_->drvUser);
    //asynStatus st =  pDrvUserIface_->create( pDrvUserDrvPvt_,scalerUser_, "A32D32 0xee001024",nullptr, nullptr);
    asynStatus st =  pDrvUserIface_->create( pDrvUserDrvPvt_,scalerUser_, "A32D32 0x38000004",nullptr, nullptr);
    epicsInt32 value = 0;
    
    scalerUser_->reason = paramA32D32_;
    //printf("AFTER scalerUser= %p drvUser=%p reason=%d \n", scalerUser_, scalerUser_->drvUser, scalerUser_->reason);
    //asynStatus st= underlyingDriver_->doBLT32Read(addr, reinterpret_cast<unsigned int*>(bltBuffer), numEvents, &gotWords);
    int kk=pInt32Iface_->read( pInt32DrvPvt_, scalerUser_, &value);

    printf("Scaler value = 0x%08X\nReturn code = %d\n", (unsigned)value, kk);
    
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
void V965Wrapper::acquisitionLoop()
{
    
        
    printf(">>> Acquisition loop started\n");
    threadIsRunning_ = true;
    int headers=0,eob=0;
    int datawc=0;
    
    unsigned int addr = *(int*)fifoUserBLT_->drvUser;
    while (acquiring_.load()) {
        std::vector<uint32_t> scalerValues = readScalerValue();
        //epicsThreadSleep(2.0);
        // 1. Leggi dati dal QDC tramite il driver sottostante
        epicsInt32 value;
        unsigned int gotWords=0;
        if (fifoUser_ && pInt32Iface_) {
            //printf("%d ",++cnt);
            pInt32Iface_->read(pInt32DrvPvt_, fifoUser_, &value);
            
            int tipo=(value & 0x7000000) >> 24;
            int evCounterNum=0, numEvents=0;
            //printf("read %x tipo ",tipo);
            switch(tipo)
            {
                case 0 :{
                     datawc++;
                     //printf("DATA %d\n",datawc);
                     break;
                }
                case 2 : {
                    numEvents = (value & 0x3F00) >> 8;
                    //printf("HEADER. words: %d\n",numEvents);
                    
                    gotWords = 0;
                    
                    asynStatus st= underlyingDriver_->doBLT32Read(addr, reinterpret_cast<unsigned int*>(bltBuffer), numEvents, &gotWords);
                    // printf("read %d word\n",gotWords);                  
                    if (st == asynSuccess && gotWords > 0)
                    {
                        //setIntegerParam(paramDataReady_, 1);   // paramDataReady_ è un int usato per debug/testing
                        //setIntegerParam(paramWaveform_, (int)gotWords); 
                        //printf("Callback: param=%d addr=%x got=%zu\n",paramWaveform_, addr, gotWords);
                        doCallbacksInt32Array(bltBuffer,gotWords,paramWaveform_,0); 
                        //setTimeStamp();
                        //callParamCallbacks();
                       
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
                    datawc=0;
                    break;
                }
                case 6 :{
                    //printf("Not valid datum. Maybe FIFO empty\n");
                    epicsThreadSleep(0.001);
                    break;
                }
                default: printf("ERROR: type %d\n",tipo); break;
            }
                   
            if (gotWords > 0) 
            {
                
                std::vector<uint32_t> result;
                result.reserve(scalerValues.size() + gotWords);  // evita riallocazioni
                result.insert(result.end(), scalerValues.begin(), scalerValues.end());
                result.insert(result.end(), bltBuffer, bltBuffer + gotWords); 
                for (uint32_t x : result) {
                    std::cout << x << " ";
                }
                std::cout << std::endl << "Size: " << result.size() << std::endl; 
                pushOnMemcached(result);       
            }
            // Qui puoi fare buffering dei valori in parametri EPICS, array, ecc.
            //callParamCallbacks();  // Notifica i record collegati
            
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
