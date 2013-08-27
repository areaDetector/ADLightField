/* LightField.cpp
 *
 * This is a driver for Priceton Instruments detectors using LightField Automation.
 *
 * Author: Mark Rivers
 *         University of Chicago
 *
 * Created: August 14, 2013
 *
 */
 
#include "stdafx.h"

#include <string>
#include <stdio.h>
#include <stdlib.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>

#include "ADDriver.h"

#include <epicsExport.h>

using namespace System;
using namespace System::Collections::Generic;

#using <PrincetonInstruments.LightField.AutomationV4.dll>
#using <PrincetonInstruments.LightFieldViewV4.dll>
#using <PrincetonInstruments.LightFieldAddInSupportServices.dll>

using namespace PrincetonInstruments::LightField::Automation;
using namespace PrincetonInstruments::LightField::AddIns;

#define ERROR_MESSAGE_SIZE 256
#define MAX_COMMENT_SIZE 80
/** The polling interval when checking to see if acquisition is complete */
#define LIGHTFIELD_POLL_TIME .01

static const char *driverName = "LightField";

static int acquisitionComplete;

typedef enum {
    LightFieldImageNormal,
    LightFieldImagePreview
} LightFieldImageMode_t;

/** Driver-specific parameters for the Lightfield driver */
#define LightFieldNumAcquisitionsString        "LIGHTFIELD_NACQUISITIONS"
#define LightFieldNumAcquisitionsCounterString "LIGHTFIELD_NACQUISITIONS_COUNTER"

/** Driver for Princeton Instruments cameras using the LightField Automation software */
class LightField : public ADDriver {
public:
    LightField(const char *portName,
               int maxBuffers, size_t maxMemory,
               int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual void setShutter(int open);
    virtual void report(FILE *fp, int details);
    void LightFieldTask();  /* This should be private but is called from C, must be public */

protected:
    int LightFieldNumAcquisitions;
    #define FIRST_LIGHTFIELD_PARAM LightFieldNumAcquisitions
    int LightFieldNumAcquisitionsCounter;
    #define LAST_LIGHTFIELD_PARAM LightFieldNumAcquisitionsCounter
         
private:                               
    gcroot<PrincetonInstruments::LightField::Automation::Automation ^> Automation_;
    gcroot<ILightFieldApplication^> Application_;
    gcroot<IExperiment^> Experiment_;
    asynStatus setROI();
    NDArray *getData();
    asynStatus getStatus();
    asynStatus convertDataType(NDDataType_t NDArrayType, int *LightFieldType);
    asynStatus saveFile();
    
    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    char errorMessage[ERROR_MESSAGE_SIZE];
};


#define NUM_LIGHTFIELD_PARAMS ((int)(&LAST_LIGHTFIELD_PARAM - &FIRST_LIGHTFIELD_PARAM + 1))

void ExperimentCompletedEventHandler(System::Object^ sender, 
                                     PrincetonInstruments::LightField::AddIns::ExperimentCompletedEventArgs^ args)
{
    printf("ExperimentCompletedEventHandler entry\n");
    acquisitionComplete = 1;
    printf("ExperimentCompletedEventHandler exit\n");
}

asynStatus LightField::saveFile()
{
    return asynSuccess;
}


NDArray* LightField::getData()
{
   return NULL;
}


asynStatus LightField::getStatus()
{
    short result;
    const char *functionName = "getStatus";
    double top, bottom, left, right;
    long minX, minY, sizeX, sizeY, binX, binY;
    
    callParamCallbacks();
    return(asynSuccess);
}


asynStatus LightField::setROI()
{
    int minX, minY, sizeX, sizeY, binX, binY, maxSizeX, maxSizeY;
    double ROILeft, ROIRight, ROITop, ROIBottom;
    asynStatus status;
    const char *functionName = "setROI";

    status = getIntegerParam(ADMinX,  &minX);
    status = getIntegerParam(ADMinY,  &minY);
    status = getIntegerParam(ADSizeX, &sizeX);
    status = getIntegerParam(ADSizeY, &sizeY);
    status = getIntegerParam(ADBinX,  &binX);
    status = getIntegerParam(ADBinY,  &binY);
    status = getIntegerParam(ADMaxSizeX, &maxSizeX);
    status = getIntegerParam(ADMaxSizeY, &maxSizeY);
    /* Make sure parameters are consistent, fix them if they are not */
    if (binX < 1) {
        binX = 1; 
        status = setIntegerParam(ADBinX, binX);
    }
    if (binY < 1) {
        binY = 1;
        status = setIntegerParam(ADBinY, binY);
    }
    if (minX < 1) {
        minX = 1; 
        status = setIntegerParam(ADMinX, minX);
    }
    if (minY < 1) {
        minY = 1; 
        status = setIntegerParam(ADMinY, minY);
    }
    if (minX > maxSizeX-binX) {
        minX = maxSizeX-binX; 
        status = setIntegerParam(ADMinX, minX);
    }
    if (minY > maxSizeY-binY) {
        minY = maxSizeY-binY; 
        status = setIntegerParam(ADMinY, minY);
    }
    if (sizeX < binX) sizeX = binX;    
    if (sizeY < binY) sizeY = binY;    
    if (minX+sizeX-1 > maxSizeX) sizeX = maxSizeX-minX+1; 
    if (minY+sizeY-1 > maxSizeY) sizeY = maxSizeY-minY+1; 
    /* The size must be a multiple of the binning or the controller can generate an error */
    sizeX = (sizeX/binX) * binX;
    sizeY = (sizeY/binY) * binY;
    status = setIntegerParam(ADSizeX, sizeX);
    status = setIntegerParam(ADSizeY, sizeY);
    ROILeft = minX;
    ROIRight = minX + sizeX - 1;
    ROITop = minY;
    ROIBottom = minY + sizeY - 1;
    
    return(asynSuccess);
}

void LightField::setShutter(int open)
{
    int shutterMode;
    
    getIntegerParam(ADShutterMode, &shutterMode);
    if (shutterMode == ADShutterModeDetector) {
        /* Simulate a shutter by just changing the status readback */
        setIntegerParam(ADShutterStatus, open);
    } else {
        /* For no shutter or EPICS shutter call the base class method */
        ADDriver::setShutter(open);
    }
}


static void LightFieldTaskC(void *drvPvt)
{
    LightField *pPvt = (LightField *)drvPvt;
    
    pPvt->LightFieldTask();
}

/** This thread computes new image data and does the callbacks to send it to higher layers */
void LightField::LightFieldTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numAcquisitions, numAcquisitionsCounter;
    int imageMode;
    int arrayCallbacks;
    int acquire, autoSave;
    NDArray *pImage;
    double acquireTime, acquirePeriod, delay;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    const char *functionName = "LightFieldTask";
    VARIANT varArg;
    IDispatch *pDocFileDispatch;
    HRESULT hr;

    this->lock();
    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire) {
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            this->unlock();
            status = epicsEventWait(this->startEventId);
            this->lock();
            getIntegerParam(ADAcquire, &acquire);
            setIntegerParam(LightFieldNumAcquisitionsCounter, 0);
        }
        
        /* We are acquiring. */
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        
        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        getIntegerParam(ADImageMode, &imageMode);
        getIntegerParam(LightFieldNumAcquisitions, &numAcquisitions);
        
        setIntegerParam(ADStatus, ADStatusAcquire);
        
        /* Open the shutter */
        setShutter(ADShutterOpen);

        /* Call the callbacks to update any changes */
        callParamCallbacks();

        /* Collect the frame(s) */
        /* Stop current exposure, if any */

        // Start acquisition 
        Experiment_->Acquire();

        /* Wait for acquisition to complete, but allow acquire stop events to be handled */
        while (1) {
            this->unlock();
            status = epicsEventWaitWithTimeout(this->stopEventId, LIGHTFIELD_POLL_TIME);
            this->lock();
            if (status == epicsEventWaitOK) {
                /* We got a stop event, abort acquisition */
                Experiment_->Stop();
                acquire = 0;
            } else {
                acquire = !acquisitionComplete;
            }
            if (!acquire) {
                /* Close the shutter */
                setShutter(ADShutterClosed);
                break;
            }
        }

        /* Get the current parameters */
        getIntegerParam(NDAutoSave,         &autoSave);
        getIntegerParam(NDArrayCounter,     &imageCounter);
        getIntegerParam(LightFieldNumAcquisitionsCounter, &numAcquisitionsCounter);
        getIntegerParam(NDArrayCallbacks,   &arrayCallbacks);
        imageCounter++;
        numAcquisitionsCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);
        setIntegerParam(LightFieldNumAcquisitionsCounter, numAcquisitionsCounter);
        
        if (arrayCallbacks) {
            /* Get the data from the DocFile */
            pImage = this->getData();
            if (pImage)  {
                /* Put the frame number and time stamp into the buffer */
                pImage->uniqueId = imageCounter;
                pImage->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;
                /* Get any attributes that have been defined for this driver */        
                this->getAttributes(pImage->pAttributeList);
                /* Call the NDArray callback */
                /* Must release the lock here, or we can get into a deadlock, because we can
                 * block on the plugin lock, and the plugin can be calling us */
                this->unlock();
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                     "%s:%s: calling imageData callback\n", driverName, functionName);
                doCallbacksGenericPointer(pImage, NDArrayData, 0);
                this->lock();
                pImage->release();
            }
        }
        
        /* See if acquisition is done */
        if ((imageMode == LightFieldImagePreview) ||
            ((imageMode == LightFieldImageNormal) && 
             (numAcquisitionsCounter >= numAcquisitions))) {
            setIntegerParam(ADAcquire, 0);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                  "%s:%s: acquisition completed\n", driverName, functionName);
        }
        
        /* Call the callbacks to update any changes */
        callParamCallbacks();
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are acquiring then sleep for the acquire period minus elapsed time. */
        if (acquire) {
            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            delay = acquirePeriod - elapsedTime;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                     "%s:%s: delay=%f\n",
                      driverName, functionName, delay);            
            if (delay >= 0.0) {
                /* We set the status to indicate we are in the period delay */
                setIntegerParam(ADStatus, ADStatusWaiting);
                callParamCallbacks();
                this->unlock();
                status = epicsEventWaitWithTimeout(this->stopEventId, delay);
                this->lock();
            }
        }
    }
}


/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADBinX, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus LightField::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    int currentlyAcquiring;
    int dataType;
    int LightFieldDataType;
    asynStatus status = asynSuccess;
    VARIANT varArg;
    int needReadStatus=1;
    const char* functionName="writeInt32";

    /* Initialize the variant and set reasonable defaults for data type and value */
    VariantInit(&varArg);
    varArg.vt = VT_I4;
    varArg.lVal = value;
    
    /* See if we are currently acquiring.  This must be done before the call to setIntegerParam below */
    getIntegerParam(ADAcquire, &currentlyAcquiring);
    
    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setIntegerParam(function, value);

    try {
        if (function == ADAcquire) {
            if (value && !currentlyAcquiring) {
                /* Send an event to wake up the LightField task.  
                 * It won't actually start generating new images until we release the lock below */
                epicsEventSignal(this->startEventId);
            } 
            if (!value && currentlyAcquiring) {
                /* This was a command to stop acquisition */
                /* Send the stop event */
                epicsEventSignal(this->stopEventId);
            }
        } else if ( (function == ADBinX) ||
                    (function == ADBinY) ||
                    (function == ADMinX) ||
                    (function == ADMinY) ||
                    (function == ADSizeX) ||
                    (function == ADSizeY)) {
            this->setROI();
        } else if (function == NDDataType) {
        } else if (function == ADNumImages) {
        } else if (function == ADNumExposures) {
        } else if (function == ADReverseX) {
        } else if (function == ADReverseY) {
        } else if (function == NDWriteFile) {
            /* Call the callbacks so the busy state is visible while file is being saved */
            callParamCallbacks();
            status = this->saveFile();
            setIntegerParam(NDWriteFile, 0);
        } else if (function == ADTriggerMode) {
        } else if (function == ADShutterMode) {
        } else if (function == NDDataType) {
            getIntegerParam(NDDataType, &dataType);
            if (value == 0) { /* Not auto data type, re-send data type */
                getIntegerParam(NDDataType, &dataType);
                convertDataType((NDDataType_t)dataType, &LightFieldDataType);
                varArg.lVal = LightFieldDataType;
            }
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_LIGHTFIELD_PARAM) status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
        status = asynError;
    }
    
    /* Read the actual state of the detector after this operation if anything could have changed */
    if (needReadStatus) this->getStatus();
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s: error, status=%d function=%d, value=%d\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%d\n", 
              driverName, functionName, function, value);
    return status;
}


/** Called when asyn clients call pasynFloat64->write().
  * This function performs actions for some parameters, including ADAcquireTime, ADGain, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus LightField::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    VARIANT varArg;
    int needReadStatus=1;
    const char* functionName="writeInt32";

    /* Initialize the variant and set reasonable defaults for data type and value */
    VariantInit(&varArg);
    varArg.vt = VT_R4;
    varArg.fltVal = (epicsFloat32)value;

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    try {
        if (function == ADAcquireTime) {
            Experiment_->SetValue(CameraSettings::ShutterTimingExposureTime, value);
        } else if (function == ADTemperature) {
        } else if (function == ADGain) {
            varArg.vt = VT_I4;
            varArg.lVal = (int)value;
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_LIGHTFIELD_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(this->errorMessage, sizeof(this->errorMessage));
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: exception = %s\n", 
            driverName, functionName, this->errorMessage);
        pEx->Delete();
    }

    /* Read the actual state of the detector after this operation */
    if (needReadStatus) this->getStatus();

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:%s, status=%d function=%d, value=%f\n", 
              driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%f\n", 
              driverName, functionName, function, value);
    return status;
}


/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void LightField::report(FILE *fp, int details)
{

    fprintf(fp, "LightField detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}

extern "C" int LightFieldConfig(const char *portName,
                           int maxBuffers, size_t maxMemory,
                           int priority, int stackSize)
{
    new LightField(portName, maxBuffers, maxMemory, priority, stackSize);
    return(asynSuccess);
}

/** Constructor for LightField driver; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to collect the detector data, 
  * and sets reasonable default values for the parameters defined in this class, asynNDArrayDriver and
  * ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is 
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
LightField::LightField(const char *portName,
             int maxBuffers, size_t maxMemory,
             int priority, int stackSize)

    : ADDriver(portName, 1, NUM_LIGHTFIELD_PARAMS, maxBuffers, maxMemory, 
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)

{
    int status = asynSuccess;
    const char *functionName = "LightField";
    const char *controllerName;
    int controllerNum;
    IDispatch *pDocFileDispatch;

    createParam(LightFieldNumAcquisitionsString,        asynParamInt32,   &LightFieldNumAcquisitions);
    createParam(LightFieldNumAcquisitionsCounterString, asynParamInt32,   &LightFieldNumAcquisitionsCounter);
 

    /* Read the state of the detector */
    status = this->getStatus();
    if (status) {
        printf("%s:%s: unable to read detector status\n", driverName, functionName);
        return;
    }

   /* Create the epicsEvents for signaling to the acquisition task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s: epicsEventCreate failure for start event\n", 
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s: epicsEventCreate failure for stop event\n", 
            driverName, functionName);
        return;
    }
    
    // options can include a list of files to open when launching LightField
    printf("LFTest1, creating list\n");
    List<String^>^ options = gcnew List<String^>();
    printf("LFTest1, creating automation\n");
    Automation_ = gcnew PrincetonInstruments::LightField::Automation::Automation(true, options);   

    // Get the application interface from the automation
    printf("LFTest1, getting application\n");
 	  Application_ = Automation_->LightFieldApplication;

    // Get the experiment interface from the application
    Experiment_  = Application_->Experiment;

    // Tell the application to suppress prompts (overwrite file names, etc...)
    Application_->UserInteractionManager->SuppressUserInteraction = true;

    // Try to connect to a camera
    bool bCameraFound = false;
    CString cameraName;
    ////////////////////////////////////////////////////////////////////////////////
    // Look for a camera already added to the experiment
    List<PrincetonInstruments::LightField::AddIns::IDevice^> experimentList = Experiment_->ExperimentDevices;        
    for each(IDevice^% device in experimentList)
    {
        if (device->Type == PrincetonInstruments::LightField::AddIns::DeviceType::Camera)
        {
            // Cache the name
            cameraName = device->Model;
            
            // Break loop on finding camera
            bCameraFound = true;
            printf("LFTest1, found camera %s\n", cameraName);
            break;
        }
    }
    if (!bCameraFound)
    {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error, cannot find camera\n",
            driverName, functionName);
        return;
    }

    /* Set some default values for parameters */
    // Get The Full Size
    RegionOfInterest full = Experiment_->FullSensorRegion;

    // Print out the ROI information
    printf("X=%d, Width=%d, XBinning=%d, Y=%d, Height=%d, YBinning=%d\n",
      full.X, full.Width, full.XBinning, full.Y, full.Height, full.YBinning);

    setIntegerParam(ADMaxSizeX, full.Width);
    setIntegerParam(ADMaxSizeY, full.Height);
    status =  setStringParam (ADManufacturer, "Princeton Instruments");
    status |= setStringParam (ADModel, cameraName);
    status |= setIntegerParam(ADImageMode, ADImageSingle);
    status |= setDoubleParam (ADAcquireTime, .1);
    status |= setDoubleParam (ADAcquirePeriod, .5);
    status |= setIntegerParam(ADNumImages, 1);
    status |= setIntegerParam(LightFieldNumAcquisitions, 1);
    status |= setIntegerParam(LightFieldNumAcquisitionsCounter, 0);
    if (status) {
        printf("%s:%s: unable to set camera parameters\n", driverName, functionName);
        return;
    }
    
    /* Create the thread that updates the images */
    status = (epicsThreadCreate("LightFieldTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)LightFieldTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s: epicsThreadCreate failure for LightField task\n", 
            driverName, functionName);
        return;
    }
}

/* Code for iocsh registration */
static const iocshArg LightFieldConfigArg0 = {"Port name", iocshArgString};
static const iocshArg LightFieldConfigArg1 = {"maxBuffers", iocshArgInt};
static const iocshArg LightFieldConfigArg2 = {"maxMemory", iocshArgInt};
static const iocshArg LightFieldConfigArg3 = {"priority", iocshArgInt};
static const iocshArg LightFieldConfigArg4 = {"stackSize", iocshArgInt};
static const iocshArg * const LightFieldConfigArgs[] =  {&LightFieldConfigArg0,
                                                    &LightFieldConfigArg1,
                                                    &LightFieldConfigArg2,
                                                    &LightFieldConfigArg3,
                                                    &LightFieldConfigArg4};
static const iocshFuncDef configLightField = {"LightFieldConfig", 5, LightFieldConfigArgs};
static void configLightFieldCallFunc(const iocshArgBuf *args)
{
    LightFieldConfig(args[0].sval, args[1].ival, args[2].ival,
                args[3].ival, args[4].ival);
}


static void LightFieldRegister(void)
{
    iocshRegister(&configLightField, configLightFieldCallFunc);
}

extern "C" {
epicsExportRegistrar(LightFieldRegister);
}
