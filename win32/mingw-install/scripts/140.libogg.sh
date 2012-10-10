PKGNAME="libogg"
PKGVER="1.3.0"

SOURCE=http://downloads.xiph.org/releases/ogg/${PKGNAME}-${PKGVER}.tar.gz

pushd ../source
wget -c $SOURCE
popd 

tar xzf ../source/${PKGNAME}-${PKGVER}.tar.gz

cd ${PKGNAME}-${PKGVER}

LDFLAGS='-mwindows' ./configure --prefix=/mingw/local
make -j 4
make install