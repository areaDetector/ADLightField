ADLightField
===========
An <a href="http://www.aps.anl.gov/epics/">EPICS</a> 
<a href="http://cars.uchicago.edu/software/epics/areaDetector.html">areaDetector</a> 
driver for recent 
<a href="http://www.princetoninstruments.com/">Princeton Instruments</a> 
detectors including the ProEM, PIXIS, PI-MAX3, PI-MAX4, PyLoN, and Quad-RO. 
It also supports the Acton Series spectrographs.
The interface to the detector is via the Microsoft Common Language Runtime (CLR)
interface to the <b>LightField</b> program that Princeton Instruments sells. The
areaDetector driver effectively "drives" LightField through the CLR interface, performing
most of the same operations that can be performed using the LightField GUI.
