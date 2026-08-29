# Kronos VST3 Cloud Build Guide

This guide describes how the **Kronos Synth VST3** is built and packaged in the cloud using **GitHub Actions**. It details the setup, commands, and secrets required for both the Windows and macOS build pipelines.

---

## 1. Overview of the Build System

We use GitHub Actions to automate compiles and build installer binaries. The workflow is configured in [.github/workflows/build.yml](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/.github/workflows/build.yml).

### Workflow Triggers
The workflow runs automatically under these conditions:
```yaml
on:
  push:
    branches:
      - main
      - master
  pull_request:
    branches:
      - main
      - master
  workflow_dispatch: # Allows manual triggers from the GitHub Actions tab
```

---

## 2. Windows Pipeline (`build-windows`)

Runs on a `windows-2022` environment (Visual Studio 2022 build tools pre-installed) to generate the 64-bit Windows installer.

### Steps & Code Examples

#### Step A: Setup JUCE & Adjust Paths
The pipeline clones the JUCE framework and updates path definitions inside [Kronos.jucer](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/Kronos.jucer). This ensures that module paths resolve dynamically on the cloud server instead of relying on hardcoded paths on a local drive:

```bash
# Clone JUCE framework (v8.0.13)
git clone --branch 8.0.13 --depth 1 https://github.com/juce-framework/JUCE.git JUCE
```

The script then rewrites paths inside the `.jucer` file using Python:
```python
import os
workspace = os.environ.get("GITHUB_WORKSPACE", "")
# Convert to absolute path on the build runner
modules_path = os.path.join(workspace, "JUCE", "modules").replace("\\", "/")

with open("Kronos.jucer", "r") as f:
    text = f.read()
# Replace default paths with the runner's workspace path
text = text.replace("C:/JUCE/modules", modules_path)
text = text.replace("useGlobalPath=\"1\"", "useGlobalPath=\"0\"")

with open("Kronos.jucer", "w") as f:
    f.write(text)
```

#### Step B: Generate Visual Studio Solution
Using the Projucer CLI, we save the `.jucer` file to automatically generate the MSBuild solution files:

```bash
# Re-save the jucer project to update builds
./Projucer.exe --resave Kronos.jucer
```

#### Step C: Build using MSBuild
Once Visual Studio solution files are generated, we restore dependencies and run the build:

```bash
# Restore NuGet Packages (if any)
nuget restore Builds/VisualStudio2022/Kronos.sln

# Build VST3 in Release mode for x64 architecture
msbuild Builds/VisualStudio2022/Kronos.sln /p:Configuration=Release /p:Platform=x64
```

#### Step D: Compile the Installer (Inno Setup)
An installer is compiled using the configuration defined in [installer_win.iss](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/installer_win.iss):

```bash
# Compile installer executable
iscc installer_win.iss
```
This produces `Output/Kronos_Windows_Installer.exe`, which copies the VST3 bundle `Kronos.vst3` into `{commoncf}\VST3\Algebra Within\`.

---

## 3. macOS Pipeline (`build-mac`)

*Note: This job is currently disabled (`if: false` in the workflow) to save CI resources, but contains all configurations needed to sign and notarize macOS binaries.*

Runs on a `macos-14` runner (M-series Silicon runner with Xcode 15+).

### Steps & Code Examples

#### Step A: Generate Xcode Project
Similarly to Windows, macOS builds download the macOS version of the Projucer CLI and resave the project:

```bash
# Generate Xcode project structure
./Projucer.app/Contents/MacOS/Projucer --resave Kronos.jucer
```

#### Step B: Temporary Keychain & Code Signing Setup
To sign binaries in the cloud, we construct a temporary keychain, import Apple certificates, and unlock them without prompting interactive dialogs:

```bash
# Create custom temporary keychain
security create-keychain -p "temp_password" "$RUNNER_TEMP/app-signing.keychain-db"
security default-keychain -s "$RUNNER_TEMP/app-signing.keychain-db"
security unlock-keychain -p "temp_password" "$RUNNER_TEMP/app-signing.keychain-db"

# Import Developer ID Application Certificate (stored as base64 in secrets)
echo "$MACOS_CERTIFICATE" | base64 --decode -o certificate.p12
security import certificate.p12 -k "$RUNNER_TEMP/app-signing.keychain-db" -P "$CERTIFICATE_PASSWORD" -T /usr/bin/codesign
```

#### Step C: Universal Binary Compilation
Builds the project for both Intel (`x86_64`) and Apple Silicon (`arm64`) architectures:

```bash
xcodebuild -project Builds/MacOSX/Kronos.xcodeproj \
  -scheme "Kronos - VST3" \
  -configuration Release \
  ARCHS="x86_64 arm64" \
  ONLY_ACTIVE_ARCH=NO \
  CODE_SIGN_IDENTITY="-" \
  ENABLE_USER_SCRIPT_SANDBOXING=NO
```

#### Step D: Code Signing & Apple Notarization
To run on modern macOS without security warnings, the plugin bundle must be signed and notarized:

```bash
# Sign VST3 Bundle
codesign --force --options runtime --timestamp --deep --sign "Developer ID Application: Your Company Name" "Kronos.vst3"

# Zip for Apple notary service submission
zip -r "Kronos_to_notarize.zip" "Kronos.vst3"

# Submit to Apple Notary Service
xcrun notarytool submit "Kronos_to_notarize.zip" \
  --apple-id "$APPLE_ID" \
  --password "$APPLE_APP_SPECIFIC_PASSWORD" \
  --team-id "$APPLE_TEAM_ID" \
  --wait

# Staple the notarization ticket to the bundle
xcrun stapler staple "Kronos.vst3"
```

#### Step E: Installer Package Creation
The VST3 is copied into an installation root directory and compiled into a signed `.pkg` installer:

```bash
# Create installation hierarchy
mkdir -p pkgroot/Library/Audio/Plug-Ins/VST3/Algebra\ Within/
cp -R "Kronos.vst3" pkgroot/Library/Audio/Plug-Ins/VST3/Algebra\ Within/

# Build component package
pkgbuild --root pkgroot \
         --identifier com.AlgebraWithin.Kronos.pkg \
         --version 1.0.0 \
         --install-location / \
         KronosComponent.pkg

# Build distribution installer & sign it
productbuild --package KronosComponent.pkg --sign "Developer ID Installer: Your Company Name" Kronos_macOS_Installer.pkg
```

---

## 4. Necessary GitHub Actions Secrets

To run the workflow successfully with signing and notarization, you must define the following secrets in your repository settings under **Settings > Secrets and variables > Actions**:

| Secret Name | Description |
| :--- | :--- |
| `MACOS_CERTIFICATE` | Base64-encoded string of your `.p12` macOS Developer ID Application certificate. |
| `MACOS_CERTIFICATE_PASSWORD` | Password for the Developer ID Application certificate. |
| `MACOS_INSTALLER_CERTIFICATE` | Base64-encoded string of your `.p12` Developer ID Installer certificate. |
| `MACOS_INSTALLER_CERTIFICATE_PASSWORD` | Password for the Developer ID Installer certificate. |
| `APPLE_ID` | Your Apple Developer Account email address. |
| `APPLE_APP_SPECIFIC_PASSWORD` | An app-specific password generated on appleid.apple.com (required for notarization). |
| `APPLE_TEAM_ID` | The 10-character team ID of your Apple Developer Account. |
