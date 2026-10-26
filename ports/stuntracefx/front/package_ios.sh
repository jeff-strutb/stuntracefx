#!/bin/sh
# Stunt Race FX for iOS on nativeshvc, as build/stuntracefx/front/ios-SDK/
# Stunt Race FX.app: the front end (front/build.sh) on nativeshvc's iOS host,
# the game's data built in (the ios assets: TAP TO START), an Info.plist.
#   IOS_SDK      iphonesimulator (default) | iphoneos
#   IOS_SIGN     iphoneos: the signing identity (security find-identity)
#   IOS_PROFILE  iphoneos: the provisioning profile for the bundle id
#   NSHVC        the console's checkout (../nativeshvc); its library for the
#                SDK is built at NSHVC/build/ios-SDK if it is not there
# A simulator app is signed ad hoc and installs with xcrun simctl install.
set -e
cd "$(dirname "$0")/../../.."
SDK=${IOS_SDK:-iphonesimulator}
NSHVC=${NSHVC:-../nativeshvc}
case "$SDK" in
  iphonesimulator) TGT=arm64-apple-ios15.0-simulator; PLAT=iPhoneSimulator;;
  iphoneos)        TGT=arm64-apple-ios15.0;           PLAT=iPhoneOS;;
  *) echo "package_ios: unknown IOS_SDK $SDK" >&2; exit 2;;
esac
NB=build/ios-$SDK
if [ ! -f "$NSHVC/$NB/libnshvc.a" ]; then
  cmake -S "$NSHVC" -B "$NSHVC/$NB" -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=$SDK \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DNSHVC_HOST=ios -DNSHVC_RENDER=metal >/dev/null
fi
cmake --build "$NSHVC/$NB" >/dev/null
OUT=build/stuntracefx/front/ios-$SDK
NSHVC="$NSHVC" NSHVC_BUILD=$NB OUT=$OUT ASSETS=ios CC="xcrun -sdk $SDK clang -target $TGT" DEFS=-DNSHVC_HOST_IOS=1 \
  LIBS="-framework UIKit -framework CoreGraphics -framework QuartzCore -framework AudioToolbox -framework AVFoundation -framework GameController -framework Metal -framework Foundation" \
  sh ports/stuntracefx/front/build.sh
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
  # a wildcard profile's app id (TEAM.*) names this app in the signature
  TEAM=$(/usr/libexec/PlistBuddy -c 'Print :TeamIdentifier:0' "$OUT/profile.plist")
  /usr/libexec/PlistBuddy -c "Set :application-identifier $TEAM.com.strutb.stuntracefx" "$OUT/entitlements.plist"
  codesign --force --sign "$IOS_SIGN" --entitlements "$OUT/entitlements.plist" "$APP"
else
  codesign --force --sign - "$APP"
fi
echo "package_ios: $APP"
