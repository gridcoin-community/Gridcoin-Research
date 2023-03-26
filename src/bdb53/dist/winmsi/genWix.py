#
#
# genWix.py is used to generate a WiX .wxs format file that
# can be compiled by the candle.exe WiX compiler.
#
# Usage: python genWix.py <output_file>
#
# The current directory is expected to be the top of a tree
# of built programs, libraries, documentation and files.
# 
# The list of directories traversed is at the bottom of this script,
# in "main."  Extra directories that do not exist are fine and will
# be ignored.  That makes the script a bit more general-purpose.
#
# "Excluded" directories/files are listed below in the GenWix class
# constructor in the excludes variable.  These will *not* be included 
# in packaging.
#
# The output file is expected to be post-processed using XQuery Update
# to add ComponentGroup elements for the various WiX Feature elements.
#
# The generated output for each directory traversed will look like:
# <Directory Id="dir_dirname_N" Name="dirname">
#    <Component DiskId="1" Guid="..." Id="some_id" KeyPath="yes">...
#        <File Id="..." Name="..." Source="pathtofile"/>
#        <File.../>
#    </Component>
# </Directory>
#
# Subdirectories are new elements under each top-level Directory element
#
# NOTE: at this time each top-level directory is its own Component. This
# mechanism does NOT generate multiple Components in a single Directory.
# That should be done as an enhancement to allow, for example, the "bin"
# directory to contain files that are part of multiple Components such
# as "runtime" "java" "sql" etc.
# WiX will do this but this script plus the generateGroups.xq XQuery script
# cannot (yet).  Doing that will be a bit of work as well as creating
# additional lists of files that indicate their respective Components.
# 

import sys
import os

class GenWix:
    def __init__(self, sourcePfx, outfile, dbg):
	self.debugOn = dbg
	self.componentId = 0
	self.indentLevel = 0
	self.indentIncr = 2
        self.shortId = 0
	self.fragName="all"
	self.refDirectory = "INSTALLDIR"
	self.compPrefix = ""
	self.dirPrefix = "dir"
	self.sourcePrefix = os.path.normpath(sourcePfx)
        # use excludes to exclude paths, e.g. add files to the array:
        # ...os.path.normpath("dbxml/test"), os.path.normpath("a/b/c")...
	self.excludes = []
	self.groups = ["group_csharp", "group_cxx", "group_devo", "group_doc", "group_examples", "group_java", "group_runtime", "group_sql"]
	self.groupfiles = ["group.csharp", "group.cxx", "group.devo", "group.doc", "group.examples", "group.java", "group.runtime", "group.sql"]
	self.groupcontent = ["","","","","","","",""]
	self.outputFile = outfile
	self.out = open(self.outputFile, "ab")
	self.out.truncate(0)
	self.initGroupFiles()

    def __del__(self):
	self.out.close()

    def initGroupFiles(self):
	idx = 0
	for file in self.groupfiles:
	    f = open(file, 'r')
	    self.groupcontent[idx] = os.path.normpath(f.read())
	    f.close()
	    idx = idx + 1

    def checkExclude(self, fname):
	for ex in self.excludes:
	    if fname.find(ex) != -1:
		return True
        return False

    # NOTE: this will count leading/trailing '/'
    def count(self, path):
	return len(path.split("/"))

    def nextId(self):
	self.componentId = self.componentId + 1

    def printComponentId(self, fragname):
	return self.makeId("%s_%s_%d"%(self.compPrefix,fragname,self.componentId))

    def printDirectoryId(self,dirname):
	return self.makeId("%s_%s_%d"%(self.dirPrefix,dirname,self.componentId))

    def indent(self, arg):
	if arg == "-" and self.indentLevel != 0:
	    self.indentLevel = self.indentLevel - self.indentIncr
	i = 0
	while i != self.indentLevel:
	    self.out.write(" ")
	    i = i+1
	if arg == "+":
	    self.indentLevel = self.indentLevel + self.indentIncr

    def echo(self, arg, indentArg):
	self.indent(indentArg)
	#sys.stdout.write(arg+"\n")
	self.out.write(arg+"\n")

    def generateGuid(self):
	if sys.version_info[1] < 5:
	    return "REPLACE_WITH_GUID"
	else:
	    import uuid
	    return uuid.uuid1()

    # used by makeShortName                                                            
    def cleanName(self, name):
	for c in ("-","%","@","!"):
	    name = name.replace(c,"")
	return name

    def makeId(self, id):
        tid = id.replace("-","_")
	if len(tid) > 70:
	    #print "chopping string %s"%tid
	    tid = tid[len(tid)-70:len(tid)]
	    # id can't start with a number...
	    i = 0
	    while 1:
		try:
		    int(tid[i])
		except:
		    break
		i = i+1
	    return tid[i:len(tid)]
	return tid	

    # turn names into Windows 8.3 names.
    # A semi-unique "ID" is inserted, using 3 bytes of hex,
    # which gives us a total of 4096 "unique" IDs.  If
    # that number is exceeded in one class instance, a bad
    # name is returned, which will eventually cause a 
    # recognizable failure.  Names look like: ABCD~NNN.EXT
    # E.g. NAMEISLONG.EXTLONG => NAME~123.EXT
    #
    def makeShortName(self, longName):
	name = longName.upper()
        try:
            index = name.find(".")
        except ValueError:
            index = -1
        
        if index == -1:
            if len(name) <= 8:
                return longName
            after = ""
        else:
	    if index <= 8 and (len(name) - index) <= 4:
	        return longName
            after = "." + name[index+1:index+4]
            after = self.cleanName(after)
            
	self.shortId = self.shortId + 1
	if self.shortId >= 4096:   # check for overflow of ID space
	    return "too_many_ids.bad" # will cause a failure...
	hid = hex(self.shortId)
	name = self.cleanName(name) # remove stray chars
	# first 5 chars + ~ + Id + . + extension
	return name[0:4]+"~"+str(hid)[2:5]+after

    def makeFullPath(self, fname, root):
	return os.path.join(self.sourcePrefix,os.path.join(root,fname))

    def makeNames(self, fname):
	return "Name=\'%s\'"%fname
        #shortName = self.makeShortName(fname)
        #if shortName != fname:
        #    longName="LongName=\'%s\'"%fname
        #else:
        #    longName=""
        #return "Name=\'%s\' %s"%(shortName,longName)

    def generateFile(self, fname, root, dirId):
	# allow exclusion of individual files
	if self.checkExclude(os.path.join(root,fname)):
	    self.debug("excluding %s\n"%os.path.join(root,fname))
	    return
        idname = self.makeId("%s_%s"%(dirId,fname))
	elem ="<File Id=\'%s\' %s Source=\'%s\' />"%(idname,self.makeNames(fname),self.makeFullPath(fname, root))
	self.echo(elem,"")
	
    def startDirectory(self, dir, parent):
	# use parent dirname as part of name for more uniqueness
	self.debug("Starting dir %s"%dir)
	self.nextId()
        dirId = self.printDirectoryId(dir)
        elem ="<Directory Id=\'%s\' %s>"%(dirId,self.makeNames(dir))
	self.echo(elem,"+")
        return dirId
	
    def endDirectory(self, dir):
	self.debug("Ending dir %s"%dir)
	self.echo("</Directory>","-")

    def startComponent(self, dir, group):
	self.debug("Starting Component for dir %s, group %s"%(dir,group))
	# Use the group name in the component id so it can be used later
	celem ="<Component Id=\'%s\' DiskId='1' KeyPath='yes' Guid=\'%s\'>"%(self.printComponentId(group),self.generateGuid())
	self.echo(celem,"+")

    def endComponent(self, dir, group):
	self.debug("Ending Component for dir %s, group %s"%(dir,group))
	self.echo("</Component>","-")

    def generatePreamble(self):
	# leave off the XML decl and Wix default namespace -- candle.exe
	# doesn't seem to care and it makes updating simpler
	self.echo("<Wix>","+")
	self.echo("<Fragment>","+")
	self.echo("<DirectoryRef Id='%s'>"%self.refDirectory,"+")

    def generateClose(self):
	self.echo("</DirectoryRef>","-")
	self.echo("</Fragment>","-")
	self.echo("</Wix>","-")

    def debug(self, msg):
	if self.debugOn:
	    sys.stdout.write(msg+"\n")

    def generateDir(self, dir, path):
	fullPath = os.path.join(path,dir)
	if self.checkExclude(fullPath):
	    self.debug("excluding %s\n"%fullPath)
	    return
	# ignore top-level directories that are missing, or other
	# errors (e.g. regular file)
	try:
	    files = os.listdir(fullPath)
	except:
	    return
	# check for empty dir (this won't detect directories that contain
	# only empty directories -- just don't do that...)
	if len(files) == 0:
	    self.debug("skipping empty dir %s"%dir)
	    return

	dirId = self.startDirectory(dir, os.path.basename(path))

	# generate a component for each possible group.  Most of these
	# will be empty but that is OK.  Components will have Id's that
	# indicate their group.  This is used by the XQuery script
	# that creates the ComponentGroup elements and references.
        # Post-processing of this is necessary to remove empty
        # Component elements or empty directories will be installed.
        # pruneComponents.xq is used for this.
	idx = 0
	for group in self.groups:
	    self.startComponent(dir, group)
	    # process regular files before directories
	    fileList = [f for f in files if os.path.isfile(os.path.join(fullPath,f))]
	    for file in fileList:
                fullFile = os.path.join(fullPath,file)
		#self.debug("looking for file %s"%fullFile)
		found = self.groupcontent[idx].find(fullFile)
                if found >= 0:
                    self.debug("found %s"%file)
                    #last = self.groupcontext[idx+len(file)]
                    #if last != "\n":
                    #    continue
                    self.generateFile(file,fullPath, dirId)

            # Component element must end before subdirectories start
	    self.endComponent(dir, group)
	    idx = idx + 1

	# now directories
	dirList = [d for d in files if os.path.isdir(os.path.join(fullPath,d))]
	for directory in dirList:
		self.generateDir(directory, fullPath)
	self.endDirectory(dir)

    def generateRequiredFiles(self):
        # LICENSE.txt, README.txt
       	celem ="<Component Id='license_readme' DiskId='1' KeyPath='yes' Guid=\'%s\'>"%self.generateGuid()
	self.echo(celem,"+")
	elem ="<File Id='LICENSE.txt' Name='LICENSE.txt' Source=\'%s\' />"%self.makeFullPath("LICENSE", "")
	self.echo(elem,"")
	elem ="<File Id='README.txt' Name='README.txt' Source=\'%s\' />"%self.makeFullPath("README", "")
	self.echo(elem,"")
        self.echo("</Component>","-")
           
    def generate(self, directories):
	self.generatePreamble()
        self.generateRequiredFiles()
	for dir in directories:
	    self.generateDir(dir, "")
	self.generateClose()
#
# Main script
#
if __name__ == "__main__":
    outfile = sys.argv[-1]
    if outfile == sys.argv[0]:
	print "Usage: genWix.py <output_file>"
	sys.exit()

    print "Generating into file: " + outfile
    gw = GenWix(os.path.realpath("."),outfile,False)
    # extra directory names here that don't exist are fine and make it easier to
    # share this script across products
    gw.generate(["bin","lib","include","jar","docs", "examples", "src", "build_windows", "clib", "perl","python","php"])   
