.AUTODEPEND

#		*Translator Definitions*
CC = bcc +EPPPDD.CFG
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
epppdd.exe: epppdd.cfg $(EXE_dependencies)
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
epppdd,epppdd
cs.lib
|


#		*Individual File Dependencies*
auth.obj: epppdd.cfg auth.c 

magic.obj: epppdd.cfg magic.c 

ipcp.obj: epppdd.cfg ipcp.c 

upap.obj: epppdd.cfg upap.c 

lcp.obj: epppdd.cfg lcp.c 

fsm.obj: epppdd.cfg fsm.c 

ppp.obj: epppdd.cfg ppp.c 

n8250.obj: epppdd.cfg n8250.c 

critical.obj: epppdd.cfg critical.asm 
	$(TASM) /MX /ZI CRITICAL.ASM,CRITICAL.OBJ

lowlevel.obj: epppdd.cfg lowlevel.asm 
	$(TASM) /MX /ZI LOWLEVEL.ASM,LOWLEVEL.OBJ

byteorde.obj: epppdd.cfg byteorde.asm 
	$(TASM) /MX /ZI BYTEORDE.ASM,BYTEORDE.OBJ

pktdrvre.obj: epppdd.cfg pktdrvre.c 

optsdos.obj: epppdd.cfg optsdos.c 

dosmain.obj: epppdd.cfg dosmain.c 

#		*Compiler Configuration File*
epppdd.cfg: epppdd.mak
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
| epppdd.cfg


