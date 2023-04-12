let pkgs = import <nixpkgs> {};
in
with pkgs.stdenv;
with pkgs.stdenv.lib;

pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [ autoreconfHook cmake libtool pkg-config qt5.wrapQtAppsHook ];
  buildInputs = with pkgs; [ boost openssl libevent curl qt5.qttools libzip qrencode ];

  shellHook = ''
    echo "Run configure with: --with-qt-bindir=${pkgs.qt5.qtbase.dev}/bin:${pkgs.qt5.qttools.dev}/bin --with-boost-libdir=${pkgs.boost.out}/lib"
  '';
}
