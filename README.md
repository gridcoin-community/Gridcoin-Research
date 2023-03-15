What is Gridcoin?
=================

Gridcoin is a POS-based cryptocurrency that rewards users for participating on the [BOINC](https://boinc.berkeley.edu/) network.
Gridcoin uses peer-to-peer technology to operate with no central authority - managing transactions, issuing money and contributing to scientific research are carried out collectively by the network.

For Gridcoin binaries, as well as more information, see https://gridcoin.us/.

Building Gridcoin
=================

These dependencies are required:

 Library      | Purpose          | Description
 -------------|------------------|----------------------------------------------------------------
 cmake        | Build            | Build system (optional)
 pkgconf      | Build            | Learn library inter-dependencies
 openssl      | Crypto           | Random Number Generation, Elliptic Curve Cryptography
 libboost     | Utility          | Library for threading, data structures, etc
 libcurl      | Utility          | URL client library
 libzip       | Utility          | Library for manipulating zip archives
 miniupnpc    | UPnP Support     | Firewall-jumping support (optional)
 qt5          | GUI              | GUI toolkit (optional)
 libqrencode  | QR codes in GUI  | Library for encoding data in a QR Code symbol (optional, depends on GUI)

To build, run:

* With CMake:

  `mkdir build && cmake build && cmake .. && cmake --build .`

* With Autotools:

  `./autogen.sh && ./configure && make`

For more detailed and platform-specific instructions, see [the doc folder.](doc/)

Development process
===================

Developers work in their own trees, then submit pull requests to the
development branch when they think their feature or bug fix is ready.

The patch will be accepted if there is broad consensus that it is a
good thing. Developers should expect to rework and resubmit patches
if they don't match the project's coding conventions (see [coding.txt](doc/coding.txt))
or are controversial.

The master branch is regularly built and tested, but is not guaranteed
to be completely stable. [Tags](https://github.com/gridcoin-community/Gridcoin-Research/tags) are regularly created to indicate new
stable release versions of Gridcoin.

Feature branches are created when there are major new features being
worked on by several people.

Branching strategy
==================

Gridcoin uses four branches to ensure stability without slowing down
the pace of the daily development activities - *development*, *staging*, *master*
and *hotfix*.

The *development* branch is used for day-to-day activities. It is the most
active branch and is where pull requests go by default. This branch may contain
code which is not yet stable or ready for production, so it should only be
executed on testnet to avoid disrupting fellow Gridcoiners.

When a decision has been made that the development branch should be moving
towards a final release it is merged to *staging* where no new development
takes place. This branch is purely to stabilize the code base and squash out
bugs rained down from development. This is Gridcoin's beta testing phase.

Once the staging branch is stable and runs smoothly, it is merged to *master*, a tag is created,
and a release is made available to the public.

When a bug is found in a production version and an update needs to be
released quickly, the changes go into a *hotfix* branch for testing before
being merged to *master* for release. This allows for production updates without having to merge straight to
master if the staging branch is busy.

Community
=========

For general questions, please visit our Discord server at https://discord.gg/jf9XX4a, or Libera Chat in #gridcoin-help. We also have a Slack channel at [teamgridcoin.slack.com](https://join.slack.com/t/teamgridcoin/shared_invite/zt-3s81akww-GHt~_KvtxfhxUgi3yW3~Bg).

License
-------

Gridcoin is released under the terms of the MIT license. See [COPYING](COPYING) or https://opensource.org/licenses/MIT for more
information.

Build Status
============

| Development                                                                                                                            | Staging                                                                                                                            | Master                                                                                                                            |
|----------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| ![Build Status](https://github.com/gridcoin-community/Gridcoin-Research/actions/workflows/ci.yml/badge.svg?branch=development) | ![Build Status](https://github.com/gridcoin-community/Gridcoin-Research/actions/workflows/ci.yml/badge.svg?branch=staging) | ![Build Status](https://github.com/gridcoin-community/Gridcoin-Research/actions/workflows/ci.yml/badge.svg?branch=master) |
