(: 
*
* Usage: xqilla -u -v "inFile" <input_file> <this_file_name>
*
* XQilla will update the file ($inFile) in-place, so it should
* use a copy if the original is to be saved.
*
* This XQuery script creates WiX ComponentGroup elements inside a
* generated XML file ($inFile). The groups are defined locally
* but need to be maintained along with where they are used
* elsewhere.
*
:)

(: preamble -- shut up revaliation and declare input files :)
declare revalidation skip;
declare variable $inFile as xs:untypedAtomic external;

declare function local:getFrag()
{
	doc($inFile)/Wix/Fragment
};

(: look for Component elements that contain the group name :)
declare function local:getComponents($group)
{
    for $comp in doc($inFile)//Component 
        where contains($comp/@Id,$group)
	    return $comp
};

declare function local:getGroups()
{
    ("group_csharp", "group_cxx", "group_devo", "group_doc", "group_examples", "group_java", "group_runtime", "group_sql")
};

declare function local:createGroup($group)
{
	let $comp := local:getComponents($group)
	return if (not(empty($comp))) then
	       <ComponentGroup Id="{$group}">
	   	  {for $c in $comp return
	           <ComponentRef Id="{$c/@Id}"/>}
	       </ComponentGroup>
	else ()
};

(: the main query :)
for $group in local:getGroups()
        return insert node local:createGroup($group) as
	first into local:getFrag()
