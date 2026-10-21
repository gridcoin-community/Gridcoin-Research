let pkgs = import <nixpkgs> {};
in
with pkgs.stdenv;
with pkgs.stdenv.lib;

pkgs.mkShell {
  nativeBuildInputs = with pkgs.buildPackages; [ cmake pkg-config qt5.wrapQtAppsHook ];
  buildInputs = with pkgs; [ boost openssl libevent curl qt5.qttools libzip qrencode ];

  shellHook = ''
    echo "Run: cmake -B build -DENABLE_GUI=ON -DUSE_QT6=OFF && cmake --build build"
  '';
}
