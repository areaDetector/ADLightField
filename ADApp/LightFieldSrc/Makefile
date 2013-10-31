TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CXXFLAGS += /EHa /clr /D_AFXDLL /AI.. /D_WIN32_WINNT=0x601

#======== DETECTOR LIBRARY ==============

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  ifeq ($(STATIC_BUILD), NO)
    LIBRARY_IOC = LightField
    LIB_SRCS += LightField.cpp
  endif
endif

DBD += LightFieldSupport.dbd

#====== DETECTOR APPLICATION ============

ifeq ($(BUILD_APPS), YES)
PROD_NAME = LightFieldApp
ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  ifeq ($(STATIC_BUILD), NO)
    PROD_IOC  += $(PROD_NAME)
    PROD_IOC_WIN32 += LFTest1
    BIN_INSTALLS += ../PrincetonInstruments.LightField.AutomationV4.dll
    BIN_INSTALLS += ../PrincetonInstruments.LightFieldAddInSupportServices.dll
    BIN_INSTALLS += ../PrincetonInstruments.LightFieldViewV4.dll
  endif
endif

# <name>.dbd will be created from <name>Include.dbd
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += LightFieldSupport.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
LightFieldApp_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add locally compiled object code
LightFieldApp_LIBS          += LightField

include $(TOP)/ADApp/commonDriverMakefile
endif

LFTest1_SRCS += LFTest1.cpp

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

