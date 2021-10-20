#!/bin/bash

NDKPath=`cat ndkpath.txt`

buildScript="$NDKPath/build/ndk-build"

./$buildScript NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
mkdir QMODBuild
cp "./mod.json" "./libs/arm64-v8a/libmappingextensions.so" ./QMODBuild
cd QMODBuild
zip "../MappingExtensions.qmod" *
cd ..
rm -r QMODBuild