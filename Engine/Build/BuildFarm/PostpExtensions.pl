# These are patterns we want to ignore
@::gExcludePatterns = (
	# UAT spam that just adds noise
	".*Exception in AutomationTool: BUILD FAILED.*",
	"RunUAT ERROR: AutomationTool was unable to run successfully.",

	# Linux error caused by linking against libfbx
	".*the use of `tempnam' is dangerous.*",

	# Warning about object files not definining any public symbols in non-unity builds
	".*warning LNK4221:.*",
	
	# Warning about static libraries missing PDB files for XboxOne
	".*warning LNK4099:.*",

	# Warning from Android monolithic builds about the javac version 
	".*major version 51 is newer than 50.*",
	
	# SignTool errors which we can retry and recover from
	"SignTool Error: ",
	
	# Temporary PS4 deprecation warnings
	".*OnlineSubsystemPS4.*warning:.*\\[-Wdeprecated-declarations\\]",
	".*PS4Application\\.cpp.*warning:.*\\[-Wdeprecated-declarations\\]",
	
	# Missing Steam DLLs/Dylibs when building samples
	"STEAM: Steam API disabled!",
	"LogMac:Warning: dlopen failed:.*libsteam_api.dylib.*: image not found",
	"LogOnline:Warning: STEAM:.*libraries not present.*failed to load!",
	
	# Some doxygen output can confuse the post-processor, because it lists a symbol containing "Warning::"
	"doxygen>.*(Warning|Error)::.*",
	
	# Arxan	
	"Warning:.*Source [cC]ode [mM]arkers will be unavailable",
	"Warning: PDB file .* not found",
	"Warning: guard '[^']*' cannot be installed at specified location",
	"Warning: The object file name .* in the do_not_analyze section was not found",
	"Warning: Code range for .* is not unique, using the first match",
	"Warning: The finalizer is unable to determine what symbol it needs.",

	# Temporary Vivox hacks
	"warning: \\\(arm64\\\) /Users/jenkins/slave-jenkins/workspace/sdk-4.9.2-build-ios-xcode9/.*: No such file or directory",

	# Tempoary IOS packaging hacks
	"warning: no debug symbols in executable \\(-arch arm64\\)",

	# Orbis-pub-cmd
	"\\[Warn\\]\\s*Number of Warning",
	"\\[Warn\\]\\s*Result : WARNING\\s*\$",
	"\\[Warn\\]\\s*[0-9 :-]+ [A-Za-z ]+ Process finished with warning\\(s\\)\\.",
	
	# FN orbis-pub-cmd hacks
    "\\[Warn\\]	\\(online check\\) Could not get the version info from the network\\.",
	"\\[Warn\\]	\\(online check\\) [A-Za-z0-9. ]+ may or may not be used for master submission\\. \\(unconfirmed version\\)",
	"\\[Warn\\]	Format of the project file is not valid\\. \\(Warning\\) \\(Chunk #\\d+ is never downloaded because no languages are assigned to it\\)",
	"\\[Warn\\]	The following combination of Volume Type/Storage Type is exceptionally allowed\\. \\(app:bd25, remaster:bd25, remaster:bd25\\)",
    "\\[Warn\\]	Number of Initial Chunks for Disc was changed by the patch/remaster\\. \\(Scenario #0, 0 -> 1\\) Confirm that it is intentional change\\.",
	"\\[Warn\\]	Language Setting was changed by the patch/remaster\\. \\(Chunk #\\d+, 0x\\d+ -> 0x\\d+\\) Confirm that it is intentional change\\.",
	
	# PVS Studio
	"warning Renew: Your license will expire in.*viva64\\.com",	
	
#	".*ERROR: The process.*not found",
#	".*ERROR: This operation returned because the timeout period expired.*",
#	".*Sync.VerifyKnownFileInManifest: ERROR:.*",
#	".*to reconcile.*",
#	".*Fatal Error: Failed to initiate build.*",
#	".*Error: 0.*",
#	".*build system error(s).*",
#	".*VerifyFilesExist.*",
#	".* Error: (.+) replacing existing signature",
#	".*0 error.*",
#	".*ERROR: Unable to load manifest file.*",
#	".*Unable to delete junk file.*",
#	".*SourceControl.SyncInternal: ERROR:.*",
#	".*Error: ACDB.*",
#	".*xgConsole: BuildSystem failed to start with error code 217.*",
#	".*error: Unhandled error code.*",
#	".*621InnerException.*",
#	".* GUBP.PrintDetailedChanges:.*",
#	".* Failed to produce item:.*",
#	"Cannot find project '.+' in branch",
#	".*==== Writing to template target file.*",
#	".*==== Writing to OBB data file.*",
#	".*==== Writing to shim file.*",
#	".*==== Template target file up to date so not writing.*",
#	".*==== Shim data file up to date so not writing.*",
#	".*Initialzation failed while creating the TSF thread manager",
#	".*SignTool Error: The specified timestamp server either could not be reached or.*",
#	".*SignTool Error: An error occurred while attempting to sign:.*",
); 

# TCL rules match any lines beginning with '====', which are output by Android toolchain.
$::gDontCheck .= "," if $::gDontCheck;
$::gDontCheck .= "tclTestFail,tclClusterTestTimeout";

# We don't compile clean for Android, so ignore javac notices for now
$::gDontCheck .= ",javacNote";

# This matcher hits anything with "...." in it, which is often used for progress bars
$::gDontCheck .= ",cppunitExtraOutput,cppunitFail";

# Moves back by a line if it matches the given pattern
sub backIf($;$)
{
	my ($pattern, $offset) = @_;
	$offset = 0 unless defined($offset);

	my $line = logLine($::gCurrentLine + $offset - 1);
	if($line =~ m/$pattern/)
	{
		return $offset - 1;
	}
	else
	{
		return $offset;
	}
}

my $clang_file_line_pattern = 
	'('							.
		'(?:[a-zA-Z]:)?'		.	# optional drive letter
		'[^:]+'					.	# any non-colon character
	')'							.
	'(?:'						.
		'\s*:[\s\d:,]+:'		.	# clang-style   :123:456:
		'|'						.
		'\([\s\d:,]+\):'		. 	# msvc-style    (123,456):
	')'							;
	
# These are patterns we want to process
# NOTE: order is important because the file is processed line by line 
# After an error is processed on a line that line is considered processed
# and no other errors will be found for that line, so keep the generic
# matchers toward the bottom so the specific matchers can be processed.
# NOTE: also be aware that the look ahead lines are considered processed

unshift @::gMatchers, (
	{
		id => "UATErrStack",
		pattern => q{begin: stack for UAT},
		action => q{incValue("errors"); my $line_count = forwardTo(q{end: stack for UAT}); my $last_line = $::gCurrentLine + $line_count; diagnostic("AutomationTool", "error", 1, $line_count - 1); $::gCurrentLine = $last_line; }
	},
    {
        id =>               "clErrorMultiline",
        pattern =>          q{([^(]+)(\([\d,]+\))? ?: (fatal )?error [a-zA-Z]+[\d]+},
        action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("errors"); my ($file_only) = ($1 =~ /([^\\\\]+)$/); diagnostic($file_only || $1, "error", 0, forwardWhile("^$indent "))},
    },
    {
        id =>               "clWarningMultiline",
        pattern =>          q{([^(]+)(\([\d,]+\))? ?: warning[ :]},
        action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("warnings"); my ($file_only) = ($1 =~ /([^\\\\]+)$/); diagnostic($file_only || $1, "warning", 0, forwardWhile("^$indent "))},
    },
    {
        id =>               "clangError",
        pattern =>          q{\s*}.$clang_file_line_pattern.q{\s*error\s*:},
		action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("errors"); diagnostic("", "error", backWhile("^(?:$indent).*(?:In (member )?function|In file included from)"), forwardWhile("^($indent |$indent".'}.$clang_file_line_pattern.q{\s*note:| *\$)'))},
	},
    {
        id =>               "clangWarning",
        pattern =>          q{\s*}.$clang_file_line_pattern.q{\s*warning\s*:},
		action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("warnings"); diagnostic("", "warning", backWhile("^(?:$indent).*(?:In (member )?function|In file included from)"), forwardWhile("^($indent |$indent".'}.$clang_file_line_pattern.q{\s*note:| *\$)'))},
	},
    {
        id =>               "ubtFailedToProduceItem",
        pattern =>          q{(ERROR: )?UBT ERROR: Failed to produce item: },
		action =>           q{incValue("errors"); diagnostic("UnrealBuildTool", "error")}
    },
    {
        id =>               "changesSinceLastSucceeded",
        pattern =>          q{\*\*\*\* Changes since last succeeded},
		action =>           q{$::gCurrentLine += forwardTo('^\\\*', 1);}
    },
	{
		id =>				"clangThreadSanitizerWarning",
		pattern =>			q{^(\s*)WARNING: ThreadSanitizer:},
        action =>           q{incValue("warnings"); my $prefix = $1; diagnostic("TSan", "warning", 0, forwardWhile("^([ ]*|$prefix  .*|${prefix}SUMMARY:.*)\$"))}
	},
	{
		id =>               "editorLogChannelError",
		pattern =>          q{^([a-zA-Z_][a-zA-Z0-9_]*):Error: },
		action =>           q{incValue("errors"); diagnostic($1, "error")}
	},
	{
		id =>               "editorLogChannelWarning",
		pattern =>          q{^([a-zA-Z_][a-zA-Z0-9_]*):Warning: },
		action =>           q{incValue("warnings"); diagnostic($1, "warning")}
	},
	{
		id =>               "editorLogChannelDisplay",
		pattern =>          q{^([a-zA-Z_][a-zA-Z0-9_]*):Display: },
		action =>           q{ }
	},
	{
		id =>               "automationException",
		pattern =>          q{AutomationTool\\.AutomationException: },
		action =>           q{incValue("errors"); diagnostic("Exception", "error", 0, forwardWhile("^  at "));}
	},
	{
		id =>				"arxanMissingSymbol",
		pattern =>			q{^\s*Lookup of.*returns:\s*$},
		action =>			q{ if(logLine($::gCurrentLine + 1) =~ /^\s*\(empty\)\s*$/) { incValue("errors"); diagnostic("GuardIT", "error", 0, 1); } }
	},
	{
		id =>				"ubtFatal",
		pattern =>			q{^FATAL:},
		action =>			q{incValue("errors"); diagnostic("AutomationTool", "error")}
	},
    {
        id =>               "genericError",
        pattern =>          q{(?<!\w)(ERROR|[Ee]rror)}.q{( (\([^)]+\)|\[[^\]]+\]))?: },
        action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("errors"); diagnostic("", "error", 0, forwardWhile("^($indent | *\$)"))},
    },
    {
        id =>               "genericWarning",
        pattern =>          q{(?<!\w)WARNING|[Ww]arning}.q{( (\([^)]+\)|\[[^\]]+\]))?: },
        action =>           q{my $line = logLine($::gCurrentLine); $line =~ /^( *)/; my $indent = ' ' x length($1); incValue("warnings"); diagnostic("", "warning", 0, forwardWhile("^($indent | *\$)"))},
    },
	{
		id =>				"genericMsError",
        pattern =>          q{[Ee]rror [A-Z]\d+\s:},
		action =>           q{incValue("errors"); diagnostic("", "error", 0)},
	},
	{
		id =>				"orbisPubCmdWarning",
        pattern =>          q{\[[Ww]arn\]},
		action =>           q{incValue("warnings"); diagnostic("", "warning", 0, forwardWhile('^\\s*\\[[Ww]arn\\]')) },
	},
	{
		id =>				"orbisPubCmdError",
        pattern =>          q{\[[Ee]rror\]},
		action =>           q{incValue("errors"); diagnostic("", "error", 0, forwardWhile('^\\s*\\[[Ee]rror\\]')) },
	},
	{
		id =>				"EacWarning",
        pattern =>          q{\[[Ww]arning\]},
		action =>           q{incValue("warnings"); diagnostic("", "warning", 0, forwardWhile('^\\s*\\[[Ww]arning\\]')) },
	},
	{
		id =>				"monoInternalError",
		pattern =>			q{\* Assertion at wthreads.c:},
		action =>           q{incValue("errors"); diagnostic("", "error", 0)},
	},
	{
		id =>				"UnhandledException",
        pattern =>          q{^\s*Unhandled Exception: },
		action =>           q{incValue("errors"); diagnostic("", "error", 0, forwardWhile('^\\s*at')) },
	}
);

push @::gMatchers, (
	{
		id => "AutomationToolException",
		pattern => q{AutomationTool terminated with exception:},
		action => q{incValue("errors"); diagnostic("Exception", "error", 0,  forwardTo(q{AutomationTool exiting with ExitCode=}));}
	},	
	{
		id => "symbols",
		pattern => q{Undefined symbols for architecture},
		action => q{incValue("errors"); diagnostic("Symbols", "error", 0, forwardTo(q{ld: symbol}));}
	},
	{
		id => "LogError",
		pattern =>q{LogWindows: === Critical error: ===},
		action => q{incValue("errors"); diagnostic("Critical Error", "error", 0, forwardTo(q{Executing StaticShutdownAfterError}));}
	},
	{
		id => "UATException",
		pattern => q{ERROR: Exception in AutomationTool:},
		action => q{incValue("errors"); diagnostic("Exception", "error", 0, 0);}
	},
	{
		id => "LNK",
		pattern => q{LINK : fatal error},
		action => q{incValue("errors"); diagnostic("Link", "error", 0, 0);}
	},
	{
		id => "MSBuildError",
		pattern => q{targets\(\d+,\d+\): error},
		action => q{incValue("errors"); diagnostic("MSBuild", "error", -2, 0);}
	},
	{
		id => "UnrealBuildToolException",
		pattern => q{UnrealBuildTool Exception:},
		action => q{incValue("errors"); diagnostic("UnrealBuildTool", "error", 0, 5);}
	},
	{
		id => "ExitCode3",
		pattern => q{ExitCode=3},
		action => q{incValue("errors"); diagnostic("ExitCode", "error", 0, 0);}
	},
	{
		id => "SegmentationFault",
		pattern => q{ExitCode=139},
		action => q{incValue("errors"); diagnostic("Exit code 139", "error", -4, 1);}
	},
	{
		id => "ExitCode255",
		pattern => q{ExitCode=255},
		action => q{incValue("errors"); diagnostic("Exit code 255", "error", 0, 0);}
	},
	{
		id => "Stack dump",
		pattern => q{Stack dump},
		action => q{incValue("errors"); diagnostic("Stack dump", "error", -4, forwardWhile('0', 1));}
	},
	{
		id => "UATErrStack",
		pattern => q{AutomationTool: Stack:},
		action => q{incValue("errors"); diagnostic("AutomationTool", "error", 0, forwardTo(q{AutomationTool: \n}));}
	},
	{
		id => "BuildFailed",
		pattern => q{BuildCommand.Execute: ERROR:},
		action => q{incValue("errors"); diagnostic("Build Failed", "error", 0, 0);}
	},
	{
		id => "StraightFailed",
		pattern => q{BUILD FAILED},
		action => q{incValue("errors");}
	}
);

1;
