(: 
*
* Usage: xqilla -u -v "inFile" <input_file> -v "libname" <libname> 
*                  -v "libversion" <libversion> <this_file_name>
*
* XQilla will update the file ($inFile) in-place, so it should
* use a copy if the original is to be saved.
*
* This XQuery script replaces the <ProjectReference> elements within
* a .csproj project file with simple <Reference> elements to libdb_dotnetXX.
*
* Variables:
*  inFile -- input file
*  libname -- BDB dotnet library name, e.g. libdb_dotnet50
*  libversion -- BDB dotnet library version, e.g. 5.0.11.0
*
:)

(: preamble -- shut up revaliation and declare input files :)
declare default element namespace "http://schemas.microsoft.com/developer/msbuild/2003";
declare revalidation skip;
declare variable $inFile as xs:untypedAtomic external;
declare variable $libname as xs:untypedAtomic external;
declare variable $libversion as xs:untypedAtomic external;

declare function local:getRefs()
{
	doc($inFile)//ItemGroup/ProjectReference[Name="db_dotnet"]
};

(: the main query :)

let $ref := local:getRefs()
return replace node $ref with <Reference Include="{$libname}, Version={$libversion}, Culture=neutral, processorArchitecture=MSIL"><SpecificVersion>False</SpecificVersion><HintPath>..\..\..\bin\{$libname}.dll</HintPath></Reference>



