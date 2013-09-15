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
#include <ellLib.h>
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

static const char *driverName = "LightField";

/** Driver-specific parameters for the Lightfield driver */
#define LFGainString                   "LF_GAIN"
#define LFNumAccumulationsString       "LF_NUM_ACCUMULATIONS"
#define LFNumAcquisitionsString        "LF_NUM_ACQUISITIONS"
#define LFGratingString                "LF_GRATING"
#define LFGratingWavelengthString      "LF_GRATING_WAVELENGTH"
#define LFEntranceSideWidthString      "LF_ENTRANCE_SIDE_WIDTH"
#define LFExitSelectedString           "LF_EXIT_SELECTED"
#define LFExperimentNameString         "LF_EXPERIMENT_NAME"
#define LFShutterModeString            "LF_SHUTTER_MODE"
#define LFBackgroundFileString         "LF_BACKGROUND_FILE"
#define LFBackgroundEnableString       "LF_BACKGROUND_ENABLE"
#define LFGatingModeString             "LF_GATING_MODE"
#define LFTriggerFrequencyString       "LF_TRIGGER_FREQUENCY"
#define LFGateWidthString              "LF_GATE_WIDTH"
#define LFGateDelayString              "LF_GATE_DELAY"

typedef enum {
    LFSettingInt32,
    LFSettingInt64,
    LFSettingEnum,
    LFSettingBoolean,
    LFSettingDouble,
    LFSettingString,
    LFSettingPulseWidth,
    LFSettingPulseDelay
} LFSetting_t;

typedef struct {
    ELLNODE      node;
    gcroot<String^> setting;
    int             epicsParam;
    asynParamType   epicsType;
    LFSetting_t     LFType;
    bool            exists;
} settingMap;

/** Driver for Princeton Instruments cameras using the LightField Automation software */
class LightField : public ADDriver {
public:
    LightField(const char *portName, const char *experimentName,
               int maxBuffers, size_t maxMemory,
               int priority, int stackSize);
                 
    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readEnum(asynUser *pasynUser, char *strings[], int values[], int severities[], 
                            size_t nElements, size_t *nIn);
    virtual void setShutter(int open);
    virtual void report(FILE *fp, int details);
    void setAcquisitionComplete();
    void frameCallback(ImageDataSetReceivedEventArgs^ args);
    void settingChangedCallback(SettingChangedEventArgs^ args);

protected:
    int LFGain_;
    #define FIRST_LF_PARAM LFGain_
    int LFNumAccumulations_;
    int LFNumAcquisitions_;
    int LFGrating_;
    int LFGratingWavelength_;
    int LFEntranceSideWidth_;
    int LFExitSelected_;
    int LFExperimentName_;
    int LFShutterMode_;
    int LFBackgroundFile_;
    int LFBackgroundEnable_;
    int LFGatingMode_;
    int LFTriggerFrequency_;
    int LFGateWidth_;
    int LFGateDelay_;
    #define LAST_LF_PARAM LFGateDelay_
         
private:                               
    gcroot<PrincetonInstruments::LightField::Automation::Automation ^> Automation_;
    gcroot<ILightFieldApplication^> Application_;
    gcroot<IExperiment^> Experiment_;
    asynStatus setExperimentInteger(String^ setting, epicsInt32 value);
    asynStatus setExperimentInteger(int param, epicsInt32 value);
    asynStatus setExperimentDouble(String^ setting, epicsFloat64 value);
    asynStatus setExperimentDouble(int param, epicsFloat64 value);
    asynStatus setExperimentString(String^ setting, String^ value);
    asynStatus getExperimentValue(String^ setting);
    asynStatus getExperimentValue(int param);
    asynStatus getExperimentValue(settingMap *ps);
    asynStatus openExperiment(const char *experimentName);
    asynStatus setROI();
    asynStatus startAcquire();
    asynStatus addSetting(int param, String^ setting, asynParamType epicsType, LFSetting_t LFType);
    List<String^>^ buildFeatureList(String^ feature);
    gcroot<List<String^>^> experimentList_;
    gcroot<List<String^>^> gratingList_;
    ELLLIST settingList_;
};

// We use a static variable to hold a pointer to the LightField driver object
// This is OK because we can only have a single driver object per IOC
// We need this because it is difficult (impossible?) to pass the object pointer
// to the LightField object in their callback functions
static LightField *LightField_;

#define NUM_LF_PARAMS ((int)(&LAST_LF_PARAM - &FIRST_LF_PARAM + 1))

void completionEventHandler(System::Object^ sender, ExperimentCompletedEventArgs^ args)
{
    LightField_->setAcquisitionComplete();
}

void imageDataEventHandler(System::Object^ sender, ImageDataSetReceivedEventArgs^ args)
{
    LightField_->frameCallback(args);
}

void settingChangedEventHandler(System::Object^ sender, SettingChangedEventArgs^ args)
{
    LightField_->settingChangedCallback(args);
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

    : ADDriver(portName, 1, NUM_LF_PARAMS, maxBuffers, maxMemory, 
               asynEnumMask,             /* Interfaces beyond those set in ADDriver.cpp */
               asynEnumMask,             /* Interfaces beyond those set in ADDriver.cpp */
               ASYN_CANBLOCK, 1, /* ASYN_CANBLOCK=1, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)

{
    int status = asynSuccess;
    const char *functionName = "LightField";

    createParam(LFGainString,                    asynParamInt32,   &LFGain_);
    createParam(LFNumAccumulationsString,        asynParamInt32,   &LFNumAccumulations_);
    createParam(LFNumAcquisitionsString,         asynParamInt32,   &LFNumAcquisitions_);
    createParam(LFGratingString,                 asynParamInt32,   &LFGrating_);
    createParam(LFGratingWavelengthString,     asynParamFloat64,   &LFGratingWavelength_);
    createParam(LFEntranceSideWidthString,       asynParamInt32,   &LFEntranceSideWidth_);
    createParam(LFExitSelectedString,            asynParamInt32,   &LFExitSelected_);
    createParam(LFExperimentNameString,          asynParamInt32,   &LFExperimentName_);
    createParam(LFShutterModeString,             asynParamInt32,   &LFShutterMode_);
    createParam(LFBackgroundFileString,          asynParamOctet,   &LFBackgroundFile_);
    createParam(LFBackgroundEnableString,        asynParamInt32,   &LFBackgroundEnable_);
    createParam(LFGatingModeString,              asynParamInt32,   &LFGatingMode_);
    createParam(LFTriggerFrequencyString,      asynParamFloat64,   &LFTriggerFrequency_);
    createParam(LFGateDelayString,             asynParamFloat64,   &LFGateDelay_);
    
    ellInit(&settingList_);
    addSetting(ADMaxSizeX,          CameraSettings::SensorInformationActiveAreaWidth,                           
                asynParamInt32, LFSettingInt32);
    addSetting(ADMaxSizeY,          CameraSettings::SensorInformationActiveAreaHeight,                          
                asynParamInt32, LFSettingInt32);
    addSetting(ADAcquireTime,       CameraSettings::ShutterTimingExposureTime,                                  
                asynParamFloat64, LFSettingDouble);
    addSetting(ADNumImages,         ExperimentSettings::AcquisitionFramesToStore,                               
                asynParamInt32, LFSettingInt64);
    addSetting(ADNumExposures,      ExperimentSettings::OnlineProcessingFrameCombinationFramesCombined,         
                asynParamInt32, LFSettingInt64);
    addSetting(ADReverseX,          ExperimentSettings::OnlineCorrectionsOrientationCorrectionFlipHorizontally, 
                asynParamInt32, LFSettingBoolean);
    addSetting(ADReverseY,          ExperimentSettings::OnlineCorrectionsOrientationCorrectionFlipVertically,   
                asynParamInt32, LFSettingBoolean);
    addSetting(ADTriggerMode,       CameraSettings::HardwareIOTriggerSource,                                    
                asynParamInt32, LFSettingEnum);
    addSetting(ADTemperature,       CameraSettings::SensorTemperatureSetPoint,                                  
                asynParamFloat64, LFSettingDouble);
    addSetting(ADTemperatureActual, CameraSettings::SensorTemperatureReading,                                   
                asynParamFloat64, LFSettingDouble);
    addSetting(LFGain_,             CameraSettings::AdcAnalogGain,                                              
                asynParamInt32, LFSettingEnum);
    addSetting(LFNumAccumulations_, CameraSettings::ReadoutControlAccumulations,                                
                asynParamInt32, LFSettingInt64);
    addSetting(LFEntranceSideWidth_,SpectrometerSettings::OpticalPortEntranceSideWidth,                         
                asynParamInt32, LFSettingInt32);
    addSetting(LFExitSelected_,     SpectrometerSettings::OpticalPortExitSelected,                              
                asynParamInt32, LFSettingEnum);
    addSetting(LFShutterMode_,      CameraSettings::ShutterTimingMode,                                          
                asynParamInt32, LFSettingEnum);
    addSetting(LFBackgroundEnable_, ExperimentSettings::OnlineCorrectionsBackgroundCorrectionEnabled,           
                asynParamInt32, LFSettingBoolean);
    addSetting(LFGratingWavelength_,SpectrometerSettings::GratingCenterWavelength,                              
                asynParamFloat64, LFSettingDouble);
    addSetting(LFGatingMode_,       CameraSettings::IntensifierGatingMode,                              
                asynParamInt32, LFSettingEnum);
    addSetting(LFTriggerFrequency_, CameraSettings::HardwareIOTriggerFrequency,                              
                asynParamFloat64, LFSettingDouble);
    addSetting(LFGateWidth_,        CameraSettings::IntensifierGatingRepetitiveGate,                              
                asynParamFloat64, LFSettingPulseWidth);
    addSetting(LFGateDelay_,        CameraSettings::IntensifierGatingRepetitiveGate,                              
                asynParamFloat64, LFSettingPulseDelay);
 
    // options can include a list of files to open when launching LightField
    List<String^>^ options = gcnew List<String^>();
    Automation_ = gcnew PrincetonInstruments::LightField::Automation::Automation(true, options);   

    // Get the application interface from the automation
 	  Application_ = Automation_->LightFieldApplication;

    // Get the experiment interface from the application
    Experiment_  = Application_->Experiment;
    
   // Tell the application to suppress prompts (overwrite file names, etc...)
    Application_->UserInteractionManager->SuppressUserInteraction = true;

    // Open the experiment
    openExperiment(experimentName);

    // Set the static object pointer
    LightField_ = this;
}

asynStatus LightField::addSetting(int param, String^ setting, asynParamType epicsType, LFSetting_t LFType)
{
    settingMap *ps = new settingMap;
    ps->epicsParam = param;
    ps->setting = gcnew String(setting);
    ps->epicsType = epicsType;
    ps->LFType = LFType;
    ellAdd(&settingList_, &ps->node);
    return asynSuccess;
}

asynStatus LightField::openExperiment(const char *experimentName) 
{
    static const char *functionName = "openExperiment";
    
    //  It is legal to pass an empty string, in which case the default experiment is used
    if (experimentName && (strlen(experimentName) > 0)) {
        Experiment_->Load(gcnew String (experimentName));
    }

    // Try to connect to a camera
    bool bCameraFound = false;
    CString cameraName;
    // Look for a camera already added to the experiment
    List<PrincetonInstruments::LightField::AddIns::IDevice^> deviceList = Experiment_->ExperimentDevices;        
    for each(IDevice^% device in deviceList)
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
        return asynError;
    }

    setStringParam (ADManufacturer, "Princeton Instruments");
    setStringParam (ADModel, cameraName);

    // Connect the acquisition event handler       
    Experiment_->ExperimentCompleted += 
        gcnew System::EventHandler<ExperimentCompletedEventArgs^>(&completionEventHandler);

    // Connect the image data event handler       
    Experiment_->ImageDataSetReceived +=
        gcnew System::EventHandler<ImageDataSetReceivedEventArgs^>(&imageDataEventHandler);

    // Connect the setting changed event handler       
    Experiment_->SettingChanged +=
        gcnew System::EventHandler<SettingChangedEventArgs^>(&settingChangedEventHandler);

    // Enable online orientation corrections
    setExperimentInteger(ExperimentSettings::OnlineCorrectionsOrientationCorrectionEnabled, true);

    // Don't Automatically Attach Date/Time to the file name
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachDate, false);
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachTime, false);
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachIncrement, false);

    experimentList_ = gcnew List<String^>(Experiment_->GetSavedExperiments());
    gratingList_    = gcnew List<String^>(buildFeatureList(SpectrometerSettings::GratingSelected));
        
    // Read the settings from the camera and ask for callbacks on each setting
    List<String^>^ filterList = gcnew List<String^>;
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        ps->exists = Experiment_->Exists(ps->setting);
        if (ps->exists) {
            getExperimentValue(ps);
            filterList->Add(ps->setting);
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    Experiment_->FilterSettingChanged(filterList);
    
    return asynSuccess;
}


void LightField::setAcquisitionComplete()
{
    lock();
    setIntegerParam(ADAcquire, 0);
    callParamCallbacks();
    unlock();
}

List<String^>^ LightField::buildFeatureList(String^ feature)
{
    List<String^>^ list = gcnew List<String^>;
    if (Experiment_->Exists(feature)) {
        IList<Object^>^ objList = Experiment_->GetCurrentCapabilities(feature);
        for each(Object^% obj in objList) {
          list->Add(dynamic_cast<String^>(obj));
        }
    }
    return list;
}

void LightField::settingChangedCallback(SettingChangedEventArgs^ args)
{
    static const char *functionName = "settingChangedCallback";
    
    lock();
    getExperimentValue(args->Setting);
    unlock();
}


//_____________________________________________________________________________________________
/** callback function that is called by XISL every frame at end of data transfer */
void LightField::frameCallback(ImageDataSetReceivedEventArgs^ args)
{
  NDArrayInfo   arrayInfo;
  int           arrayCounter;
  int           imageCounter;
  char          *pInput;
  size_t        dims[2];
  NDArray       *pImage;
  NDDataType_t  dataType;
  epicsTimeStamp currentTime;
  static const char *functionName = "frameCallback";
    
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
    "%s:%s: entry ...\n",
    driverName, functionName);

  IImageDataSet^ dataSet = args->ImageDataSet;
  IImageData^ frame = dataSet->GetFrame(0, 0); 
  Array^ array = frame->GetData();
  switch (frame->Format) {
    case PixelDataFormat::MonochromeUnsigned16: {
      dataType = NDUInt16;
      cli::array<epicsUInt16>^ data = dynamic_cast<cli::array<epicsUInt16>^>(array);
      pin_ptr<epicsUInt16> pptr = &data[0];
      pInput = (char *)pptr;
      break;
    }
    case PixelDataFormat::MonochromeUnsigned32: {
      dataType = NDUInt32;
      cli::array<epicsUInt32>^ data = dynamic_cast<cli::array<epicsUInt32>^>(array);
      pin_ptr<epicsUInt32> pptr = &data[0];
      pInput = (char *)pptr;
      break;
    }
    case PixelDataFormat::MonochromeFloating32: {
      dataType = NDFloat32;
      cli::array<epicsFloat32>^ data = dynamic_cast<cli::array<epicsFloat32>^>(array);
      pin_ptr<epicsFloat32> pptr = &data[0];
      pInput = (char *)pptr;
      break;
    }
  }

  lock();

  /* Update the image */
  /* We save the most recent image buffer so it can be used in the read() function.
   * Now release it before getting a new version. */
  if (this->pArrays[0])
      this->pArrays[0]->release();
  /* Allocate the array */
  dims[0] = frame->Width;
  dims[1] = frame->Height;
  this->pArrays[0] = pNDArrayPool->alloc(2, dims, dataType, 0, NULL);
  if (this->pArrays[0] == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
      "%s:%s: error allocating buffer\n",
      driverName, functionName);
    unlock();
    return;
  }
  pImage = this->pArrays[0];
  pImage->getInfo(&arrayInfo);
  // Copy the data from the input to the output
  memcpy(pImage->pData, pInput, arrayInfo.totalBytes);

  setIntegerParam(NDDataType, dataType);
  setIntegerParam(NDArraySize,  (int)arrayInfo.totalBytes);
  setIntegerParam(NDArraySizeX, (int)pImage->dims[0].size);
  setIntegerParam(NDArraySizeY, (int)pImage->dims[1].size);

  getIntegerParam(ADNumImagesCounter, &imageCounter);
  imageCounter++;
  setIntegerParam(ADNumImagesCounter, imageCounter);

  /* Put the frame number and time stamp into the buffer */
  getIntegerParam(NDArrayCounter, &arrayCounter);
  arrayCounter++;
  setIntegerParam(NDArrayCounter, arrayCounter);
  pImage->uniqueId = arrayCounter;
  epicsTimeGetCurrent(&currentTime);
  pImage->timeStamp = currentTime.secPastEpoch + currentTime.nsec / 1.e9;
  updateTimeStamp(&pImage->epicsTS);

  /* Get any attributes that have been defined for this driver */
  getAttributes(pImage->pAttributeList);

  /* Call the NDArray callback */
  /* Must release the lock here, or we can get into a deadlock, because we can
   * block on the plugin lock, and the plugin can be calling us */
  unlock();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
    "%s:%s: calling imageData callback\n", 
    driverName, functionName);
  doCallbacksGenericPointer(pImage, NDArrayData, 0);
  lock();

  // Do callbacks on parameters
  callParamCallbacks();

  unlock();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
    "%s:%s: exit\n",
    driverName, functionName);
}

asynStatus LightField::startAcquire()
{
    size_t len;
    int imageMode;
    char filePath[MAX_FILENAME_LEN], fileName[MAX_FILENAME_LEN];

    /* Set the file name and path */
    createFileName(MAX_FILENAME_LEN, filePath, fileName);
    // Remove trailing \ or / because LightField won't accept it
    len = strlen(filePath);
    if (len > 0) filePath[len-1] = 0;        
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationDirectory, gcnew String (filePath));    
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationBaseFileName, gcnew String (fileName));    

    // Start acquisition 
    getIntegerParam(ADImageMode, &imageMode);
    if (imageMode == ADImageContinuous) {
        Experiment_->Preview();
    } else {
        Experiment_->Acquire();
    }
    return asynSuccess;
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

asynStatus LightField::setExperimentInteger(int param, epicsInt32 value)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->epicsParam == param) {
            return setExperimentInteger(ps->setting, value);
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return asynError;
}

asynStatus LightField::setExperimentInteger(String^ setting, epicsInt32 value)
{
    static const char *functionName = "setExperimentInteger";
    try {
        if (Experiment_->Exists(setting) &&
            ((setting == SpectrometerSettings::OpticalPortExitSelected) || Experiment_->IsValid(setting, value)))
            Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%d, exception = %s\n", 
            driverName, functionName, (CString)setting, value, pEx->ToString());
        return asynError;
    }
    return asynSuccess;
}

asynStatus LightField::setExperimentDouble(int param, epicsFloat64 value)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->epicsParam == param) {
            return setExperimentDouble(ps->setting, value);
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return asynError;
}

asynStatus LightField::setExperimentDouble(String^ setting, epicsFloat64 value)
{
    static const char *functionName = "setExperimentDouble";
    try {
        if (Experiment_->Exists(setting) &&
            Experiment_->IsValid(setting, value))
            Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%f, exception = %s\n", 
            driverName, functionName, (CString)setting, value, pEx->ToString());
        return asynError;
    }
    return asynSuccess;
}

asynStatus LightField::setExperimentString(String^ setting, String^ value)
{
    static const char *functionName = "setExperimentString";
    try {
        if (Experiment_->Exists(setting) &&
            ((setting == SpectrometerSettings::GratingSelected) || Experiment_->IsValid(setting, value)))
            Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%s, exception = %s\n", 
            driverName, functionName, (CString)setting, (CString)value, pEx->ToString());
        return asynError;
    }
    return asynSuccess;
}


asynStatus LightField::getExperimentValue(String^ setting)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->setting == setting) {
            return getExperimentValue(ps);
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return asynError;
}

asynStatus LightField::getExperimentValue(int param)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->epicsParam == param) {
            return getExperimentValue(ps);
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return asynError;
}

asynStatus LightField::getExperimentValue(settingMap *ps)
{
    static const char *functionName = "getExperimentValue";
    String^ setting = ps->setting;
    CString settingName = CString(setting);
    
    if (!ps->exists) return asynSuccess;

    Object^ obj = Experiment_->GetValue(ps->setting);

    try {
        switch (ps->epicsType) {
            case asynParamInt32: {
                epicsInt32 value;
                switch (ps->LFType) {
                    case LFSettingInt64: {
                        __int64 temp = safe_cast<__int64>(obj);
                        value = (epicsInt32)temp;
                        break;
                    }
                    case LFSettingInt32: {
                        __int32 temp = safe_cast<__int32>(obj);
                        value = (epicsInt32)temp;
                        break;
                    }
                    case LFSettingBoolean: {
                        bool temp = safe_cast<bool>(obj);
                        value = (epicsInt32)temp;
                        break;
                    }
                    case LFSettingEnum: {
                        __int32 temp = safe_cast<__int32>(obj);
                        value = (epicsInt32)temp;
                        break;
                    }
                    default:
                        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                            "%s:%s: setting=%s, asynInt32, unknown LFSetting_t=%d\n",
                            driverName, functionName, ps->LFType);
                        break;
                }
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s:%s: setting=%s, param=%d, value=%d\n",
                    driverName, functionName, settingName, ps->epicsParam, value);
                setIntegerParam(ps->epicsParam, value);
                break;
            }
            case asynParamFloat64: {
                epicsFloat64 value = safe_cast<epicsFloat64>(obj);
                // Convert exposure time from ms to s
                if (setting == CameraSettings::ShutterTimingExposureTime) value = value/1000.;
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s:%s: setting=%s, param=%d, value=%f\n",
                    driverName, functionName, settingName, ps->epicsParam, value);
                setDoubleParam(ps->epicsParam, value);
                break;
            }
            case asynParamOctet: {
                String^ value = safe_cast<String^>(obj);
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s:%s: setting=%s, param=%d, value=%s\n",
                    driverName, functionName, settingName, ps->epicsParam, (CString)value);
                setStringParam(ps->epicsParam, (CString)value);
                break;
            }
        }
        callParamCallbacks();
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, param=%d, exception = %s\n", 
            driverName, functionName, settingName, ps->epicsParam, pEx->ToString());
        return asynError;
    }
    return asynSuccess;
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
    asynStatus status = asynSuccess;
    const char* functionName="writeInt32";

    /* See if we are currently acquiring.  This must be done before the call to setIntegerParam below */
    getIntegerParam(ADAcquire, &currentlyAcquiring);
    
    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setIntegerParam(function, value);

    if (function == ADAcquire) {
        if (value && !currentlyAcquiring) {
            startAcquire();
        } 
        if (!value && currentlyAcquiring) {
            /* This was a command to stop acquisition */
            Experiment_->Stop();
        }
    } else if ( (function == ADBinX) ||
                (function == ADBinY) ||
                (function == ADMinX) ||
                (function == ADMinY) ||
                (function == ADSizeX) ||
                (function == ADSizeY)) {
        this->setROI();
    } else if ( (function == ADNumImages) ||
                (function == ADNumExposures) ||
                (function == LFNumAccumulations_) ||
                (function == ADReverseX) ||
                (function == ADReverseY) ||
                (function == ADTriggerMode) ||
                (function == LFGain_) ||
                (function == LFShutterMode_) ||
                (function == LFEntranceSideWidth_) ||
                (function == LFExitSelected_) ||
                (function == LFBackgroundEnable_) ||
                (function == LFGatingMode_) ) {
        status = setExperimentInteger(function, value); 
     } else if (function == LFGrating_) {
        List<String^>^ list = gratingList_;
        String^ grating = list[value];
        status = setExperimentString(SpectrometerSettings::GratingSelected, grating);
    } else if (function == LFExperimentName_) {
        List<String^>^ list = experimentList_;
        String^ experimentName = list[value];
        status = openExperiment((CString)experimentName);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_LF_PARAM) status = ADDriver::writeInt32(pasynUser, value);
    }
    
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
    const char* functionName="writeFloat64";

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    if (function == ADAcquireTime) {
        // LightField units are ms 
        value = value*1000;
    }
    if ((function == ADAcquireTime) ||
        (function == ADTemperature) ||
        (function == LFGratingWavelength_) ||
        (function == LFTriggerFrequency_))
        status = setExperimentDouble(function, value);
    else if (function == LFGateWidth_) {
    }
    else if (function == LFGateDelay_) {
    } 
    else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_LF_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }

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

asynStatus LightField::readEnum(asynUser *pasynUser, char *strings[], int values[], int severities[], 
                            size_t nElements, size_t *nIn)
{
  int index = pasynUser->reason;
  static const char *functionName = "readEnum";
  List<String^>^ list;

  *nIn = 0;
  if (index == LFExperimentName_) {
    list = experimentList_;
  }
  else if (index == LFGrating_) {
    list = gratingList_;
  }
  else {
    return asynError;
  }

  if (list->Count > 0) {
    for each(String^% str in list) {
      CString enumString = str;
      if (strings[*nIn]) free(strings[*nIn]);
      strings[*nIn] = epicsStrDup(enumString);
      values[*nIn] = (int)*nIn;
      severities[*nIn] = 0;
      (*nIn)++;
      if (*nIn >= nElements) break;
    }
  }
  else {
    strings[0] = epicsStrDup("N.A. 0");
    values[0] = 0;
    severities[0] = 0;
    (*nIn) = 1;
  }
  
  return asynSuccess;
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
