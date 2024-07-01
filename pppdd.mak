.AUTODEPEND

#		*Translator Definitions*
CC = bcc +PPPDD.CFG
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
pppdd.exe: pppdd.cfg $(EXE_dependencies)
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
pppdd,pppdd
cs.lib
|


#		*Individual File Dependencies*
auth.obj: pppdd.cfg auth.c 

magic.obj: pppdd.cfg magic.c 

ipcp.obj: pppdd.cfg ipcp.c 

upap.obj: pppdd.cfg upap.c 

lcp.obj: pppdd.cfg lcp.c 

fsm.obj: pppdd.cfg fsm.c 

ppp.obj: pppdd.cfg ppp.c 

n8250.obj: pppdd.cfg n8250.c 

critical.obj: pppdd.cfg critical.asm 
	$(TASM) /MX /ZI CRITICAL.ASM,CRITICAL.OBJ

lowlevel.obj: pppdd.cfg lowlevel.asm 
	$(TASM) /MX /ZI LOWLEVEL.ASM,LOWLEVEL.OBJ

byteorde.obj: pppdd.cfg byteorde.asm 
	$(TASM) /MX /ZI BYTEORDE.ASM,BYTEORDE.OBJ

pktdrvr.obj: pppdd.cfg pktdrvr.c 

optsdos.obj: pppdd.cfg optsdos.c 

dosmain.obj: pppdd.cfg dosmain.c 

#		*Compiler Configuration File*
pppdd.cfg: pppdd.mak
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
-DDEBUGALL=1
| pppdd.cfg


