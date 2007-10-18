# -x assembler-with-cpp
# gpSP makefile
# Gilead Kutnick - Exophase

# Global definitions

include ../Makefile.set

PSPSDK          = ${shell psp-config --pspsdk-path}
PREFIX          = ${shell psp-config --psp-prefix}

COMMON_FILE       = common.h cpu.h memory.h video.h input.h sound.h main.h gui.h zip.h cheats.h
UI_COMMON_FILE  = fbm_print.h draw.h message.h

PSP_EBOOT_ICON  = ../image/icon1.png
#文字列が表示されないので保留
#PSP_EBOOT_PIC1  = ../image/pic1.png 

OBJS            = main.o cpu.o video.o memory.o sound.o input.o
OBJS           += cpu_threaded.o gui.o zip.o cheats.o mips_stub.o fbm_print.o bios.o draw.o
OBJS           += pspDveManager.o 

ifeq ($(ADHOC_MODE), 1)
OBJS           += adhoc.o
COMMON_FILE += adhoc.h
endif

TARGET          = UO_gpSP_kai

PSP_FW_VERSION=300
VERSION_MODE = user

ifeq ($(M64_MODE), 1)
VERSION_MODE += 64M
endif

ifeq ($(TEST_MODE), 1)
VERSION_RELEASE = test
endif

VERSION_OPT = ${VERSION_RELEASE} ${VERSION_BUILD}

PSP_EBOOT_TITLE = UO gpSP kai ${VERSION_MAJOR}.${VERSION_MINOR} ${VERSION_MODE} ${VERSION_OPT}

VPATH           += ..
CFLAGS          += -O3 -DPSP_BUILD -G0 -funsigned-char
CFLAGS          += -Wall
#CFLAGS          += -W -Wno-sign-compare
#CFLAGS          += -Wshadow -Wbad-function-cast -Wconversion -Wstrict-prototypes -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wnested-externs -Wredundant-decls 
CFLAGS          +=  -funsigned-char -ffast-math -fforce-addr -fmerge-all-constants -floop-optimize2 -funsafe-loop-optimizations -ftree-loop-linear

CFLAGS          += -DVERSION_MAJOR=${VERSION_MAJOR}
CFLAGS          += -DVERSION_MINOR=${VERSION_MINOR}
CFLAGS          += -DVERSION_BUILD=${VERSION_BUILD}

CFLAGS          += -DUSER_MODE

ifeq ($(M64_MODE), 1)
CFLAGS          += -DM64_MODE
endif

ifeq ($(TEST_MODE), 1)
CFLAGS          += -DTEST_MODE
endif

ifeq ($(ADHOC_MODE), 1)
CFLAGS          += -DADHOC_MODE
endif

ifeq ($(C_CORE_MODE), 1)
CFLAGS          += -DC_CORE_MODE
endif

ASFLAGS         = ${CFLAGS}
EXTRA_TARGETS   = EBOOT.PBP

LIBS            += -lpspgu -lpsprtc -lpspaudio -lz -lpsppower -lpspkubridge

ifdef ADHOC_MODE
LIBS            += -lpspnet -lpspwlan -lpspnet_adhoc -lpspnet_adhocctl -lpspnet_adhocmatching
endif

include ${PSPSDK}/lib/build.mak

main.o         : ${COMMON_FILE} ${UI_COMMON_FILE} main.c
cpu.o          : ${COMMON_FILE} cpu.c
video.o        : ${COMMON_FILE} video.c
memory.o       : ${COMMON_FILE} memory.c
sound.o        : ${COMMON_FILE} sound.c
input.o        : ${COMMON_FILE} input.c
cpu_threaded.o : ${COMMON_FILE} cpu_threaded.c mips_emit.h bios.h
gui.o          : ${COMMON_FILE} ${UI_COMMON_FILE} gui.c
zip.o          : ${COMMON_FILE} zip.c
cheats.o       : ${COMMON_FILE} cheats.c
mips_stub.o    : mips_stub.S
fbm_print.o    : ${COMMON_FILE} ${UI_COMMON_FILE} fbm_print.c
bios.o         : ${COMMON_FILE} bios.c bios.h
draw.o         : ${COMMON_FILE} ${UI_COMMON_FILE} draw.c draw.h

ifdef ADHOC_MODE
#adhoc.o         : ${COMMON_FILE} adhoc.c adhoc.h
endif