# Maintainer: Bruno <bruno@centaure.ws>
# Contributor: Balló György <ballogyor+arch at gmail dot com>
# Contributor: Jan Alexander Steffens (heftig) <heftig@archlinux.org>
# Contributor: Ionut Biru <ibiru@archlinux.org>

pkgname=gshot
pkgver=alpha2
pkgrel=1
pkgdesc='Take pictures of your screen'
arch=(x86_64)
url='https://github.com/foss-gshot/gshot'
license=(GPL-2.0-or-later)
depends=(
  cairo
  gcc-libs
  gdk-pixbuf2
  glib2
  glibc
  gtk4
  hicolor-icon-theme
  wayland
)
makedepends=(
  appstream
  meson
  ninja
  pkgconf
)
source=(
  "https://github.com/kartracer24/gshot/raw/master/bin/gshot-$pkgver.tar.gz"
)
sha256sums=(
  '689537fbf21d700e1d2a85b4c5a78ee423be5067b39e2063434302b055deb216'
)

build() {
  meson setup build "$pkgname-$pkgver"
  meson compile -C build
}

package() {
  meson install -C build --destdir "$pkgdir"
}
