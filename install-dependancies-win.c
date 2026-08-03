#include <stdio.h>
#include <stdlib.h>

int main(void) {
  const char *script =
    "Set-StrictMode -Version Latest; "
    "$ErrorActionPreference='Stop'; "
    "Write-Host 'Installing Salvium GUI build dependencies via MSYS2 UCRT64...'; "
    "$bash='C:\\msys64\\usr\\bin\\bash.exe'; "
    "if (!(Test-Path $bash)) { "
    "  if (Get-Command winget -ErrorAction SilentlyContinue) { "
    "    winget install --id MSYS2.MSYS2 -e --accept-package-agreements --accept-source-agreements; "
    "  } else { "
    "    throw 'MSYS2 is not installed and winget is unavailable. Install MSYS2 from https://www.msys2.org/ and rerun this installer.'; "
    "  } "
    "} "
    "$bash='C:\\msys64\\usr\\bin\\bash.exe'; "
    "if (!(Test-Path $bash)) { throw 'MSYS2 bash was not found at C:\\msys64\\usr\\bin\\bash.exe after installation.'; } "
    "& $bash -lc 'pacman -Sy --needed --noconfirm base-devel git make cmake pkgconf python ccache gettext-devel gperf bison flex autoconf automake libtool zip unzip mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-zeromq mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-unbound mingw-w64-ucrt-x86_64-protobuf mingw-w64-ucrt-x86_64-libusb mingw-w64-ucrt-x86_64-libgcrypt mingw-w64-ucrt-x86_64-libgpg-error mingw-w64-ucrt-x86_64-icu mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-zstd mingw-w64-ucrt-x86_64-xz mingw-w64-ucrt-x86_64-libiconv mingw-w64-ucrt-x86_64-libpgm mingw-w64-ucrt-x86_64-readline mingw-w64-ucrt-x86_64-qt5-base mingw-w64-ucrt-x86_64-qt5-declarative mingw-w64-ucrt-x86_64-qt5-svg mingw-w64-ucrt-x86_64-qt5-quickcontrols mingw-w64-ucrt-x86_64-qt5-quickcontrols2 mingw-w64-ucrt-x86_64-qt5-graphicaleffects mingw-w64-ucrt-x86_64-qt5-multimedia mingw-w64-ucrt-x86_64-qt5-virtualkeyboard mingw-w64-ucrt-x86_64-qt5-winextras mingw-w64-ucrt-x86_64-qt5-xmlpatterns mingw-w64-ucrt-x86_64-qt5-tools mingw-w64-ucrt-x86_64-qt5-static'; "
    "if ($LASTEXITCODE -ne 0) { throw ('pacman failed with exit code ' + $LASTEXITCODE); } "
    "Write-Host 'Dependency installation complete. Build from the MSYS2 UCRT64 shell with: make release-static';";

  char command[16384];
  int written = snprintf(
    command,
    sizeof(command),
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"%s\"",
    script
  );

  if (written < 0 || written >= (int)sizeof(command)) {
    fprintf(stderr, "internal error: PowerShell command is too long\n");
    return 1;
  }

  return system(command);
}
