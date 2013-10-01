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
#include <sys/stat.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <ellLib.h>
#include <epicsExit.h>
#include <iocsh.h>

#include "ADDriver.h"

#include <epicsExport.h>

#define LF_POLL_TIME 1.0
#define MAX_ENUM_STATES 16

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
#define LFUpdateExperimentsString      "LF_UPDATE_EXPERIMENTS"
#define LFShutterModeString            "LF_SHUTTER_MODE"
#define LFBackgroundPathString         "LF_BACKGROUND_PATH"
#define LFBackgroundFileString         "LF_BACKGROUND_FILE"
#define LFBackgroundFullFileString     "LF_BACKGROUND_FULL_FILE"
#define LFBackgroundEnableString       "LF_BACKGROUND_ENABLE"
#define LFIntensifierEnableString      "LF_INTENSIFIER_ENABLE"
#define LFIntensifierGainString        "LF_INTENSIFIER_GAIN"
#define LFGatingModeString             "LF_GATING_MODE"
#define LFTriggerFrequencyString       "LF_TRIGGER_FREQUENCY"
#define LFSyncMasterEnableString       "LF_SYNCMASTER_ENABLE"
#define LFSyncMaster2DelayString       "LF_SYNCMASTER2_DELAY"
#define LFRepGateWidthString           "LF_REP_GATE_WIDTH"
#define LFRepGateDelayString           "LF_REP_GATE_DELAY"
#define LFSeqStartGateWidthString      "LF_SEQ_START_GATE_WIDTH"
#define LFSeqStartGateDelayString      "LF_SEQ_START_GATE_DELAY"
#define LFSeqEndGateWidthString        "LF_SEQ_END_GATE_WIDTH"
#define LFSeqEndGateDelayString        "LF_SEQ_END_GATE_DELAY"
#define LFAuxWidthString               "LF_AUX_WIDTH"
#define LFAuxDelayString               "LF_AUX_DELAY"
#define LFReadyToRunString             "LF_READY_TO_RUN"
#define LFFilePathString               "LF_FILE_PATH"
#define LFFileNameString               "LF_FILE_NAME"

typedef enum {
    LFImageModeNormal,
    LFImageModePreview,
    LFImageModeBackground
} LFImageMode_t;

typedef enum {
    LFSettingInt32,
    LFSettingInt64,
    LFSettingEnum,
    LFSettingBoolean,
    LFSettingDouble,
    LFSettingString,
    LFSettingPulse,
    LFSettingROI
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
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, 
                            size_t nChars, size_t *nActual);
    virtual asynStatus readEnum(asynUser *pasynUser, char *strings[], int values[], int severities[], 
                            size_t nElements, size_t *nIn);
    virtual void setShutter(int open);
    virtual void report(FILE *fp, int details);
    void setAcquisitionComplete();
    void frameCallback(ImageDataSetReceivedEventArgs^ args);
    void settingChangedCallback(SettingChangedEventArgs^ args);
    void exitHandler(void *args);
    void pollerTask();

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
    int LFUpdateExperiments_;
    int LFShutterMode_;
    int LFBackgroundPath_;
    int LFBackgroundFile_;
    int LFBackgroundFullFile_;
    int LFBackgroundEnable_;
    int LFIntensifierEnable_;
    int LFIntensifierGain_;
    int LFGatingMode_;
    int LFTriggerFrequency_;
    int LFSyncMasterEnable_;
    int LFSyncMaster2Delay_;
    int LFRepGateWidth_;
    int LFRepGateDelay_;
    int LFSeqStartGateWidth_;
    int LFSeqStartGateDelay_;
    int LFSeqEndGateWidth_;
    int LFSeqEndGateDelay_;
    int LFAuxWidth_;
    int LFAuxDelay_;
    int LFReadyToRun_;
    int LFFilePath_;
    int LFFileName_;
    #define LAST_LF_PARAM LFFileName_
         
private:                               
    settingMap* findSettingMap(String^ setting);
    settingMap* findSettingMap(int param);
    asynStatus setFilePathAndName(bool doAutoIncrement);
    asynStatus setBackgroundFile();
    asynStatus setExperimentInteger(String^ setting, epicsInt32 value);
    asynStatus setExperimentInteger(int param, epicsInt32 value);
    asynStatus setExperimentDouble(String^ setting, epicsFloat64 value);
    asynStatus setExperimentDouble(int param, epicsFloat64 value);
    asynStatus setExperimentPulse(int param, double width, double delay);
    asynStatus setExperimentString(String^ setting, String^ value);
    asynStatus getExperimentValue(String^ setting);
    asynStatus getExperimentValue(int param);
    asynStatus getExperimentValue(settingMap *ps);
    asynStatus openExperiment(const char *experimentName);
    asynStatus getExperimentList();
    asynStatus getROI();
    asynStatus setROI();
    asynStatus startAcquire();
    asynStatus addSetting(int param, String^ setting, asynParamType epicsType, LFSetting_t LFType);
    List<String^>^ buildFeatureList(String^ feature);
    gcroot<PrincetonInstruments::LightField::Automation::Automation ^> Automation_;
    gcroot<ILightFieldApplication^> Application_;
    gcroot<IExperiment^> Experiment_;
    gcroot<List<String^>^> experimentList_;
    gcroot<List<String^>^> gratingList_;
    gcroot<String^> previousExperimentName_;
    int backgroundWasEnabled_;
    bool exiting_;
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


void LFExitHandler(void *args)
{
    LightField_->exitHandler(args);
}

void LFPollerTask(void *args)
{
    LightField_->pollerTask();
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
  * \param[in] experimentName The name of the experiment to open initially.  Set this to an empty string
  *            to open the default experiment in LightField.
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

    // Set the static object pointer
    LightField_ = this;
    
    // Set up an exit handler
    epicsAtExit(LFExitHandler, (void *)this);
    
    exiting_ = false;

    lock();
    createParam(LFGainString,                    asynParamInt32,   &LFGain_);
    createParam(LFNumAccumulationsString,        asynParamInt32,   &LFNumAccumulations_);
    createParam(LFNumAcquisitionsString,         asynParamInt32,   &LFNumAcquisitions_);
    createParam(LFGratingString,                 asynParamInt32,   &LFGrating_);
    createParam(LFGratingWavelengthString,     asynParamFloat64,   &LFGratingWavelength_);
    createParam(LFEntranceSideWidthString,       asynParamInt32,   &LFEntranceSideWidth_);
    createParam(LFExitSelectedString,            asynParamInt32,   &LFExitSelected_);
    createParam(LFExperimentNameString,          asynParamInt32,   &LFExperimentName_);
    createParam(LFUpdateExperimentsString,       asynParamInt32,   &LFUpdateExperiments_);
    createParam(LFShutterModeString,             asynParamInt32,   &LFShutterMode_);
    createParam(LFBackgroundPathString,          asynParamOctet,   &LFBackgroundPath_);
    createParam(LFBackgroundFileString,          asynParamOctet,   &LFBackgroundFile_);
    createParam(LFBackgroundFullFileString,      asynParamOctet,   &LFBackgroundFullFile_);
    createParam(LFBackgroundEnableString,        asynParamInt32,   &LFBackgroundEnable_);
    createParam(LFIntensifierEnableString,       asynParamInt32,   &LFIntensifierEnable_);
    createParam(LFIntensifierGainString,         asynParamInt32,   &LFIntensifierGain_);
    createParam(LFGatingModeString,              asynParamInt32,   &LFGatingMode_);
    createParam(LFTriggerFrequencyString,      asynParamFloat64,   &LFTriggerFrequency_);
    createParam(LFSyncMasterEnableString,        asynParamInt32,   &LFSyncMasterEnable_);
    createParam(LFSyncMaster2DelayString,      asynParamFloat64,   &LFSyncMaster2Delay_);
    // Note: the code assumes that for each pulse parameter the width and gate
    // are successive parameter numbers, so this order cannot be changed.
    createParam(LFRepGateWidthString,          asynParamFloat64,   &LFRepGateWidth_);
    createParam(LFRepGateDelayString,          asynParamFloat64,   &LFRepGateDelay_);
    createParam(LFSeqStartGateWidthString,     asynParamFloat64,   &LFSeqStartGateWidth_);
    createParam(LFSeqStartGateDelayString,     asynParamFloat64,   &LFSeqStartGateDelay_);
    createParam(LFSeqEndGateWidthString,       asynParamFloat64,   &LFSeqEndGateWidth_);
    createParam(LFSeqEndGateDelayString,       asynParamFloat64,   &LFSeqEndGateDelay_);
    createParam(LFAuxWidthString,              asynParamFloat64,   &LFAuxWidth_);
    createParam(LFAuxDelayString,              asynParamFloat64,   &LFAuxDelay_);
    createParam(LFReadyToRunString,              asynParamInt32,   &LFReadyToRun_);
    createParam(LFFilePathString,                asynParamOctet,   &LFFilePath_);
    createParam(LFFileNameString,                asynParamOctet,   &LFFileName_);

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
    addSetting(LFBackgroundFullFile_, ExperimentSettings::OnlineCorrectionsBackgroundCorrectionReferenceFile,           
                asynParamOctet, LFSettingString);
    addSetting(LFBackgroundEnable_, ExperimentSettings::OnlineCorrectionsBackgroundCorrectionEnabled,           
                asynParamInt32, LFSettingBoolean);
    addSetting(LFGrating_,          SpectrometerSettings::GratingSelected,                              
                asynParamInt32, LFSettingString);
    addSetting(LFGratingWavelength_,SpectrometerSettings::GratingCenterWavelength,                              
                asynParamFloat64, LFSettingDouble);
    addSetting(LFIntensifierEnable_, CameraSettings::IntensifierEnabled,                              
                asynParamInt32, LFSettingBoolean);
    addSetting(LFIntensifierGain_, CameraSettings::IntensifierGain,                              
                asynParamInt32, LFSettingInt32);
    addSetting(LFGatingMode_,       CameraSettings::IntensifierGatingMode,                              
                asynParamInt32, LFSettingEnum);
    addSetting(LFTriggerFrequency_, CameraSettings::HardwareIOTriggerFrequency,                              
                asynParamFloat64, LFSettingDouble);
    addSetting(LFSyncMasterEnable_, CameraSettings::HardwareIOSyncMasterEnabled,                              
                asynParamInt32, LFSettingBoolean);
    addSetting(LFSyncMaster2Delay_, CameraSettings::HardwareIOSyncMaster2Delay,                              
                asynParamFloat64, LFSettingDouble);
    addSetting(LFRepGateWidth_, CameraSettings::IntensifierGatingRepetitiveGate,                              
                asynParamFloat64, LFSettingPulse);
    addSetting(LFSeqStartGateWidth_, CameraSettings::IntensifierGatingSequentialStartingGate,                              
                asynParamFloat64, LFSettingPulse);
    addSetting(LFSeqEndGateWidth_, CameraSettings::IntensifierGatingSequentialEndingGate,                              
                asynParamFloat64, LFSettingPulse);
    addSetting(LFAuxWidth_,        CameraSettings::HardwareIOAuxOutput,                              
                asynParamFloat64, LFSettingPulse);
    addSetting(LFFilePath_,        ExperimentSettings::FileNameGenerationDirectory,                              
                asynParamOctet, LFSettingString);
    addSetting(LFFileName_,       ExperimentSettings::FileNameGenerationBaseFileName,                              
                asynParamOctet, LFSettingString);
    addSetting(ADBinX,            CameraSettings::ReadoutControlRegionsOfInterestResult,                              
                asynParamInt32, LFSettingROI);
 
 
    // options can include a list of files to open when launching LightField
    List<String^>^ options = gcnew List<String^>();
    Automation_ = gcnew PrincetonInstruments::LightField::Automation::Automation(true, options);   

    // Get the application interface from the automation
 	  Application_ = Automation_->LightFieldApplication;

    // Get the experiment interface from the application
    Experiment_  = Application_->Experiment;
    
    // Connect the acquisition event handler       
    Experiment_->ExperimentCompleted += 
        gcnew System::EventHandler<ExperimentCompletedEventArgs^>(&completionEventHandler);

    // Connect the image data event handler       
    Experiment_->ImageDataSetReceived +=
        gcnew System::EventHandler<ImageDataSetReceivedEventArgs^>(&imageDataEventHandler);

    // Connect the setting changed event handler       
    Experiment_->SettingChanged +=
        gcnew System::EventHandler<SettingChangedEventArgs^>(&settingChangedEventHandler);

   // Tell the application to suppress prompts (overwrite file names, etc...)
    Application_->UserInteractionManager->SuppressUserInteraction = true;

    previousExperimentName_ = gcnew String(Experiment_->Name);

    getExperimentList();

    // Open the experiment
    openExperiment(experimentName);
    
    /* Create the thread that polls for status */
    status = (epicsThreadCreate("LFTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)LFPollerTask,
                                this) == NULL);    
    unlock();

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
    asynStatus status = asynSuccess;
    
    setIntegerParam(ADStatus, ADStatusWaiting);
    callParamCallbacks();

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
        status = asynError;
        goto done;
    }

    setStringParam (ADManufacturer, "Princeton Instruments");
    setStringParam (ADModel, cameraName);

    // Enable online orientation corrections
    setExperimentInteger(ExperimentSettings::OnlineCorrectionsOrientationCorrectionEnabled, true);

    // Don't Automatically Attach Date/Time to the file name
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachDate, false);
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachTime, false);
    setExperimentInteger(ExperimentSettings::FileNameGenerationAttachIncrement, false);

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
    
    done:
    return status;
}

asynStatus LightField::getExperimentList()
{
    char *strings[MAX_ENUM_STATES];
    int values[MAX_ENUM_STATES];
    int severities[MAX_ENUM_STATES];
    int count = 0;
    List<String^>^ list;

    experimentList_ = gcnew List<String^>(Experiment_->GetSavedExperiments());
    list = experimentList_;
    if (list->Count > 0) {
      for each(String^% str in list) {
        CString enumString = str;
        strings[count] = epicsStrDup(enumString);
        values[count] = count;
        severities[count] = 0;
        count++;
        if (count >= MAX_ENUM_STATES) break;
      }
    }
    else {
      strings[0] = epicsStrDup("N.A.");
      values[0] = 0;
      severities[0] = 0;
      count = 1;
    }
    doCallbacksEnum(strings, values, severities, count, LFExperimentName_, 0);
    return asynSuccess;
}


void LightField::setAcquisitionComplete()
{
    int imageMode;
    
    lock();
    setIntegerParam(ADAcquire, 0);
    setIntegerParam(ADStatus, ADStatusIdle);
    /* If this is a background image restore the backgroundEnabled setting and set the background file */
    getIntegerParam(ADImageMode, &imageMode);
    if (imageMode == LFImageModeBackground) {
        setExperimentInteger(LFBackgroundEnable_, backgroundWasEnabled_);
        setBackgroundFile();
    }
        
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

void LightField::exitHandler(void *args)
{
    lock();
    exiting_ = true;
    delete Automation_;
    unlock();
}

//_____________________________________________________________________________________________
/** callback function that is called by XISL every frame at end of data transfer */
void LightField::frameCallback(ImageDataSetReceivedEventArgs^ args)
{
  NDArrayInfo   arrayInfo;
  int           arrayCounter;
  int           imageCounter;
  int           arrayCallbacks;
  char          *pInput;
  size_t        dims[2];
  NDArray       *pImage;
  NDDataType_t  dataType;
  epicsTimeStamp currentTime;
  static const char *functionName = "frameCallback";
    
  lock();

  getIntegerParam(ADNumImagesCounter, &imageCounter);
  imageCounter++;
  setIntegerParam(ADNumImagesCounter, imageCounter);

  /* Put the frame number and time stamp into the buffer */
  getIntegerParam(NDArrayCounter, &arrayCounter);
  arrayCounter++;
  setIntegerParam(NDArrayCounter, arrayCounter);

  getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
  if (arrayCallbacks) {
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
  }

  // Do callbacks on parameters
  callParamCallbacks();

  unlock();
}

asynStatus LightField::startAcquire()
{
    int imageMode;
    static const char *functionName = "startAcquire";

    getIntegerParam(ADImageMode, &imageMode);
    setIntegerParam(ADStatus, ADStatusAcquire);
    getIntegerParam(LFBackgroundEnable_, &backgroundWasEnabled_);
    callParamCallbacks();

    switch (imageMode) {
        case LFImageModeNormal:
            setFilePathAndName(true);
            if (!Experiment_->IsReadyToRun) return asynError;
            Experiment_->Acquire();
            break;
        case LFImageModePreview:
            if (!Experiment_->IsReadyToRun) return asynError;
            Experiment_->Preview();
            break;
        case LFImageModeBackground:
            setBackgroundFile();
            setExperimentInteger(LFBackgroundEnable_, 0);
            if (!Experiment_->IsReadyToRun) return asynError;
            Experiment_->Acquire();
            break;
    }
    return asynSuccess;
}

asynStatus LightField::setFilePathAndName(bool doAutoIncrement)
{
    /* Formats a complete file name from the components defined in NDStdDriverParams */
    char filePath[MAX_FILENAME_LEN];
    char fileName[MAX_FILENAME_LEN];
    char name[MAX_FILENAME_LEN];
    char fileTemplate[MAX_FILENAME_LEN];
    char fullFileName[MAX_FILENAME_LEN];
    int fileNumber;
    int autoIncrement;
    int status;
    size_t len;
    
    status = checkPath();  // This appends trailing "/" if there is no trailing "/" or "\"
    status |= getStringParam(NDFilePath, sizeof(filePath), filePath);
    if (status) return asynError; 
    // Remove trailing \ or / because LightField won't accept it
    len = strlen(filePath);
    if (len > 0) filePath[len-1] = 0;        
    status = getStringParam(NDFileName, sizeof(name), name); 
    status |= getStringParam(NDFileTemplate, sizeof(fileTemplate), fileTemplate); 
    status |= getIntegerParam(NDFileNumber, &fileNumber);
    status |= getIntegerParam(NDAutoIncrement, &autoIncrement);
    if (status) return asynError;
    len = epicsSnprintf(fileName, sizeof(fileName), fileTemplate, 
                        name, fileNumber);
    if (len < 0) return asynError;    
    len = epicsSnprintf(fullFileName, sizeof(fullFileName), "%s\\%s", 
                        filePath, fileName);
    if (len < 0) return asynError;
    if (doAutoIncrement && autoIncrement) {
        fileNumber++;
        setIntegerParam(NDFileNumber, fileNumber);
    }
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationDirectory, gcnew String (filePath));    
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationBaseFileName, gcnew String (fileName));
    setStringParam(NDFullFileName, fullFileName); 
    return asynSuccess;   
}

asynStatus LightField::setBackgroundFile()
{
    char filePath[MAX_FILENAME_LEN];
    char fileName[MAX_FILENAME_LEN];
    char fullFileName[MAX_FILENAME_LEN];
    struct stat buff;
    int stat_ret;
    asynStatus status;
    size_t len;
    
    status = getStringParam(LFBackgroundPath_, sizeof(filePath), filePath);
    if (status) return asynError; 
    // Remove trailing '\' or '/'
    len = strlen(filePath);
    if (len > 0) {
        if ((filePath[len-1] == '/') ||
            (filePath[len-1] == '\\')) {
            filePath[len-1] = 0;
        } 
    } 
    status = getStringParam(LFBackgroundFile_, sizeof(fileName), fileName); 
    if (status) return asynError;
    len = epicsSnprintf(fullFileName, sizeof(fullFileName), "%s\\%s.spe", 
                        filePath, fileName);
    if (len < 0) return asynError;
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationDirectory, gcnew String (filePath));    
    Experiment_->SetValue(ExperimentSettings::FileNameGenerationBaseFileName, gcnew String (fileName));
    setStringParam(LFBackgroundFullFile_, fullFileName); 
    // If this file actually exists then set the background correction file to this.
    // It might not exist if we have not collected it yet.
    stat_ret = stat(fullFileName, &buff);
    if (!stat_ret && (buff.st_mode & S_IFREG)) {
        Experiment_->SetValue(ExperimentSettings::OnlineCorrectionsBackgroundCorrectionReferenceFile, 
                              gcnew String(fullFileName));    
    }
    return asynSuccess;   
}


asynStatus LightField::getROI()
{
    const char *functionName = "getROI";

    array<RegionOfInterest>^ customRegions = Experiment_->CustomRegions;
    RegionOfInterest^ roi = customRegions[0];
    setIntegerParam(ADMinX, roi->X);
    setIntegerParam(ADMinY, roi->Y);
    setIntegerParam(ADSizeX, roi->Width);
    setIntegerParam(ADSizeY, roi->Height);
    setIntegerParam(ADBinX, roi->XBinning);
    setIntegerParam(ADBinY, roi->YBinning);

    return asynSuccess;
}


asynStatus LightField::setROI()
{
    int minX, minY, sizeX, sizeY, binX, binY, maxSizeX, maxSizeY;
    asynStatus status;
    const char *functionName = "setROI";

    setIntegerParam(ADStatus, ADStatusWaiting);
    callParamCallbacks();
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
    
    setIntegerParam(ADStatus, ADStatusIdle);
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

settingMap* LightField::findSettingMap(int param)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->epicsParam == param) {
            return ps;
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return 0;
}

settingMap* LightField::findSettingMap(String^ setting)
{
    settingMap *ps = (settingMap *)ellFirst(&settingList_);
    while (ps) {
        if (ps->setting == setting) {
            return ps;
        }
        ps = (settingMap *)ellNext(&ps->node);
    }
    return 0;
}

asynStatus LightField::setExperimentInteger(int param, epicsInt32 value)
{
    settingMap *ps = findSettingMap(param);
    if (ps) {
        return setExperimentInteger(ps->setting, value);
    }
    return asynError;
}

asynStatus LightField::setExperimentInteger(String^ setting, epicsInt32 value)
{
    static const char *functionName = "setExperimentInteger";
    try {
        if (!Experiment_->Exists(setting)) return asynSuccess;
        if (!Experiment_->IsValid(setting, value)) return asynError;
        setIntegerParam(ADStatus, ADStatusWaiting);
        callParamCallbacks();
        Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%d, exception = %s\n", 
            driverName, functionName, (CString)setting, value, pEx->ToString());
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    return asynSuccess;
}

asynStatus LightField::setExperimentDouble(int param, epicsFloat64 value)
{
    settingMap *ps = findSettingMap(param);
    if (ps) {
        return setExperimentDouble(ps->setting, value);
    }
    return asynError;
}

asynStatus LightField::setExperimentDouble(String^ setting, epicsFloat64 value)
{
    static const char *functionName = "setExperimentDouble";
    try {
        if (!Experiment_->Exists(setting)) return asynSuccess;
        if (!Experiment_->IsValid(setting, value)) return asynError;
        setIntegerParam(ADStatus, ADStatusWaiting);
        callParamCallbacks();
        Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%f, exception = %s\n", 
            driverName, functionName, (CString)setting, value, pEx->ToString());
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    return asynSuccess;
}

asynStatus LightField::setExperimentPulse(int param, double width, double delay)
{
    static const char *functionName = "setExperimentPulse";
    settingMap *ps = findSettingMap(param);
    if (!ps) return asynError;
    String^ setting = ps->setting;
    Pulse^ pulse = gcnew Pulse(width*1e9, delay*1e9);
    try {
        if (!Experiment_->Exists(setting)) return asynSuccess;
        if (!Experiment_->IsValid(setting, pulse)) return asynError;
        setIntegerParam(ADStatus, ADStatusWaiting);
        callParamCallbacks();
        Experiment_->SetValue(setting, pulse);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, width=%f, delay=%f, exception = %s\n", 
            driverName, functionName, (CString)setting, pulse->Width, pulse->Delay, pEx->ToString());
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    return asynSuccess;
}

asynStatus LightField::setExperimentString(String^ setting, String^ value)
{
    static const char *functionName = "setExperimentString";
    try {
        if (!Experiment_->Exists(setting)) return asynSuccess;
        if (!Experiment_->IsValid(setting, value)) return asynError;
        setIntegerParam(ADStatus, ADStatusWaiting);
        callParamCallbacks();
        Experiment_->SetValue(setting, value);
    }
    catch(System::Exception^ pEx) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: setting=%s, value=%s, exception = %s\n", 
            driverName, functionName, (CString)setting, (CString)value, pEx->ToString());
    }
    setIntegerParam(ADStatus, ADStatusIdle);
    return asynSuccess;
}


asynStatus LightField::getExperimentValue(String^ setting)
{
    settingMap *ps = findSettingMap(setting);
    if (ps) {
        return getExperimentValue(ps);
    }
    return asynError;
}

asynStatus LightField::getExperimentValue(int param)
{
    settingMap *ps = findSettingMap(param);
    if (ps) {
        return getExperimentValue(ps);
    }
    return asynError;
}

void LightField::pollerTask()
{
    static const char *functionName = "pollerTask";

    lock();
    while (!exiting_) {
        try {
            int ready = Experiment_->IsReadyToRun;
            setIntegerParam(LFReadyToRun_, ready);
            if (Experiment_->Name != previousExperimentName_) {
                previousExperimentName_ = Experiment_->Name;
                String^ name = Experiment_->Name + gcnew String(".lfe");
                int experimentIndex = experimentList_->IndexOf(name);
                if (experimentIndex >= 0) {
                    setIntegerParam(LFExperimentName_, experimentIndex);
                }
            }
        }
        catch(System::Exception^ pEx) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: exception = %s\n", 
                driverName, functionName, pEx->ToString());
        }
        callParamCallbacks();
        unlock();
        epicsThreadSleep(LF_POLL_TIME);
        lock();
    }
}
        

asynStatus LightField::getExperimentValue(settingMap *ps)
{
    static const char *functionName = "getExperimentValue";
    String^ setting = ps->setting;
    CString settingName = CString(setting);
    
    if (!ps->exists) return asynSuccess;

    try {
        Object^ obj = Experiment_->GetValue(ps->setting);

        // Special cases that don't fall into the categories below
        if (ps->epicsParam == LFGrating_) {
            String^ value = safe_cast<String^>(obj);
            int i = gratingList_->IndexOf(value);
            if (i >= 0) setIntegerParam(ps->epicsParam, i);
        }
        
        else if (ps->LFType == LFSettingROI) {
            getROI();
        }
        else {
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
                    switch (ps->LFType) {
                        case LFSettingDouble: {
                            epicsFloat64 value = safe_cast<epicsFloat64>(obj);
                            // Convert exposure time from ms to s
                            if (ps->epicsParam == ADAcquireTime) value = value/1e3;
                            // Convert SyncMaster2Delay time from us to s
                            if (ps->epicsParam == LFSyncMaster2Delay_) value = value/1e6;
                            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                                "%s:%s: setting=%s, param=%d, value=%f\n",
                                driverName, functionName, settingName, ps->epicsParam, value);
                            setDoubleParam(ps->epicsParam, value);
                            break;
                        }
                        case LFSettingPulse: {
                            Pulse^ pulse = safe_cast<Pulse^>(obj);
                            double width = pulse->Width * 1e-9;
                            double delay = pulse->Delay * 1e-9;
                            setDoubleParam(ps->epicsParam, width);
                            setDoubleParam(ps->epicsParam+1, delay);
                            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                                "%s:%s: setting=%s, param=%d, width=%f, delay=%f\n",
                                driverName, functionName, settingName, ps->epicsParam, width, delay);
                            break;
                        }
                    }
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
        
    } else if (function == NDFileNumber) {
        setFilePathAndName(false);
        
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
                (function == LFIntensifierEnable_) ||
                (function == LFIntensifierGain_) ||
                (function == LFGatingMode_) ||
                (function == LFSyncMasterEnable_)) {
        status = setExperimentInteger(function, value); 
        
     } else if (function == LFUpdateExperiments_) {
        status = getExperimentList();

     } else if (function == LFGrating_) {
        List<String^>^ list = gratingList_;
        if (value < list->Count) {
            String^ grating = list[value];
            status = setExperimentString(SpectrometerSettings::GratingSelected, grating);
        }
        
    } else if (function == LFExperimentName_) {
        List<String^>^ list = experimentList_;
        if (value < list->Count) {
            String^ experimentName = list[value];
            status = openExperiment((CString)experimentName);
        }
        
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
        value = value*1e3;
    }
    if (function == LFSyncMaster2Delay_) {
        // LightField units are us 
        value = value*1e6;
    }
    if ((function == ADAcquireTime) ||
        (function == ADTemperature) ||
        (function == LFGratingWavelength_) ||
        (function == LFTriggerFrequency_) ||
        (function == LFSyncMaster2Delay_))
        status = setExperimentDouble(function, value);
    else if ((function == LFRepGateWidth_) ||
             (function == LFSeqStartGateWidth_) ||
             (function == LFSeqEndGateWidth_) ||
             (function == LFAuxWidth_)) {
        double delay;
        getDoubleParam(function+1, &delay);
        status = setExperimentPulse(function, value, delay);
    }
    else if ((function == LFRepGateDelay_) ||
             (function == LFSeqStartGateDelay_) ||
             (function == LFSeqEndGateDelay_) ||
             (function == LFAuxDelay_)) {
        double width;
        getDoubleParam(function-1, &width);
        status = setExperimentPulse(function-1, width, value);
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


/** Called when asyn clients call pasynOctet->write().
  * This function performs actions for some parameters, including NDAttributesFile.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Address of the string to write.
  * \param[in] nChars Number of characters to write.
  * \param[out] nActual Number of characters actually written. */
asynStatus LightField::writeOctet(asynUser *pasynUser, const char *value, 
                                    size_t nChars, size_t *nActual)
{
    int addr=0;
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeOctet";

    /* Set the parameter in the parameter library. */
    setStringParam(addr, function, (char *)value);

    if ((function == NDFilePath) ||
        (function == NDFileName) ||
        (function == NDFileTemplate)) {
        status = setFilePathAndName(false);
    }
    else if ((function == LFBackgroundPath_) ||
             (function == LFBackgroundFile_)){
        status = setBackgroundFile();
    }
    else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_LF_PARAM) status = ADDriver::writeOctet(pasynUser, value, nChars, nActual);
    }

     /* Do callbacks so higher layers see any changes */
    status = (asynStatus)callParamCallbacks(addr, addr);

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
