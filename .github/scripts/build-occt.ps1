# SPDX-License-Identifier: AGPL-3.0-only

$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$sourceDirectory = Join-Path $repositoryRoot ".ci/occt-source"
$buildDirectory = Join-Path $repositoryRoot ".ci/occt-build"
$installDirectory = Join-Path $repositoryRoot ".ci/occt-install"

$arguments = @(
  "-S", $sourceDirectory,
  "-B", $buildDirectory,
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
  "-DBUILD_LIBRARY_TYPE=Static",
  "-DBUILD_MODULE_FoundationClasses=OFF",
  "-DBUILD_MODULE_ModelingData=OFF",
  "-DBUILD_MODULE_ModelingAlgorithms=OFF",
  "-DBUILD_MODULE_Visualization=OFF",
  "-DBUILD_MODULE_ApplicationFramework=OFF",
  "-DBUILD_MODULE_DataExchange=OFF",
  "-DBUILD_MODULE_Draw=OFF",
  "-DBUILD_ADDITIONAL_TOOLKITS=TKSTEP TKMesh TKShHealing",
  "-DUSE_TBB=OFF",
  "-DUSE_FREETYPE=OFF",
  "-DUSE_TCL=OFF",
  "-DINSTALL_DIR=$installDirectory",
  "-DINSTALL_DIR_LIB=lib",
  "-DINSTALL_DIR_INCLUDE=include/opencascade",
  "-DINSTALL_DIR_CMAKE=cmake"
)

if ($IsMacOS) {
  $arguments += "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
  $arguments += "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"
}

cmake @arguments
if ($LASTEXITCODE -ne 0) {
  throw "OCCT configure failed with exit code $LASTEXITCODE"
}

cmake --build $buildDirectory --config Release --parallel 2
if ($LASTEXITCODE -ne 0) {
  throw "OCCT build failed with exit code $LASTEXITCODE"
}

cmake --install $buildDirectory --config Release
if ($LASTEXITCODE -ne 0) {
  throw "OCCT install failed with exit code $LASTEXITCODE"
}

$configFile = Join-Path $installDirectory "cmake/OpenCASCADEConfig.cmake"
if (-not (Test-Path -LiteralPath $configFile)) {
  throw "OCCT package configuration was not installed at $configFile"
}
