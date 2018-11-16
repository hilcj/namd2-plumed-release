cd ~/apps/; mkdir temp; cd temp;
wget ftp://ftp.fftw.org/pub/fftw/fftw-3.3.8.tar.gz;
tar xf *;

cd fftw*;
./configure --prefix ~/.local --enable-single;
make -j4;
make install;



# On v23 do:
# $   scp -r ~/apps/release vxx:~/apps/

APPDIR=${HOME}/apps/
LOCAL_DIR=${HOME}/.local
mkdir $APPDIR

# 1. Compile plumed.
PLUMED=plumed-2.4.2
PLUMED_SOURCE=$PLUMED
cd $APPDIR
mkdir $PLUMED
cd $PLUMED

# Deploy source file.
cp -rf ${HOME}/apps/release/${PLUMED_SOURCE} .
cd $PLUMED_SOURCE

# Build
./configure --disable-mpi --prefix ${HOME}/apps/${PLUMED}/build | grep matheval
# CHECK whether matheval is there or not.

make -j8 && make install
cp ${HOME}/apps/release/namd_plumed_patch/* ${HOME}/apps/${PLUMED}/build/lib/plumed/patches/

# Load plumed into the working environment.
cp ${HOME}/apps/release/plumed_enable ${LOCAL_DIR}/bin/
source plumed_enable

# 2.1 Compile NAMD2 with cuda.
NAMD=namd-2.13
NAMD_SOURCE=NAMD_2.13_Source
cd $APPDIR
mkdir $NAMD
cd $NAMD

# Deploy source file.
cp ${HOME}/apps/release/${NAMD_SOURCE}
cd $NAMD_SOURCE

# Compile charmm
tar -xf charm*
cd charm*
./build charm++ multicore-linux-x86_64 --with-production
cd ..

# Download tcl
wget http://www.ks.uiuc.edu/Research/namd/libraries/tcl8.5.9-linux-x86_64.tar.gz
wget http://www.ks.uiuc.edu/Research/namd/libraries/tcl8.5.9-linux-x86_64-threaded.tar.gz
tar xzf tcl8.5.9-linux-x86_64.tar.gz
tar xzf tcl8.5.9-linux-x86_64-threaded.tar.gz
mv tcl8.5.9-linux-x86_64 tcl
mv tcl8.5.9-linux-x86_64-threaded tcl-threaded

# Configure namd2
nano arch/Linux-x86_64.fftw3
nano arch/Linux-x86_64.cuda

# Compile and deploy
./config Linux-x86_64-g++ --charm-arch multicore-linux-x86_64 --with-cuda --with-fftw3
cd Linux-x86_64-g++
make -j16;
cd ../
mv Linux-x86_64-g++ ../build_cuda;
cd ../
ln build_cuda/namd2 ${LOCAL_DIR}/bin/namd2-cuda

# 2.2 Compile NAMD2 with plumed and cuda.
# Fast version, assume 2.1 is done.
cd $APPDIR
cd $NAMD
cd $NAMD_SOURCE
./config Linux-x86_64-g++ --charm-arch multicore-linux-x86_64 --with-cuda --with-fftw3
plumed patch -p -e namd-2.13
cd Linux-x86_64-g++
make -j16;
cd ../
mv Linux-x86_64-g++ ../build_plumed_cuda;
cd ../
ln build_plumed_cuda/namd2 ${LOCAL_DIR}/bin/namd2-plumed-cuda


# Complete version
/*
NAMD=namd-2.13b1
NAMD_SOURCE=NAMD_2.13b1_Source
cd $APPDIR
mkdir $NAMD
cd $NAMD

# Deploy source file.
cp ${HOME}/apps/release/${NAMD_SOURCE}.tar.gz .
tar xf ${NAMD_SOURCE}.tar.gz
cd $NAMD_SOURCE

# Compile charmm
tar -xf charm*
cd charm*
./build charm++ multicore-linux-x86_64 --with-production
cd ..

# Download tcl
  wget http://www.ks.uiuc.edu/Research/namd/libraries/tcl8.5.9-linux-x86_64.tar.gz
  wget http://www.ks.uiuc.edu/Research/namd/libraries/tcl8.5.9-linux-x86_64-threaded.tar.gz
  tar xzf tcl8.5.9-linux-x86_64.tar.gz
  tar xzf tcl8.5.9-linux-x86_64-threaded.tar.gz
  mv tcl8.5.9-linux-x86_64 tcl
  mv tcl8.5.9-linux-x86_64-threaded tcl-threaded

# Configure namd2
nano arch/Linux-x86_64.fftw3
nano arch/Linux-x86_64.cuda

# Compile and deploy
./config Linux-x86_64-g++ --charm-arch multicore-linux-x86_64 --with-cuda --with-fftw3
plumed patch -p -e namd-2.13
cd Linux-x86_64-g++
make -j4
cd ../
mv Linux-x86_64-g++ ../build_plumed_cuda
cd ../
ln build_plumed_cuda/namd2 ${LOCAL_DIR}/bin/namd2-plumed-cuda


