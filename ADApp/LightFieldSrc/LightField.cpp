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
    LightField(const char *portName, const char *experimentName,
               int maxBuffers, size_t maxMemory,
               int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t nChars, size_t *nActual);
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
    
    //bool acquisitionComplete_;
    //void completionEventHandler(System::Object^ sender, 
     //                          ExperimentCompletedEventArgs^ args);
    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    char errorMessage[ERROR_MESSAGE_SIZE];
};


#define NUM_LIGHTFIELD_PARAMS ((int)(&LAST_LIGHTFIELD_PARAM - &FIRST_LIGHTFIELD_PARAM + 1))

static bool acquisitionComplete_;
void completionEventHandler(System::Object^ sender, ExperimentCompletedEventArgs^ args)
{
    acquisitionComplete_ = true;
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
    if (minX < 0) {
        minX = 0; 
        status = setIntegerParam(ADMinX, minX);
    }
    if (minY < 0) {
        minY = 0; 
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
    sizeX = (sizeX/binX) * binX;
    sizeY = (sizeY/binY) * binY;
    status = setIntegerParam(ADSizeX, sizeX);
    status = setIntegerParam(ADSizeY, sizeY);
    RegionOfInterest^ roi = gcnew RegionOfInterest(minX, minY, sizeX, sizeY, binX, binY);

    // Create an array that can contain many regions (simple example 1)
    array<RegionOfInterest>^ rois = gcnew array<RegionOfInterest>(1);

    // Fill in the array element(s)
    rois[0] = *roi;

    // Set the custom regions
    Experiment_->SetCustomRegions(rois);  
    
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
        acquisitionComplete_ = false;
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
                acquire = !acquisitionComplete_;
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
    int needReadStatus=1;
    const char* functionName="writeInt32";

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
            if (Experiment_->Exists(ExperimentSettings::AcquisitionFramesToStore) &&
                Experiment_->IsValid(ExperimentSettings::AcquisitionFramesToStore, value))
                Experiment_->SetValue(ExperimentSettings::AcquisitionFramesToStore, value);
        } else if (function == ADNumExposures) {
            if (Experiment_->Exists(CameraSettings::ReadoutControlAccumulations) &&
                Experiment_->IsValid(CameraSettings::ReadoutControlAccumulations, value))
                Experiment_->SetValue(CameraSettings::ReadoutControlAccumulations, value);
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
            }
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_LIGHTFIELD_PARAM) status = ADDriver::writeInt32(pasynUser, value);
        }
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: function=%d, value=%d, exception = %s\n", 
            driverName, functionName, function, value, pEx->ToString());
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
    int needReadStatus=1;
    const char* functionName="writeFloat64";

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    try {
        if (function == ADAcquireTime) {
        if (Experiment_->Exists(CameraSettings::ShutterTimingExposureTime) &&
            Experiment_->IsValid(CameraSettings::ShutterTimingExposureTime, value*1000.))
                // LightField units are ms 
                Experiment_->SetValue(CameraSettings::ShutterTimingExposureTime, value*1000.);
        } else if (function == ADTemperature) {
            if (Experiment_->Exists(CameraSettings::SensorTemperatureSetPoint) &&
                Experiment_->IsValid(CameraSettings::SensorTemperatureSetPoint, value))
                Experiment_->SetValue(CameraSettings::SensorTemperatureSetPoint, value);
        } else if (function == ADGain) {
            PrincetonInstruments::LightField::AddIns::AdcGain gain;
            if (value <= 1.5)      gain = AdcGain::Low;
            else if (value <= 2.5) gain = AdcGain::Medium;
            else                   gain = AdcGain::High;
            if (Experiment_->Exists(CameraSettings::AdcAnalogGain) &&
                Experiment_->IsValid(CameraSettings::AdcAnalogGain, gain))
                Experiment_->SetValue(CameraSettings::AdcAnalogGain, gain);
        } else {
            needReadStatus = 0;
            /* If this parameter belongs to a base class call its method */
            if (function < FIRST_LIGHTFIELD_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
        }
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: function=%d, value=%f, exception = %s\n", 
            driverName, functionName, function, value, pEx->ToString());
        status = asynError;
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


 /** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including ADFilePath, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus LightField::writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    /* Set the parameter in the parameter library. */
    status = (asynStatus)setStringParam(function, (char *)value);

    if (function == NDFilePath) {
        // Set the file path value
        Experiment_->SetValue(ExperimentSettings::FileNameGenerationDirectory, gcnew String (value));    
    } else if (function == NDFileName) {
        // Set the Base file name value
        Experiment_->SetValue(ExperimentSettings::FileNameGenerationBaseFileName, gcnew String (value));    
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_LIGHTFIELD_PARAM) status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }
    
     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks();

    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, value=%s", 
                  driverName, functionName, status, function, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, value=%s\n", 
              driverName, functionName, function, value);
    *nActual = nChars;
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

extern "C" int LightFieldConfig(const char *portName, const char *experimentName,
                           int maxBuffers, size_t maxMemory,
                           int priority, int stackSize)
{
    new LightField(portName, experimentName, maxBuffers, maxMemory, priority, stackSize);
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
LightField::LightField(const char *portName, const char* experimentName,
             int maxBuffers, size_t maxMemory,
             int priority, int stackSize)

    : ADDriver(portName, 1, NUM_LIGHTFIELD_PARAMS, maxBuffers, maxMemory, 
               0, 0,             /* No interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)

{
    int status = asynSuccess;
    const char *functionName = "LightField";

    acquisitionComplete_ = true;

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
    List<String^>^ options = gcnew List<String^>();
    Automation_ = gcnew PrincetonInstruments::LightField::Automation::Automation(true, options);   

    // Get the application interface from the automation
 	  Application_ = Automation_->LightFieldApplication;

    // Get the experiment interface from the application
    Experiment_  = Application_->Experiment;
    
    // Open the user-specified experiment, if any
    if (experimentName && strlen(experimentName) > 0) {
        Experiment_->Load(gcnew String (experimentName));
    }

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
        if (device->Type == DeviceType::Camera)
        {
            // Cache the name
            cameraName = device->Model;
            
            // Break loop on finding camera
            bCameraFound = true;
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
    int width = safe_cast<int>(Experiment_->GetValue(CameraSettings::SensorInformationActiveAreaWidth));
    int height = safe_cast<int>(Experiment_->GetValue(CameraSettings::SensorInformationActiveAreaHeight));

    setIntegerParam(ADMaxSizeX, width);
    setIntegerParam(ADMaxSizeY, height);
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
    
    // Connect the acquisition event handler       
    Experiment_->ExperimentCompleted += gcnew 
        System::EventHandler<ExperimentCompletedEventArgs^>(&completionEventHandler);

    // Don't Automatically Attach Date/Time to the file name
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationAttachDate, false);
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationAttachTime, false);
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationAttachIncrement, false);

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
static const iocshArg LightFieldConfigArg1 = {"Experiment name", iocshArgString};
static const iocshArg LightFieldConfigArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg LightFieldConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg LightFieldConfigArg4 = {"priority", iocshArgInt};
static const iocshArg LightFieldConfigArg5 = {"stackSize", iocshArgInt};
static const iocshArg * const LightFieldConfigArgs[] =  {&LightFieldConfigArg0,
                                                         &LightFieldConfigArg1,
                                                         &LightFieldConfigArg2,
                                                         &LightFieldConfigArg3,
                                                         &LightFieldConfigArg4,
                                                         &LightFieldConfigArg5};
static const iocshFuncDef configLightField = {"LightFieldConfig", 6, LightFieldConfigArgs};
static void configLightFieldCallFunc(const iocshArgBuf *args)
{
    LightFieldConfig(args[0].sval, args[1].sval, args[2].ival,
                args[3].ival, args[4].ival, args[5].ival);
}


static void LightFieldRegister(void)
{
    iocshRegister(&configLightField, configLightFieldCallFunc);
}

extern "C" {
epicsExportRegistrar(LightFieldRegister);
}
