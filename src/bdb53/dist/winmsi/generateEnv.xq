(: 
*
* Usage: xqilla -u -v "inFile" <input_file> -v "envFile" <group_file> <this_file_name>
*
* XQilla will update the file ($inFile) in-place, so it should
* use a copy if the original is to be saved.
*
* This XQuery script creates WiX Environment elements and inserts them
* into the $inFile as directed by content in $envFile.
*
:)

(: preamble -- shut up revaliation and declare input files :)
declare revalidation skip;
declare variable $inFile as xs:untypedAtomic external;
declare variable $envFile as xs:untypedAtomic external;

declare function local:getEnv()
{
    doc($envFile)/envVars/env
};

declare updating function local:createEnv($env)
{
    for $comp in doc($inFile)//Directory[@Name=$env/@name]/Component
        for $evar in $env/Environment
            where contains($comp/@Id,$env/@group)
                return insert node $evar as first into $comp
};

(: the main query :)
for $env in local:getEnv()
    return local:createEnv($env)


