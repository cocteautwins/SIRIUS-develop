#
# C++ compiler
#
CXX = g++-mp-4.7

#
#  Initial compiler options typically include optimization / warning switches and a preprocessor directive
#  which controls the debug
#
#CXX_OPT = -O0 -g -Wall -fopenmp -Wno-cpp -Wno-reorder
#CXX_OPT = -O0 -g -Wall -Wno-cpp -Wno-reorder
#CXX_OPT = -O2 -g -Wall -fopenmp -DNDEBUG
CXX_OPT = -O2 -g -Wall

#
#  Initial list of libraries
#
LIBS = -lstdc++

#
#  Provide the full list of libraries for the linker.
#
LINK_OPT = $(LIBS)

#  Lapack / BLAS
LIBS := $(LIBS) -llapack -lblas

#  libxc
CXX_OPT := $(CXX_OPT) -I/Users/anton/src/libxc-1.2.0/src
LIBS := $(LIBS) /Users/anton/src/libxc-1.2.0/src/.libs/libxc.a 

#  spglib
CXX_OPT := $(CXX_OPT) -I/Users/anton/src/sirius-project/spglib-1.2.2/src
LIBS := $(LIBS) /Users/anton/src/sirius-project/spglib-1.2.2/src/.libs/libsymspg.a

#  GNU scientific library
CXX_OPT := $(CXX_OPT) -I/Users/anton/src/sirius-project/gsl-1.15
LIBS := $(LIBS) /Users/anton/src/sirius-project/gsl-1.15/.libs/libgsl.a

#  FFTW 
LIBS := $(LIBS) /opt/local/lib/libfftw3.a