isScriptingSupported
- [x] THEMESDOWNLOADER
- [ ] RETROACHIVEMENTS
- [ ] KODI
- [x] WIFI
- [x] BLUETOOTH
- [ ] RESOLUTION
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
- [ ] SUPPORTFILE
- [x] UPGRADE
- [ ] SUSPEND
- [ ] READPLANEMODE
- [ ] WRITEPLANEMODE
- [ ] SERVICES
- [ ] BACKGLASS


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

- [ ] WIFI
  - [ ] enableWifi
  - [x] disableWifi
  - [ ] getWifiNetworks

- [ ] Brighntees **(rights needs for /sys/class/backlight)**
  - [x] getBrightness
  - [x] setBrightness

- [ ] Resolution
  - [ ] getVideoModes

- [ ] BatoceraPackages
  - [ ] std::vector<PacmanPackage> getBatoceraStorePackages();
  - [ ] std::pair<std::string, int> installBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
  - [ ] std::pair<std::string, int> uninstallBatoceraStorePackage(std::string name, const std::function<void(const std::string)>& func = nullptr);
  - [ ] void updateBatoceraStorePackageList();
  - [ ] void refreshBatoceraStorePackageList();
