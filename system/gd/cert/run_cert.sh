#! /bin/bash

if [[ -z "${ANDROID_BUILD_TOP}" ]]; then
  echo "ANDROID_BUILD_TOP is not set"
fi

if [[ -z "${ANDROID_HOST_OUT}" ]]; then
  echo "ANDROID_HOST_OUT is not set for host run"
fi

# Run normal facade to cert API tests
PYTHONPATH=$PYTHONPATH:$ANDROID_BUILD_TOP/out/host/linux-x86/lib64:$ANDROID_BUILD_TOP/packages/modules/Bluetooth/system/gd python3.8 `which act.py` -c $ANDROID_BUILD_TOP/packages/modules/Bluetooth/system/gd/cert/host_only_config.json -tf $ANDROID_BUILD_TOP/packages/modules/Bluetooth/system/gd/cert/cert_testcases -tp $ANDROID_BUILD_TOP/packages/modules/Bluetooth/system/gd

