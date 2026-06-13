# Maintainer: Oleh <oleh@example.com>
pkgname=gtk-sine-generator-git
pkgver=v0.0.3.r3.g417fdac
pkgrel=1
pkgdesc="Simple sine wave generator with GTK3 GUI and logarithmic frequency sweep"
arch=('x86_64')
url="https://github.com/ixnewton/simpleSineWaveGenerator"
license=('MIT')
depends=('gtk3')
makedepends=('gcc' 'pkg-config' 'git' 'libpulse')
optdepends=('pulseaudio: PulseAudio backend (or pipewire-pulse)')
source=("git+https://github.com/ixnewton/simpleSineWaveGenerator.git#branch=Sweep-Generator-aur")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/simpleSineWaveGenerator"
  git describe --long --tags --always | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd "$srcdir/simpleSineWaveGenerator"
  make -f Makefile.gtk
}

package() {
  cd "$srcdir/simpleSineWaveGenerator"
  
  # Install binary
  install -Dm755 gtk_sine_generator "$pkgdir/usr/bin/gtk_sine_generator"
  
  # Install license
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
  
  # Install documentation
  install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
}
