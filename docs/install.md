
The prerequisites needed to build:

- autotools
- cmake >= 3.18
- MPI Library
- C++17 compiler
- CUDA >= 10.2 (only if building with GPU support)

`MACOSX`: We recommend using brew to install the prerequisites:  
- `brew install gcc openmpi cmake wget autoconf automake`

Supported Compilers
--------------------
- GCC versions >= 8.3
- LLVM Clang >= 8 (Tested on Linux Only)
- `MACOSX`: We only support brew installed `GCC`, `AppleClang` is not supported.


Supported Configurations
-------------------------
- The following configurations are recommended since they are tested and are known to work:
  - GCC versions >= 8.x + OpenMPI-2.x/MPICH-3.x built using corresponding gcc versions.
  - LLVM Clang versions >= 7.x + OpenMPI-2.x/MPICH-3.x 


Build Instructions
=====================

```
export GFCC_SRC=$HOME/gfcc_src
export GFCC_INSTALL_PATH=$HOME/gfcc_install
git clone https://github.com/spec-org/gfcc.git $GFCC_SRC
```

Step 1: Choose Build Options
============================

### CUDA Options 
```
-DUSE_CUDA=ON (OFF by default)
-DNV_GPU_ARCH=70 (GPU arch is detected automatically, only set this option if need to override)
```

Step 2: Building TAMM
=====================

```
cd $GFCC_SRC/contrib/TAMM
mkdir build && cd build
```

## In addition to the build options chosen, there are various build configurations depending on the BLAS library one wants to use.

* **[Default build using BLIS and NETLIB Lapack](install.md#default-build-using-blis-and-netlib-lapack)**

* **[Build using Intel MKL](install.md#build-using-intel-mkl)**

* **[Build instructions for Summit using ESSL](install.md#build-instructions-for-summit-using-essl)**

* **[Build instructions for Cori](install.md#build-instructions-for-cori)**

* **[Build instructions for Theta](install.md#build-instructions-for-theta)**

## Default build using BLIS and NETLIB LAPACK

### To enable CUDA build, add `-DUSE_CUDA=ON`

```
cd $GFCC_SRC/contrib/TAMM
mkdir build && cd build

CC=gcc CXX=g++ FC=gfortran cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..

# make step takes a while, please use as many cores as possible
make -j3
make install
```

Step 3: Building the GFCC library
=================================
```
cd $GFCC_SRC
mkdir build && cd build

### To enable CUDA build, add `-DUSE_CUDA=ON`

### The `cmake` invocation below is the same as in Step 2.
CC=gcc CXX=g++ FC=gfortran cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..

make -j2
```

Step 4: Running the GFCC code
=============================
`cd $GFCC_SRC/build`   
`export OMP_NUM_THREADS=1`
`mpirun -n 4 $GFCC_INSTALL_PATH/bin/GF_CCSD $GFCC_SRC/tests/co.json`


---------------------------------------------------------------------
# Advanced Build Options for TAMM/libGFCC (applies to Steps 2 and 3)
---------------------------------------------------------------------

The following configurations are to be used to replace the `cmake` command in Steps 2 and 3.   

## Build using Intel MKL

### To enable CUDA build, add `-DUSE_CUDA=ON`

```
cd $GFCC_SRC/contrib/TAMM/build

CC=gcc CXX=g++ FC=gfortran cmake -DLINALG_VENDOR=IntelMKL \
-DLINALG_PREFIX=/opt/intel/mkl \
-DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..

make -j3
make install
```

## Build instructions for Summit using ESSL

```
module load gcc/8.3.0
module load cmake/3.18.2
module load essl/6.1.0-2
module load cuda/10.1.105
```

### To enable CUDA build, add `-DUSE_CUDA=ON`

```
cd $GFCC_SRC/contrib/TAMM/build

CC=gcc CXX=g++ FC=gfortran cmake \
-DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH \
-DBLIS_CONFIG=power9 \
-DLINALG_VENDOR=IBMESSL \
-DLINALG_PREFIX=/sw/summit/essl/6.1.0-2/essl/6.1 ..

make -j3
make install
```

## Build instructions for Cori

```
module unload PrgEnv-intel/6.0.5
module load PrgEnv-gnu/6.0.5
module swap gcc/8.3.0 
module swap craype/2.5.18
module swap cray-mpich/7.7.6 (OR) module load openmpi
module unload cmake
module load cmake/3.18.2
module load cuda/10.1.168
```

```
export CRAYPE_LINK_TYPE=dynamic
```

### To enable CUDA build, add `-DUSE_CUDA=ON`

```
cd $GFCC_SRC/contrib/TAMM/build

CC=cc CXX=CC FC=ftn cmake -DLINALG_VENDOR=IntelMKL \
-DLINALG_PREFIX=/opt/intel/mkl \
-DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..

make -j3
make install
```

## Build instructions for Theta

```
module unload darshan xalt perftools-base
module swap PrgEnv-intel PrgEnv-gnu
module unload darshan xalt perftools-base
module load cmake
```

```
export CRAYPE_LINK_TYPE=dynamic
```

```
cd $GFCC_SRC/contrib/TAMM/build

CC=cc CXX=CC FC=ftn cmake -DLINALG_VENDOR=IntelMKL \
-DLINALG_PREFIX=/opt/intel/mkl \
-DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..

make -j3
make install
```


Build instructions for Mac OS
-------------------------------

```
brew install gcc openmpi cmake wget autoconf automake

export GFCC_SRC=$HOME/gfcc_src
export GFCC_INSTALL_PATH=$HOME/gfcc_install
git clone https://github.com/spec-org/gfcc.git GFCC_SRC

cd $GFCC_SRC/contrib/TAMM
mkdir build && cd build
CC=gcc-8 CXX=g++-8 FC=gfortran-8 cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..
make -j3 
make install
ctest (optional)

cd $GFCC_SRC
mkdir build && cd build
CC=gcc-8 CXX=g++-8 FC=gfortran-8 cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..
make -j2 install
```

Build instructions for Ubuntu Bionic 18.04
------------------------------------------

```
sudo apt install g++-8 gcc-8 gfortran-8 openmpi-dev

curl -LJO https://github.com/Kitware/CMake/releases/download/v3.19.4/cmake-3.19.4-Linux-x86_64.tar.gz
tar xzf cmake-3.19.4-Linux-x86_64.tar.gz
export PATH=`pwd`/cmake-3.19.4-Linux-x86_64/bin:$PATH

export GFCC_SRC=$HOME/gfcc_src
export GFCC_INSTALL_PATH=$HOME/gfcc_install
git clone https://github.com/spec-org/gfcc.git GFCC_SRC

cd $GFCC_SRC/contrib/TAMM
mkdir build && cd build
// To enable CUDA build, add -DUSE_CUDA=ON
CC=gcc-8 CXX=g++-8 FC=gfortran-8 cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..
make -j3 install
ctest (optional)

cd $GFCC_SRC
mkdir build && cd build
CC=gcc-8 CXX=g++-8 FC=gfortran-8 cmake -DCMAKE_INSTALL_PREFIX=$GFCC_INSTALL_PATH ..
make -j2 install
```
