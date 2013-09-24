
#include "stdafx.h"

#include <string>
#include <stdio.h>
#include <stdlib.h>

using namespace System;
using namespace System::Collections::Generic;

#using <PrincetonInstruments.LightField.AutomationV4.dll>
#using <PrincetonInstruments.LightFieldViewV4.dll>
#using <PrincetonInstruments.LightFieldAddInSupportServices.dll>

using namespace PrincetonInstruments::LightField::Automation;
using namespace PrincetonInstruments::LightField::AddIns;

int acquisitionComplete;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
void ExperimentCompletedEventHandler(System::Object^ sender, 
                                     PrincetonInstruments::LightField::AddIns::ExperimentCompletedEventArgs^ args)
{
    printf("ExperimentCompletedEventHandler entry\n");
    acquisitionComplete = 1;
    printf("ExperimentCompletedEventHandler exit\n");
}


int main(int argc, char *argv[])
{
    printf("LFTest1, entry\n");
    gcroot<PrincetonInstruments::LightField::Automation::Automation ^> m_Automation;
    gcroot<ILightFieldApplication^> m_Application;
    gcroot<IExperiment^> m_Experiment;
    
     // options can include a list of files to open when launching LightField
    printf("LFTest1, creating list\n");
    List<String^>^ options = gcnew List<String^>();
    printf("LFTest1, creating automation\n");
    m_Automation = gcnew PrincetonInstruments::LightField::Automation::Automation(true, options);   

    // Get the application interface from the automation
    printf("LFTest1, getting application\n");
 	  m_Application = m_Automation->LightFieldApplication;

    // Get the experiment interface from the application
    m_Experiment  = m_Application->Experiment;

    // Tell the application to suppress prompts (overwrite file names, etc...)
    m_Application->UserInteractionManager->SuppressUserInteraction = true;

    // Lets see if a camera is connected
    bool bCameraFound = false;
    CString cameraName;
    ////////////////////////////////////////////////////////////////////////////////
    // Look for a camera already added to the experiment
    List<PrincetonInstruments::LightField::AddIns::IDevice^> experimentList = m_Experiment->ExperimentDevices;        
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
    ////////////////////////////////////////////////////////////////////////////////
    // Next look for an available camera that can be added to the experiment
    if (!bCameraFound)
    {
        List<PrincetonInstruments::LightField::AddIns::IDevice^> availableList  = m_Experiment->AvailableDevices;
        for each(IDevice^% device in availableList)
        {
            if (device->Type == PrincetonInstruments::LightField::AddIns::DeviceType::Camera)
            {
                // Cache the name
                 cameraName = device->Model;

                 // Add the device to the experiment
                 printf("LFTest1, adding camera %s\n", cameraName);
                 m_Experiment->Add(device);
                             
                 // Break loop on finding camera
                 bCameraFound = true;                 
                 break;
            }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////
    // If we found a camera lets get its information
    if (bCameraFound)
    {
        // Get The Full Size
        RegionOfInterest full = m_Experiment->FullSensorRegion;
        
        // Print out the ROI information
        printf("X=%d, Width=%d, XBinning=%d, Y=%d, Height=%d, YBinning=%d\n",
          full.X, full.Width, full.XBinning, full.Y, full.Height, full.YBinning);

        // Do we have an exposure time?
        if (m_Experiment->Exists(CameraSettings::ShutterTimingExposureTime))
        {            
            // Get the exposure
            double exposure = safe_cast<double>(m_Experiment->GetValue(CameraSettings::ShutterTimingExposureTime));
            printf("Exposure time=%f\n", exposure);
        }
        printf("LFTest1, done reading existing parameters\n");
    }

    printf("LFTest1, preparing to set new parameters\n");

    // Connect the acquisition event handler       
    m_Experiment->ExperimentCompleted += gcnew System::EventHandler<PrincetonInstruments::LightField::AddIns::ExperimentCompletedEventArgs^>(&ExperimentCompletedEventHandler);

    /////////////////////////////////////////////////////////////////////////////
    //  Push all the settings to the camera and acquire
    //
    /////////////////////////////////////////////////////////////////////////////
    int x  = 100;
    int w  = 500;
    int xb = 1;

    int y  = 10;
    int h  = 50;
    int yb = 1;

    double exposureTime = 1100.;  // The units are ms
    CString fileName = "test1";
    
    ///////////////////////// PROGRAM TO CAMERA /////////////////////////
    ///////////// REGION OF INTEREST SETUP //////////////////////////////
    // Create an Region of Interest Structure
    printf("LFTest1, creating ROI\n");
    RegionOfInterest^ roi = gcnew RegionOfInterest(x, y, w, h, xb, yb);

    // Create an array that can contain many regions (simple example 1)
    array<RegionOfInterest>^ rois = gcnew array<RegionOfInterest>(1);

    // Fill in the array element(s)
    rois[0] = *roi;

    // Set the custom regions
    printf("LFTest1, calling SetCustomRegions\n");
    m_Experiment->SetCustomRegions(rois);  

    ///////////////EXPOSURE TIME SETUP /////////////////////////////
    if (m_Experiment->Exists(CameraSettings::ShutterTimingExposureTime))
    {
        printf("LFTest1, setting exposure time\n");
        if (m_Experiment->IsValid(CameraSettings::ShutterTimingExposureTime, exposureTime))        
            m_Experiment->SetValue(CameraSettings::ShutterTimingExposureTime, exposureTime);              
    }               

    ///////////////Spectrometer optical exit port setup /////////////////////////////
    printf("LFTest1, testing if SpectrometerSettings::OpticalPortExitSelected exists\n");       
    if (m_Experiment->Exists(SpectrometerSettings::OpticalPortExitSelected))
    {
        int ival;
        printf("LFTest1, testing if SpectrometerSettings::OpticalPortExitSelected=OpticalPortLocation::FrontExit is readonly\n"); 
        bool readOnly = m_Experiment->IsReadOnly(SpectrometerSettings::OpticalPortExitSelected);
        printf("LFTest1, SpectrometerSettings::OpticalPortExitSelected, readonly=%d\n", readOnly);         
        printf("LFTest1, testing if SpectrometerSettings::OpticalPortExitSelected=OpticalPortLocation::FrontExit is valid\n");       
        if (m_Experiment->IsValid(SpectrometerSettings::OpticalPortExitSelected, OpticalPortLocation::FrontExit)) {
            printf("LFTest1, setting SpectrometerSettings::OpticalPortExitSelected=4 (%d)\n", OpticalPortLocation::FrontExit);       
            m_Experiment->SetValue(SpectrometerSettings::OpticalPortExitSelected, 4); 
            ival = safe_cast<int>(m_Experiment->GetValue(SpectrometerSettings::OpticalPortExitSelected));
            printf("Set exit port=4, read exit port=%d\n", ival);
            m_Experiment->SetValue(SpectrometerSettings::OpticalPortExitSelected, 5); 
            ival = safe_cast<int>(m_Experiment->GetValue(SpectrometerSettings::OpticalPortExitSelected));
            printf("Set exit port=5, read exit port=%d\n", ival);
            Object^ obj = m_Experiment->GetValue(SpectrometerSettings::OpticalPortExitSelected);
            ival = safe_cast<int>(obj);
            printf("Set exit port=5, read exit port=%d\n", ival);
             
        }             
    }               

    printf("LFTest1, testing if CameraSettings::IntensifierGatingMode exists\n");       
    if (m_Experiment->Exists(CameraSettings::IntensifierGatingMode))
    {
        int ival;
        printf("LFTest1, testing if CameraSettings::IntensifierGatingMode is readonly\n"); 
        bool readOnly = m_Experiment->IsReadOnly(CameraSettings::IntensifierGatingMode);
        printf("LFTest1, CameraSettings::IntensifierGatingMode, readonly=%d\n", readOnly);         
        printf("LFTest1, testing if CameraSettings::IntensifierGatingMode=GatingMode::Repetitive is valid\n");       
        if (m_Experiment->IsValid(CameraSettings::IntensifierGatingMode, GatingMode::Repetitive)) {
            printf("LFTest1, setting CameraSettings::IntensifierGatingMode=GatingMode::Repetitive (%d)\n", GatingMode::Repetitive);       
            m_Experiment->SetValue(CameraSettings::IntensifierGatingMode, GatingMode::Repetitive); 
            ival = safe_cast<int>(m_Experiment->GetValue(CameraSettings::IntensifierGatingMode));
            printf("Set CameraSettings::IntensifierGatingMode=%d, read CameraSettings::IntensifierGatingMode=%d\n", 
                GatingMode::Repetitive, ival);             
        }             
    }               

    ///////////////Spectrometer setup /////////////////////////////
    printf("LFTest1, testing if SpectrometerSettings::OpticalPortEntranceSideWidth exists\n");       
    if (m_Experiment->Exists(SpectrometerSettings::OpticalPortEntranceSideWidth))
    {
        printf("LFTest1, testing if SpectrometerSettings::OpticalPortEntranceSideWidth=10 is valid\n");       
        if (m_Experiment->IsValid(SpectrometerSettings::OpticalPortEntranceSideWidth, 10)) {
            printf("LFTest1, setting SpectrometerSettings::OpticalPortEntranceSideWidth=10\n");       
            m_Experiment->SetValue(SpectrometerSettings::OpticalPortEntranceSideWidth, 10); 
        }             
    }               
    ////////////////////////////////////////////////////////////////
    // Don't Automatically Attach Date/Time to the file name
    m_Experiment->SetValue(ExperimentSettings::FileNameGenerationAttachDate, false);
    m_Experiment->SetValue(ExperimentSettings::FileNameGenerationAttachTime, false);
                  
    // Set the Base file name value
    printf("LFTest1, setting base file name\n");
    m_Experiment->SetValue(ExperimentSettings::FileNameGenerationBaseFileName, gcnew String (fileName));    
        
    // Begin the acquisition 
    if (m_Experiment->IsReadyToRun) {
        printf("LFTest1, starting acquisition\n");
        m_Experiment->Acquire();
    }
    else
        printf("Settings are in error!\n");
    while(!acquisitionComplete) {
        printf("Waiting for acquisition to complete\n");
        Sleep(100);
    }
    Sleep(200);
    printf("Acquisition complete, program exiting\n");
}
