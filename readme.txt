The code was ported using the Borland C 3.1 version compiler and assembler, I
have not tested if other tools would compile it. I have included the .OBJ
files for the .ASM modules, in case you don't have Borland's Turbo Assembler
3.1.

The code also implements some hacks that are very BC 3.1 specific, specially
for resident memory size reduction. One nice trick is the 'spawn()'
replacement function, which calls an internal Borland's library routine for
avoiding the inclusion of lots of unwanted code for path searching, etc.

I have provided both .PRJ files (for IDE compilation) and makefiles. If you
are going to compile from the ide, remember to change the Directories option
to reflect your BC installation paths. The same applies for the makefiles,
edit them and change the paths for LIB and INCLUDE.

There is one set of source files, and every executable is built via a
different .PRJ or makefile that defines some preprocessor symbols. These
symbols are then used in the .C files for conditional compilation of the
interesting parts for each driver executable.

If you do require accessing the serial port when Dospppd is in memory, you
must use an special interface found in the driver; look at CHAT.C, PKTDRVR.C
and N8250.C for the details. The reason for this is that Dospppd owns the
serial port IRQ vector, and it uses interrupts for CD changing detection, so
stealing the vector and restoring it later won't work. Feel free to expand or
enhance this interface if you need more functions.

The compilation process will generate many warnings (about 100), but I have
checked them all and they are harmless. One could avoid them by using type
casts, etc., but I had no time for it.

I can't assist you in porting the code to another set of tools. I own newer
versions of Borland's compiler, but I have found that BC 3.1 generates the
smallest code, so I did the port for it.

And remember, this is free software, not public domain software. The original
authors have rights on the code, and I have rights on the DOS port and some
files that were written from the scratch. If you are going to use the code in
commercial applications, please be kind and follow the conditions explained
at the top of every source file.

I'm interested in improvements and other ports of Dospppd, so feel free to
send me your modified sources. I'll try to keep a coherent code base for
future Dospppd development.

I was very busy lately due my real job, and it seems that this situation will
be worse in the near future :(, so don't expect much assistance from me while
you are working with the code.
