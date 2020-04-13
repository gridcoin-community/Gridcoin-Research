
const core = require('@actions/core');
const github = require('@actions/github');

try {
  const host = core.getInput('host');
  switch (host) {
      case "x86_64-unknown-linux-gnu": {
        core.setOutput("pkgmgnrdepends", "ccache libqt5gui5 libqt5core5a qtbase5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools libqt5charts5-dev libssl-dev libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-iostreams-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev protobuf-compiler libqrencode-dev xvfb libzip4 libzip-dev zlib1g zlib1g-dev libcurl4-gnutls-dev");
        core.setOutput("config", "--disable-dependency-tracking --with-incompatible-bdb --with-gui=qt5 --enable-reduce-export")
      }

  }
} catch (error) {
  core.setFailed(error.message);
}