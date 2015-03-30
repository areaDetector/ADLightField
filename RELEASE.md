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

R2-2 (30-March-2015)
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
