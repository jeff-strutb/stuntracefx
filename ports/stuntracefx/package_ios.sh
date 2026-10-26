#!/bin/sh
# Package the iOS build as build/stuntracefx/port/ios-SDK/Stunt Race FX.app:
# the executable (the game's data built in from SRFX_ROM, see link.sh) and
# an Info.plist. The app needs no ROM and nothing from this tree.
#   env: IOS_SDK      iphonesimulator (default) | iphoneos
#        IOS_SIGN     iphoneos: the signing identity (security find-identity)
#        IOS_PROFILE  iphoneos: the provisioning profile for the bundle id,
#                     embedded; its entitlements sign the app
# A simulator app is signed ad hoc and installs with xcrun simctl install.
set -e
cd "$(dirname "$0")/../.."
SDK=${IOS_SDK:-iphonesimulator}
case "$SDK" in
  iphonesimulator) TGT=arm64-apple-ios15.0-simulator; PLAT=iPhoneSimulator;;
  iphoneos)        TGT=arm64-apple-ios15.0;           PLAT=iPhoneOS;;
  *) echo "package_ios: unknown IOS_SDK $SDK" >&2; exit 2;;
esac
OUT=build/stuntracefx/port/ios-$SDK
HOST=ios OUT=$OUT CC="xcrun -sdk $SDK clang -target $TGT" ports/stuntracefx/link.sh
APP="$OUT/Stunt Race FX.app"
rm -rf "$APP"
mkdir -p "$APP"
cp "$OUT/stuntracefx" "$APP/Stunt Race FX"
cat > "$APP/Info.plist" <<EOP
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Stunt Race FX</string>
  <key>CFBundleDisplayName</key><string>Stunt Race FX</string>
  <key>CFBundleExecutable</key><string>Stunt Race FX</string>
  <key>CFBundleIdentifier</key><string>com.strutb.stuntracefx</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>CFBundleSupportedPlatforms</key><array><string>$PLAT</string></array>
  <key>MinimumOSVersion</key><string>15.0</string>
  <key>LSRequiresIPhoneOS</key><true/>
  <key>UIDeviceFamily</key><array><integer>1</integer><integer>2</integer></array>
  <key>UIRequiredDeviceCapabilities</key><array><string>arm64</string><string>metal</string></array>
  <key>UILaunchScreen</key><dict/>
  <key>UIRequiresFullScreen</key><true/>
  <key>UIStatusBarHidden</key><true/>
  <key>UISupportedInterfaceOrientations</key>
  <array><string>UIInterfaceOrientationLandscapeRight</string><string>UIInterfaceOrientationLandscapeLeft</string></array>
  <key>UISupportedInterfaceOrientations~ipad</key>
  <array><string>UIInterfaceOrientationLandscapeRight</string><string>UIInterfaceOrientationLandscapeLeft</string></array>
  <key>UIApplicationSceneManifest</key>
  <dict>
    <key>UIApplicationSupportsMultipleScenes</key><false/>
    <key>UISceneConfigurations</key>
    <dict>
      <key>UIWindowSceneSessionRoleApplication</key>
      <array><dict>
        <key>UISceneConfigurationName</key><string>Default</string>
        <key>UISceneDelegateClassName</key><string>PortSceneDelegate</string>
      </dict></array>
    </dict>
  </dict>
  <key>GCSupportsControllerUserInteraction</key><true/>
  <key>GCSupportedGameControllers</key>
  <array><dict><key>ProfileName</key><string>ExtendedGamepad</string></dict></array>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict>
</plist>
EOP
printf 'APPL????' > "$APP/PkgInfo"
if [ "$SDK" = iphoneos ]; then
  [ -n "$IOS_SIGN" ] && [ -f "$IOS_PROFILE" ] || { echo "package_ios: iphoneos needs IOS_SIGN and IOS_PROFILE" >&2; exit 2; }
  cp "$IOS_PROFILE" "$APP/embedded.mobileprovision"
  security cms -D -i "$IOS_PROFILE" > "$OUT/profile.plist"
  /usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$OUT/profile.plist" > "$OUT/entitlements.plist"
  codesign --force --sign "$IOS_SIGN" --entitlements "$OUT/entitlements.plist" "$APP"
else
  codesign --force --sign - "$APP"
fi
echo "package_ios: $APP"
