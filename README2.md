# General setup
Please take note that /bin/sh should not be running dash. You can make sure running readlink /bin/sh.
To be able to set up the build environment use '. init-oe.sh (MACHINE name i.e. int-txo (default), idesk)'
The home repository is currently used as the build directory. This is to allow the windows folder structure to be ignored.


Machines currenlty supported:
1. inttxo
2. idesk
3. idesk-vp

Use the following command to create an output folder with the binaries of the specific machine. of the 
> python copy_output_files.py MACHINE


## Setup of modules
Currently submodules are used for the parts that are yocto specific. To populate these run the following:
> git submodule init \
> git submodule update


# Building on windows
- Docker desktop is required. 
- Login to https://artifactory.keen.tech/ui/packages, 
- Left top press on your icon, select 'set me up' and choose docker. 
- Do the setup by simply placing the config.json file in C:\Users\mgj1ein\.docker\ folder and then do the docker login to authenticate
- start the build with 'run.cmd bash' then e.g '. init.oe idesk-vp' then 'bitbake idesk-vp-image'
```
run.cmd bash
init.oe idesk-vp
bitbake idesk-vp-image
```
- you will notice the error The TMPDIR (/workdir/build/build/tmp-glibc) can't be on a case-insensitive file system, after attempting the first build
- delete /workdir/build/* then on your windows machine start a administrator cmd and run the below command, where build directory is the same build directory mounted to the docker image
```
fsutil.exe file setCaseSensitiveInfo C:\workspace\BT-CO-ENG\Conference_Yocto_EmbeddedLinux_Distro\build\
```
- improvements
in %userprofile%\.wslconfig add the folllowing
```
memory=8GB
processors=4
```
- in run.cmd consider configuring --shm-size to 8192M