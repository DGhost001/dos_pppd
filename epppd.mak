.AUTODEPEND

#		*Translator Definitions*
CC = bcc +EPPPD.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = F:\BORLANDC\LIB
INCLUDEPATH = F:\BORLANDC\INCLUDE


#		*Implicit Rules*
.c.obj:
  $(CC) -c {$< }

.cpp.obj:
  $(CC) -c {$< }

#		*List Macros*


EXE_dependencies =  \
 auth.obj \
 magic.obj \
 ipcp.obj \
 upap.obj \
 lcp.obj \
 fsm.obj \
 ppp.obj \
 n8250.obj \
 critical.obj \
 lowlevel.obj \
 byteorde.obj \
 pktdrvre.obj \
 optsdos.obj \
 dosmain.obj

#		*Explicit Rules*
epppd.exe: epppd.cfg $(EXE_dependencies)
  $(TLINK) /s/c/d/P-/L$(LIBPATH) @&&|
c0s.obj+
auth.obj+
magic.obj+
ipcp.obj+
upap.obj+
lcp.obj+
fsm.obj+
ppp.obj+
n8250.obj+
critical.obj+
lowlevel.obj+
byteorde.obj+
pktdrvre.obj+
optsdos.obj+
dosmain.obj
epppd,epppd
cs.lib
|


#		*Individual File Dependencies*
auth.obj: epppd.cfg auth.c 

magic.obj: epppd.cfg magic.c 

ipcp.obj: epppd.cfg ipcp.c 

upap.obj: epppd.cfg upap.c 

lcp.obj: epppd.cfg lcp.c 

fsm.obj: epppd.cfg fsm.c 

ppp.obj: epppd.cfg ppp.c 

n8250.obj: epppd.cfg n8250.c 

critical.obj: epppd.cfg critical.asm 
	$(TASM) /MX /ZI CRITICAL.ASM,CRITICAL.OBJ

lowlevel.obj: epppd.cfg lowlevel.asm 
	$(TASM) /MX /ZI LOWLEVEL.ASM,LOWLEVEL.OBJ

byteorde.obj: epppd.cfg byteorde.asm 
	$(TASM) /MX /ZI BYTEORDE.ASM,BYTEORDE.OBJ

pktdrvre.obj: epppd.cfg pktdrvre.c 

optsdos.obj: epppd.cfg optsdos.c 

dosmain.obj: epppd.cfg dosmain.c 

#		*Compiler Configuration File*
epppd.cfg: epppd.mak
  copy &&|
-f-
-ff-
-C
-w+
-j0
-g0
-O
-Oe
-Ob
-Z
-k-
-d
-vi-
-I$(INCLUDEPATH)
-L$(LIBPATH)
-DIOMACROS=1
-DLOWLEVELASY=1
| epppd.cfg


