EasyWinBuilder v0.3
===============
(c) 2013 phelix / blockchained.com - MIT license

Download environment software, all dependencies and build Bitcoin/Namecoin/Altcoin automatically. To run simply double click __all_easywinbuilder.bat or manually start the batch files in order. Building Bitcoin on Windows really is a pain. Hopefully this saves people some trouble.

EasyWinBuilder can run on a mint windows system (e.g. a virtual machine) or a normal system. It might overwrite your MinGW or Qt installation, though. Please note the process takes about half an hour or more and will need about 3GB of hard drive space.

In this repository there are no changes to the Bitcoin code itself though the process makes a handful of small changes. This means it should be possible to plug'n'play the easywinbuilder directory into similar Bitcoin versions.

To change dependency versions edit set_vars.bat

File Hashes
-----------
The process will calculate a hash of the disassemblies of the final executables. The idea is to validate binaries by several people.
I am not quite sure if this will reliably result in the same hash for the same build. Please let me know if you get different values or know how to improve this feature.

ToDo
-----
* Check daemon disassembly hashes (deterministic build)
* Automatic environment install (how to bootstrap?)
* Try to speed up building of OpenSSL and Berkeley DB by only building what is necessary


Credits
-------
Based on build instructions by nitrogenetics (https://bitcointalk.org/index.php?topic=149479.0), Matt Corallo and others
