TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CXXFLAGS += /EHa /clr /D_AFXDLL /AI.. /D_WIN32_WINNT=0x601

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  ifeq ($(STATIC_BUILD), NO)
    LIBRARY_IOC = LightField
    LIB_SRCS += LightField.cpp
    PROD_IOC_WIN32 += LFTest1
  endif
endif

DBD += LightFieldSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

LFTest1_SRCS += LFTest1.cpp

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

