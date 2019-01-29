---
layout: default
title: "Troubleshooting building UE4 HTML5"
---

* * *
# Troubleshooting UE4 HTML5 builds

the following are tips and suggestions to try out when your HTML5 builds goes wrong.


* * *
## attempting to package for HTML5 instead brings up the help page

if you are seeing the [Developing HTML5 project](https://docs.unrealengine.com/en-us/Platforms/HTML5/GettingStarted)
help page after clicking on **Menu Bar -> File -> Package Project -> HTML5**, chances are
that there is a mismatched "emscripten toolchain version" value(s).  this is normally seen
when developers try to upgrade their emscripten toolchain and forget to update those values
HTML5SDKInfo.cs.  please see
(Emscripten and UE4: Part 2 - UE4 C# scripts)[README.2.emscripten.and.UE4.md#part-2---ue4-c-scripts]
for more details on what to check for and how to fix this.

note: do not forget to recompile all ThirdParty libraries (see Part 1 of the same link above)
with your changed toolchain versions.


* * *
## 


* * *
