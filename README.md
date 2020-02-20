ADLightField
===========
An 
[EPICS](http://www.aps.anl.gov/epics/) 
[areaDetector](https://github.com/areaDetector/areaDetector/blob/master/README.md) 
driver for recent 
[Princeton Instruments](http://www.princetoninstruments.com/)
detectors including the ProEM, PIXIS, PI-MAX3, PI-MAX4, PyLoN, and Quad-RO. 
It also supports the Acton Series spectrographs.
The interface to the detector is via the Microsoft Common Language Runtime (CLR)
interface to the LightField program that Princeton Instruments sells. The
areaDetector driver effectively "drives" LightField through the CLR interface, performing
most of the same operations that can be performed using the LightField GUI.

Additional information:
* [Documentation](https://areadetector.github.io/master/ADLightField/ADLightField.html)
* [Release notes and links to source and binary releases](RELEASE.md)
