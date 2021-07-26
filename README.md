# MpClientHarness

An Harness implemented for MpClient.dll
As we all know the Windows Defender Client MpCmdRun is kind of an odd ball to fuzz hence this is an harness for the MpClinet.dll

# How to use it
To fuzz it with winafl , use the following commands :
```
afl-fuzz -i in -o out -t 2000+ -D \DynamoRIO\bin64\ -- -coverage_module MpClient.dll -target_module mpharness.exe -target_offset 0x1500 -fuzz_iterations 5000 -covtype edge -nargs 2 -- mpharness.exe @@ 
```
