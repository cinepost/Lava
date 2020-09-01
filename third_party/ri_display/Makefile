#!/bin/make -f

#INCEXTRA = -I"$(RMANTREE)"/include 
INCEXTRA = -I/home/max/dev/Falcor/third_party/prman 


all: install

install: d_houdini.so

d_houdini.so: d_houdini.C d_houdini.h dspyhlpr.c
	g++ -c $(INCEXTRA) -I. d_houdini.C -o d_houdini.o
	g++ -shared d_houdini.o -o d_houdini.so 

clean:
	rm -f *.o d_houdini.so 
