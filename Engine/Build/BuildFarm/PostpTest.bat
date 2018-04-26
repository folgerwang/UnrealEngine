@echo off
if [%1]==[] goto invalid_args

if [%2]==[-SkipCopy] goto skip_copy
ec-perl Main.pl CopyLog --ec-jobstep=%1 --to-file=PostpTest.txt
:skip_copy

if not exist diag-%1.xml goto skip_delete
del diag-%1.xml
:skip_delete

ec-perl PostpFilter.pl < PostpTest.txt | postp --load=./PostpExtensions.pl --jobStepId=%1
goto :eof

:invalid_args
echo Syntax: PostpTest [JobStepId] [-SkipCopy]
