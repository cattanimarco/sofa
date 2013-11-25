ABOUT
====

SOFA is an opportunistic duty cycled protocol for Extreme Wireless Sensor Networks
More info at http://cattanimarco.com/2013/11/20/sofa/

RUN DEMO
====
1) Go to SOFA's example application and compile
	cd contiki/examples/sofa/
	make clean TARGET=sky; make TARGET=sky

This will create an example application (example-sofa.sky) for TmoteSky nodes. The application computes the average among all the connected nodes. The starting value of each node is its ID multiplied by 10. Every time a node exchange a value with a neighbor, it updates it local value and print it on the serial (string).

2) Run the application in Cooja or on real node and enjoy SOFA. 

INSTALL PATCH FILE
====

Instructions to install SOFA on InstantContiki 2.6.1

1) Download and run InstantContiki. Note that this patch is created from Contiki 2.6.1 and was not tested on other versions.

2) Download SOFA's patch file (sofa.patch)
	
3) Go to the contiki folder and apply the patch
	cd ~/contiki
	patch -p1 < ~/sofa.patch
The resulting output should look like this:
	patching file core/net/mac/Makefile.mac
	patching file core/net/mac/sofamac.c
	patching file core/net/mac/sofamac.h
	patching file examples/sofa/example-sofa.c
	patching file examples/sofa/Makefile
	patching file examples/sofa/project-conf.h

4) Follow the "RUN DEMO" instructions


