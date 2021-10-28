$NDKPath = Get-Content ./ndkpath.txt
$stackScript = "$NDKPath/ndk-stack"
if (-not ($PSVersionTable.PSEdition -eq "Core")) {
    $stackScript += ".cmd"
}
adb pull /sdcard/Android/data/com.beatgames.beatsaber/files/tombstone_00 $PSScriptRoot/tombstone_00
Get-Content $PSScriptRoot/tombstone_00 | & $stackScript -sym ./obj/local/arm64-v8a/ > $PSScriptRoot/CrashTemp.log
rm $PSScriptRoot/tombstone_00
mv $PSScriptRoot/CrashTemp.log $PSScriptRoot/lastCrash.log -Force
Write "crash stack available as lastCrash.log"