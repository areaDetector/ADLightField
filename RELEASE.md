ADLightField Releases
=====================

The latest untagged master branch can be obtained at
https://github.com/areaDetector/ADLightField.

Tagged source code releases from R2-0 onward can be obtained at 
https://github.com/areaDetector/ADLightField/releases.

Tagged prebuilt binaries from R2-0 onward can be obtained at
http://cars.uchicago.edu/software/pub/ADLightField.

The versions of EPICS base, asyn, and other synApps modules used for each release can be obtained from 
the EXAMPLE_RELEASE_PATHS.local, EXAMPLE_RELEASE_LIBS.local, and EXAMPLE_RELEASE_PRODS.local
files respectively, in the configure/ directory of the appropriate release of the 
[top-level areaDetector](https://github.com/areaDetector/areaDetector) repository.


Release Notes
=============

R2-5 (30-January-2018)
---
* Added support for creating the directories for data and background files.
* LightField.template
  - Change LFGratingWavelength records to LFGratingWL because names were too long for links.
  - Disable PINI for LFGratingWL because it will crash LightField if Step and Glue is enabled.
* Added support for Step And Glue to collect spectra that require changing gratings automatically.
  There are records to enable Step And Glue and to set the starting and ending wavelength.
* Added support for selecting the Entrance Port of the spectrometer.
* Fixed medm adl files to improve the autoconversion to other display manager files.
* Added op/Makefile to automatically convert adl files to edl, ui, and opi files.
* Updated the edl, ui, and opi autoconvert directories to contain the conversions
  from the most recent adl files.
* Updated the DLL files to those from LightField 6.4.
* Changed CONFIG_SITE to disable GraphicsMagick with LightField because it is incompatible for some reason.


R2-4 (04-July-2017)
----
* Changed layout of medm screen for ADCore R3-0.
* Changed LightField.template to prevent crashes
  - LFExperimentName to PINI=NO so it does load the experiment at startup. This seems to sometimes cause problems.
  - Set LFRepGateWidth.DRVL = 1.e-10 so it cannot be set to 0.


R2-3 (20-February-2017)
----
* Changed layout of medm screen for ADCore R2-6.
* Changed default IOC arch from windows-x64-dynamic to windows-x64 to be consistent with the 
  standard ARCH names in EPICS base.


R2-2 (16-April-2015)
----
* Add capability to change ADAcquireTime, LFRepGateWidth, or LFRepGateDelay 
  while acquisition is active.  This works in both normal acquire and preview
  focus mode.  This matches the capability of LightField.
* Changes for compatibility with ADCore R2-2.


R2-1 (19-Aug-2014)
----
* Finished documentation, LightFieldDoc.html.


R2-0 (4-Apr-2014)
----
* New driver added in this release.


Future Releases
===============
* Support mumber of acquisitions (outer-most loop)
* Restore previous state of background enable in image completion callback?
