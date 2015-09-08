TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = LightFieldApp
ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  ifeq ($(STATIC_BUILD), NO)
    PROD_IOC += $(PROD_NAME)
    # Note: Normally it is sufficient to have DLLs in the PATH environment.
    # However, LightField won't start unless the DLLs are in the same directory as this application
    # The DLLs need to be in the ADLightField/lightFieldApp/src directory or the library won't build
    # so we install them into the bin directory from there.
    BIN_INSTALLS += $(ADLIGHTFIELD)/lightFieldApp/src/PrincetonInstruments.LightField.AutomationV4.dll
    BIN_INSTALLS += $(ADLIGHTFIELD)/lightFieldApp/src/PrincetonInstruments.LightFieldAddInSupportServices.dll
    BIN_INSTALLS += $(ADLIGHTFIELD)/lightFieldApp/src/PrincetonInstruments.LightFieldViewV4.dll
  endif
endif

# <name>.dbd will be created from <name>Include.dbd
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += LightFieldSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
PROD_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add locally compiled object code
PROD_LIBS += LightField

include $(ADEXAMPLE)/exampleApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

