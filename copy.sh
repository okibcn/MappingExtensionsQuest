#!/bin/bash

NDKPath=`cat ndkpath.txt`

buildScript="$NDKPath/build/ndk-build"

./$buildScript NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
adb push libs/arm64-v8a/libmappingextensions.so /sdcard/Android/data/com.beatgames.beatsaber/files/mods/libmappingextensions.so
adb shell am force-stop com.beatgames.beatsaber
#adb shell input keyevent KEYCODE_DPAD_RIGHT
#adb shell input keyevent KEYCODE_SPACE

adb shell am start com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity

adb logcat > test.log