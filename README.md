What is Gridcoin?
=================

Gridcoin is a POS-based cryptocurrency that rewards users for participating on the BOINC network. 
Gridcoin uses peer-to-peer technology to operate with no central authority: managing transactions, issuing money and contributing to scientific research are carried out collectively by the network. 

For Gridcoin binaries, as well as more information, see http://gridcoin.us/ . 

Development process
===========================

The master branch is regularly built and tested, but is not guaranteed 
to be completely stable. [Tags](https://github.com/gridcoin/Gridcoin-Research/tags)
are created regularly to indicate new official, stable release versions 
of Gridcoin.

Developers work in their own trees, then submit pull requests to the
development branch when they think their feature or bug fix is ready.

The patch will be accepted if there is broad consensus that it is a
good thing.  Developers should expect to rework and resubmit patches
if they don't match the project's coding conventions (see coding.txt)
or are controversial.

The master branch is regularly built and tested, but is not guaranteed
to be completely stable. Tags are regularly created to indicate new
stable release versions of Gridcoin.

Feature branches are created when there are major new features being
worked on by several people.

From time to time a pull request will become outdated. If this occurs, and
the pull is no longer automatically mergeable; a comment on the pull will
be used to issue a warning of closure. The pull will be closed 15 days
after the warning if action is not taken by the author. Pull requests closed
in this manner will have their corresponding issue labeled 'stagnant'.

Issues with no commits will be given a similar warning, and closed after
15 days from their last activity. Issues closed in this manner will be 
labeled 'stale'.


Branching strategy
==================

Gridcoin uses four branches to ensure stability without slowing down
the pace of the daily development activities; development, staging, master
and hotfix.

The *development* branch is used for day-to-day activities. It is the most
active branch and is where pull requests go by default. This branch may contain
code which is not yet stable or ready for production, so it should only be
executed on testnet to avoid disrupting fellow Gridcoiners. Using this branch
is the equivalence of alpha testing.

When a decision has been made that the development branch should be moving
towards a final release it is merged to *staging* where no new development
takes place. This branch is purely to stabilize the code base and squash out
bugs rained down from development. Using this is close to beta testing.

Once the staging branch is stable and runs smoothly it is merged to *master*
and a release is made available to the public. At this point the code is
considered mature and ready for production.

When a bug is found in a production version and an update is needed to be
released quickly the changes go into the *hotfix* branch for test before
being merged to *master* for release. This shows the intent of the change
and allows for production updates without having to merge straight to
master if the staging branch is busy.

Community
============

For general questions see the forum https://cryptocurrencytalk.com/forum/464-gridcoin-grc/ , or freenode irc on #gridcoin. We also have a slack channel on teamgridcoin.slack.com .

License
--------

Gridcoin is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.


Build Status
=============

| Development                                                                                                                            | Staging                                                                                                                            | Master                                                                                                                            |
|----------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| [![Build Status](https://travis-ci.org/gridcoin/Gridcoin-Research.svg?branch=development)](https://travis-ci.org/gridcoin/Gridcoin-Research) | [![Build Status](https://travis-ci.org/gridcoin/Gridcoin-Research.svg?branch=staging)](https://travis-ci.org/gridcoin/Gridcoin-Research) | [![Build Status](https://travis-ci.org/gridcoin/Gridcoin-Research.svg?branch=master)](https://travis-ci.org/gridcoin/Gridcoin-Research) |
