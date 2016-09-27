***********************
1.4. Installation steps
***********************
-------------------------------------------------------------------------------------------------------------------------------------------

The following are the steps to use the library

      * ROCM 1.0 Kernel, Driver and Compiler Installation (if not done until now)

      * Library installation.

1.4.1. ROCM 1.0 Installation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To Know more about ROCM  refer https://github.com/RadeonOpenCompute/ROCm/blob/master/README.md

a. Installing Debian ROCM repositories
--------------------------------------

  Before proceeding, make sure to completely uninstall any pre-release ROCm packages.

  Refer https://github.com/RadeonOpenCompute/ROCm#removing-pre-release-packages for instructions to remove pre-release ROCM packages.

  Steps to install rocm package are,



  ``wget -qO - http://packages.amd.com/rocm/apt/debian/rocm.gpg.key | sudo apt-key add -``

 
  ``sudo sh -c 'echo deb [arch=amd64] http://packages.amd.com/rocm/apt/debian/ trusty main > /etc/apt/sources.list.d/rocm.list'``


  ``sudo apt-get update``

 

  ``sudo apt-get install rocm``


  and Reboot the system

b. Verifying the Installation
-----------------------------

  Once Reboot, to verify that the ROCm stack completed successfully you can execute HSA vector_copy sample application:

       * cd /opt/rocm/hsa/sample

       * make

       * ./vector_copy


1.4.2. Library Installation
^^^^^^^^^^^^^^^^^^^^^^^^^^^

a. Install using Prebuilt debian


    ``wget https://bitbucket.org/multicoreware/hcfft/downloads/hcfft-master-9607fd5-Linux.deb``



    ``sudo dpkg -i hcfft-master-9607fd5-Linux.deb``


b. Build debian from source


    ``git clone https://bitbucket.org/multicoreware/hcfft.git && cd hcfft``


    ``chmod +x build.sh && ./build.sh``


    To use LC Compiler,


    ``export HCCLC=1``


    build.sh execution builds the library and generates a debian under build directory.

1.4.3. Library UnInstallation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

a. To uninstall the library, do

       ``chmod +x clean.sh && ./clean.sh``
