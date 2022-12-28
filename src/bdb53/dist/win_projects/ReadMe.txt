
# To maintain source files easily, we uses bash + xqilla to generate internal project files for Visual Studio. We don't modify the project file manually. 
# This file includes some instructions about how to generate Berkeley DB internal project files for Visual Studio.


== How to add a new Visual Studio project? ==
1. Add the new project name in dist/win_projects/db.projects
2. Add your project to dist/win_projects/projects.template.xml. Please use the following template:
<project name="your_project_name" guid="your_project_guid">
	-- Add your project type here.
	-- If you want to create a library(.lib or .dll), the project type should be "library".
	-- If you want to create a application(.exe), the project type should be "application".
    <type>library</type>
	-- Add this line if you want to create an application or a dynamic linked library(dll) for this project
    <configuration></configuration>
	-- Add this line if you want to create static linked library for this project, only available when type is library
    <configuration>Static </configuration>
	-- Add your depended library(which will appear to "Additional Library" list) here, if any. 
	-- The depended library should appear in some <library ></library> element in projects.template.xml, otherwise you need to create one.
    <depends>library_name</depends>
	-- Add your project dependencies here, if any. 
	-- This is only available for VS2010 because VS2005 stores project dependency info in solution file.
    <references>depended_project_name</references>    
    <options>
      -- Add project specified configuration here
      <nowp64/>
      <link config="Debug">/FIXED:NO</link>
    </options>
	-- Add your configurations specified preprocess here; Configname should be one of these:
	-- Debug, Release, Static Debug, Static Release, dll(means both Debug and Release), all.
    <preprocessor config="configname1">xxx</preprocessor>
    <preprocessor config="configname2">xxx</preprocessor>
    <files>
	-- Add head files if you need
      <file name="your head_files"/>
	-- Add source files if you need
      <file name="your_source_files"/>
    </files>
</project>
3. cd dist, run ./s_window_dsp to generate the project. You will get two projects: your_project_name.vcproj for VS2005 and your_project_name.vcxproj for vs2010.
4. Add your project file into related solution (Berkeley_DB.sln or Berkeley_DB_examples.sln for VS2005, Berkeley_DB_vs2010.sln or Berkeley_DB_examples_vs2010.sln for VS2010).


== How to add a file to exist project files? ==
1. Open dist/win_projects/projects.template.xml
2. Add the file to <files> element of the project by alphabeta order.
3. If you want to add the file to more than one project, add the file for every project.
4. cd dist, run s_windows_dsp to regenerate the projects.


== How to add user defined compile flag for all projects? ==
1. Open dist/win_projects/projects.template.xml
2. Add the flag to related preprosessor element at the begining of the file:
  <preprocessor config="all">WIN32;_WINDOWS;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE</preprocessor>	This is for all configurations of all projects
  <preprocessor config="Debug">_DEBUG;DIAGNOSTIC</preprocessor>	--This is for Debug and Static Debug configurations of all projects
  <preprocessor config="Release">NDEBUG</preprocessor>	-- This is for Release and Static Release configurations of all projects
  <preprocessor config="dll">_USRDLL</preprocessor>	--This is for Release and Debug configurations of all library projects
  <preprocessor config="app">_CONSOLE</preprocessor>	--This is for Release and Debug configurations of all application projects
  <preprocessor config="static_lib">_LIB;</preprocessor>	This is for Static Debug and Static Release configurations of all library projects
  <preprocessor config="static_app"></preprocessor>	Reserved, no used currently
3. cd dist, run s_windows_dsp to regenerate the projects.


== How to add user defined compile flag for a specified project? ==
1. Open dist/win_projects/projects.template.xml
2. Add the flag to related preprosessor element of specified project
  <preprocessor config="all">xxx</preprocessor>	This is for all configurations of this project
  <preprocessor config="Debug">xxx</preprocessor>	--This is for Debug and Static Debug configurations of this project
  <preprocessor config="Release">xxx</preprocessor>	-- This is for Release and Static Release configurations of all project
3. cd dist, run s_windows_dsp to regenerate the project.

== Notes ==
1. Xqilla is necessary for running s_windows
2. To generate project for Wince platform, you need to modify db_wince.projects and projects_wince.template.xml instead of db.projects and projects.template.xml

