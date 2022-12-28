# Perl script to generate a DTrace provider specification from dist/events.in
$class = undef;
$continuing = 0;
$header_comment = 1;
$single_line = undef;
$multi_line = undef;
print "#include \"dbdefs.d\"\n\nprovider bdb {\n";

while (<>) {
	$lineno++;
	if (/^$/) {
		# Always put the (required multi-line) header comment
		# by itself. It ends at the first empty line.
		if ($header_comment && defined($multi_line)) {
			printf("%s*/\n", $multi_line);
			$multi_line = undef;
		}
		printf("\n");
		next;
	}
	chop;

	# Translate single-line # comments to DTrace compatible /* ... */.
	# Generate both single and multi-line version to match KNF standards.
	if (/^([ ]*)#([ ]*)(.*)$/) {
		if (defined($multi_line)) {
			$single_line = undef;
			$multi_line = "$multi_line* $3\n$1 ";
		} else {
			$single_line = "$1/* $3 */";
			$multi_line = "$1/*\n$1 * $3\n$1 ";
		}
		next;
	}
	# It is not a comment line, see whether to output any pending comment
	# that was saved up over the previous line(s).
	if (defined($multi_line)) {
		if (defined($single_line)) {
			printf("%s\n", $single_line);
		} else {
			printf("%s*/\n", $multi_line);
		}
		$single_line = undef;
		$multi_line = undef;
	}
	# A line starting with a letter is an event class name.
	if (/^[a-z]/) {
		$class = $_;
		next;
	}
	if ($continuing) {
	    # End of a continued probe signature?
	    if (/([a-z0-9-_ ,)]*;)$/) {
		printf("%s\n", $_);
	    	$continuing = 0;
		next;
	    } elsif (/([a-z0-9-_ ,]*,)$/) {
		printf("%s\n", $_);
		next;
	    }
	}
	if (/([ 	]*)([a-z-_]*)[	 ]*(\([^)]*,)$/) {
		# printf("\tprobe %s__%s%s\n", $class, $1, $2);
		printf("%sprobe %s__%s%s\n", $1, $class, $2, $3);
		$continuing = 1;
	} elsif (/([ 	]*)([a-z-_]*)[	 ]*(\([^)]*\);)/) {
		printf("%sprobe %s__%s%s\n", $1, $class, $2, $3);
		# printf("\tprobe %s__%s%s\n", $class, $1, $2);
	} else {
	    printf("** Error in line %d: %s\n", $lineno, $_);
	    printf("** Missing or unrecognized parameter list under class %s\n", $class);
	    exit(1);
	}
}
print "};\n\n";
if ($continuing) {
	printf("** Unfinished probe under class %s\n", $class);
	exit(1);
}
	

print "#pragma D attributes Evolving/Evolving/Common provider bdb provider\n";
print "#pragma D attributes Private/Private/Common provider bdb module\n";
print "#pragma D attributes Private/Private/Common provider bdb function\n";
print "#pragma D attributes Evolving/Evolving/Common provider bdb name\n";
print "#pragma D attributes Evolving/Evolving/Common provider bdb args\n\n";
