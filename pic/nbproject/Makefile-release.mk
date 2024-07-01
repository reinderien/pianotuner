#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
ifeq "${IGNORE_LOCAL}" "TRUE"
# do not include local makefile. User is passing all local related variables already
else
include Makefile
# Include makefile containing local settings
ifeq "$(wildcard nbproject/Makefile-local-release.mk)" "nbproject/Makefile-local-release.mk"
include nbproject/Makefile-local-release.mk
endif
endif

# Environment
MKDIR=gnumkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=release
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
endif

ifeq ($(COMPARE_BUILD), true)
COMPARISON_BUILD=
else
COMPARISON_BUILD=
endif

ifdef SUB_IMAGE_ADDRESS

else
SUB_IMAGE_ADDRESS_COMMAND=
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=main.asm

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/main.o
POSSIBLE_DEPFILES=${OBJECTDIR}/main.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/main.o

# Source Files
SOURCEFILES=main.asm



CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps

.build-conf:  ${BUILD_SUBPROJECTS}
ifneq ($(INFORMATION_MESSAGE), )
	@echo $(INFORMATION_MESSAGE)
endif
	${MAKE}  -f nbproject/Makefile-release.mk dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=PIC16F1773
# ------------------------------------------------------------------------------------
# Rules for buildStep: pic-as-assembler
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/main.o: main.asm  nbproject/Makefile-${CND_CONF}.mk 
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/main.o 
	${MP_AS} -mcpu=PIC16F1773 -c \
	-o ${OBJECTDIR}/main.o \
	main.asm \
	 -mdfp=".mchp_packs\Microchip\PIC12-16F1xxx_DFP\1.7.242\xc8" -msummary=-mem,+psect,+class,+hex,+file,-sha1,-sha256,-xml,-xmlfull -v -fmax-errors=20 -mwarn=9 -Dbuild_for_debug=0 -xassembler-with-cpp
	
else
${OBJECTDIR}/main.o: main.asm  nbproject/Makefile-${CND_CONF}.mk 
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/main.o 
	${MP_AS} -mcpu=PIC16F1773 -c \
	-o ${OBJECTDIR}/main.o \
	main.asm \
	 -mdfp=".mchp_packs\Microchip\PIC12-16F1xxx_DFP\1.7.242\xc8" -msummary=-mem,+psect,+class,+hex,+file,-sha1,-sha256,-xml,-xmlfull -v -fmax-errors=20 -mwarn=9 -Dbuild_for_debug=0 -xassembler-with-cpp
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: pic-as-linker
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} -mcpu=PIC16F1773 ${OBJECTFILES_QUOTED_IF_SPACED} \
	-o dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX} \
	 -mdfp=".mchp_packs\Microchip\PIC12-16F1xxx_DFP\1.7.242\xc8" -msummary=-mem,+psect,+class,+hex,+file,-sha1,-sha256,-xml,-xmlfull -Wl,-aNEARCODE=000h-3FFh,-ppsect_por_vec=0,-ppsect_isr_vec=4 -mcallgraph=std -mno-download-hex
else
dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} -mcpu=PIC16F1773 ${OBJECTFILES_QUOTED_IF_SPACED} \
	-o dist/${CND_CONF}/${IMAGE_TYPE}/pic.${IMAGE_TYPE}.${OUTPUT_SUFFIX} \
	 -mdfp=".mchp_packs\Microchip\PIC12-16F1xxx_DFP\1.7.242\xc8" -msummary=-mem,+psect,+class,+hex,+file,-sha1,-sha256,-xml,-xmlfull -Wl,-aNEARCODE=000h-3FFh,-ppsect_por_vec=0,-ppsect_isr_vec=4 -mcallgraph=std -mno-download-hex
endif


# Subprojects
.build-subprojects:


# Subprojects
.clean-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r build/release
	${RM} -r dist/release

# Enable dependency checking
.dep.inc: .depcheck-impl

DEPFILES=$(shell mplabwildcard ${POSSIBLE_DEPFILES})
ifneq (${DEPFILES},)
include ${DEPFILES}
endif
