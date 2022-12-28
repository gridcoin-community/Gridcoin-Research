(: 
*
* Usage: xqilla -u -v "inFile" <input_file> <this_file_name>
*
* XQilla will update the file ($inFile) in-place, so it should
* use a copy if the original is to be saved.
*
* This XQuery script removes <ProjectReference> elements that reference
* db.vcxproj, db_stl.vcxproj and db_sql.vcxproj because those projects do 
* not exist in the binary bundle
*
* Variables:
*  inFile -- input file
*
:)

(: preamble -- shut up revaliation and declare input files :)
declare default element namespace "http://schemas.microsoft.com/developer/msbuild/2003";
declare revalidation skip;
declare variable $inFile as xs:untypedAtomic external;

(: the main query :)

for $i in doc($inFile)//ItemGroup/ProjectReference[@Include="db.vcxproj" or
@Include="db_stl.vcxproj" or @Include="db_sql.vcxproj"] return delete node $i



