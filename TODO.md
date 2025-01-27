isScriptingSupported()
- [x] THEMESDOWNLOADER
- [ ] RETROACHIVEMENTS
- [ ] KODI
- [x] WIFI
  - [ ] enableWifi
  - [x] disableWifi
  - [ ] getWifiNetworks
- [ ] BLUETOOTH
  - [x] enableBluetooth
  - [x] disableBluetooth
  - [x] startBluetoothLiveDevices
  - [x] stopBluetoothLiveDevices
  - [x] pairBluetoothDevice
  - [x] connectBluetoothDevice
  - [x] disconnectBluetoothDevice
  - [x] removeBluetoothDevice
  - [x] getPairedBluetoothDeviceList
  - [x] forgetBluetoothControllers
  - [ ] scanNewBluetooth (marked as "Obsolete", but used in GUI)
- [ ] RESOLUTION
  - [ ] getVideoModes
    > Check [batocera-resolution.wayland-sway](https://github.com/batocera-linux/batocera.linux/blob/4158b1c5602461f8dabe965541cda9c6f3d73dfa/package/batocera/core/batocera-resolution/scripts/resolution/batocera-resolution.wayland-sway)
- [ ] BIOSINFORMATION
- [ ] DISKFORMAT
- [ ] OVERCLOCK
- [ ] NETPLAY
- [ ] PDFEXTRACTION
- [ ] BATOCERASTORE
- [ ] THEBEZELPROJECT
- [ ] PADSINFO
- [ ] EVMAPY
- [ ] BATOCERAPREGAMELISTSHOOK
- [x] TIMEZONES
- [ ] AUDIODEVICE
- [ ] BACKUP
- [ ] INSTALL
    > For installation of batocera
    > Looks like unnecessary
- [ ] SUPPORTFILE
- [x] UPGRADE
- [ ] SUSPEND
- [ ] READPLANEMODE, WRITEPLANEMODE
    > Check [batocera-planemode](https://github.com/batocera-linux/batocera.linux/blob/4158b1c5602461f8dabe965541cda9c6f3d73dfa/package/batocera/core/batocera-scripts/scripts/batocera-planemode)
    > TLDR:
    > isPlaneMode = blEnabled && wifiEnabled
    > Disable planeMode is enable bt and enable wifi
  - [ ] setPlaneMode
  - [ ] isPlaneMode
- [x] SERVICES **(uses system services, so need root)**
- [ ] BACKGLASS

Extra:
- [x] getSystemInformations
> "Architecture" and "Available Memory" implemented
> Full list is here: https://github.com/batocera-linux/batocera.linux/blob/4158b1c5602461f8dabe965541cda9c6f3d73dfa/package/batocera/core/batocera-scripts/scripts/batocera-info
- [x] Brighntees **(rights needs for /sys/class/backlight)**
