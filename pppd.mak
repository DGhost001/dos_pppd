.AUTODEPEND

#		*Translator Definitions*
CC = bcc +PPPD.CFG
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
 pktdrvr.obj \
 optsdos.obj \
 dosmain.obj

#		*Explicit Rules*
pppd.exe: pppd.cfg $(EXE_dependencies)
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
pktdrvr.obj+
optsdos.obj+
dosmain.obj
pppd,pppd
cs.lib
|


#		*Individual File Dependencies*
auth.obj: pppd.cfg auth.c 

magic.obj: pppd.cfg magic.c 

ipcp.obj: pppd.cfg ipcp.c 

upap.obj: pppd.cfg upap.c 

lcp.obj: pppd.cfg lcp.c 

fsm.obj: pppd.cfg fsm.c 

ppp.obj: pppd.cfg ppp.c 

n8250.obj: pppd.cfg n8250.c 

critical.obj: pppd.cfg critical.asm 
	$(TASM) /MX /ZI CRITICAL.ASM,CRITICAL.OBJ

lowlevel.obj: pppd.cfg lowlevel.asm 
	$(TASM) /MX /ZI LOWLEVEL.ASM,LOWLEVEL.OBJ

byteorde.obj: pppd.cfg byteorde.asm 
	$(TASM) /MX /ZI BYTEORDE.ASM,BYTEORDE.OBJ

pktdrvr.obj: pppd.cfg pktdrvr.c 

optsdos.obj: pppd.cfg optsdos.c 

dosmain.obj: pppd.cfg dosmain.c 

#		*Compiler Configuration File*
pppd.cfg: pppd.mak
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
| pppd.cfg


