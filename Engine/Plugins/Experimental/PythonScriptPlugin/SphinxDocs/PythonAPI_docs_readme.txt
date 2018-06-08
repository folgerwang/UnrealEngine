Unreal Python API HTML generation using Sphinx
===========================================================

Ensure Python is installed
---------------------------------------

Download from https://www.python.org/downloads/

[Python 2.7.14 was used for testing and to make this guide since it matches the current needs of enterprise studios.]

You *may* need to set Python to use UTF-8 (if you have a code page set with "chcp 65001" for example). You can set it interactively on the command line:
set PYTHONIOENCODING=UTF-8

Or set it permanently in the environment variables.


Install Sphinx
---------------------------------------

[Should only need to do this step once and then it is set up for repeated builds.]

Install Sphinx using command prompt:
pip install -U sphinx

Confirm installation and version:
sphinx-build --version

[sphinx-build 1.7.2 was used at the time of writing.]


Ensure Sphinx template files are in place
---------------------------------------

[Should only need to do this step once and then it is set up for repeated builds.]

Ensure the PythonScriptPlugin is synced from Perforce.

The Sphinx template files used to for the doc generation are located here:
Engine\Plugins\Experimental\PythonScriptPlugin\SphinxDocs

The two main template files are in SphinxDocs\source\_templates and they are used by the PythonScriptPlugin when creating files used by Sphinx:

index.rst
{{SectionsTOC}} is replaced with several tables of contents for all the Unreal Python classes and placed at SphinxDocs\source\index.rst

Class.rst
{{Class}} is replaced with the name of each specific Unreal Python class and each separate file is named [ClassName].rst and placed in SphinxDocs\source\class\

You can add additional .rst files for additional documentation, though be sure to reference them in the SphinxDocs\source\_templates\index.rst file table of contents.


You can find out more about Sphinx at http://www.sphinx-doc.org/en/master/

Most of the template files are written in reStructuredText format.

Here is a cheat sheet on Sphinx and reStructuredText - https://thomas-cokelaer.info/tutorials/sphinx/rest_syntax.html

You can add reStructuredText syntax highlighting to Notepad++:
https://github.com/steenhulthin/reStructuredText_NPP


Generate Sphinx files from UE4 Editor
---------------------------------------

[This step needs to be done for each refresh of the Unreal Python API whenever reflected engine bindings are changed.]

Build an up-to-date version of the UE4 editor including the PythonScriptPlugin.

Run the UE4 editor and ensure the PythonScriptPlugin is enabled. Reload the UE4 editor if necessary.

If there are any delayed dynamically loaded modules that you want to include for the Python API docs then take whatever steps necessary to ensure that they are loaded.

Go to the settings dialog for the Python plugin and enable both "Developer Mode" and "Generate Online Docs".

You will see "Restart required to apply new settings" so select "Restart Now".

In the future, the generation process now will occur (after a small delay) whenever a module is loaded. This happens at the load of any project and at other points when a new module is needed such as when a project is initially loaded.

The stubs for the Python API will be generated first and then all the files needed for Sphinx to generate HTML files. They will be placed in:
Engine\Plugins\Experimental\PythonScriptPlugin\SphinxDocs


Generate Python API HTML using Sphinx
---------------------------------------

[This step needs to be done for each refresh of the Unreal Python API whenever reflected engine bindings are changed.]

Using the command prompt (note: you must use the regular command prompt and not the Windows Powershell which may display colored text as invisible) go to the Python SphinxDocs folder:
Engine\Plugins\Experimental\PythonScriptPlugin\SphinxDocs

Run Sphinx to build the static Python API HTML:
sphinx-build -b html source/ build/

All the HTML files will be output to the directory:
Engine\Plugins\Experimental\PythonScriptPlugin\SphinxDocs\build

Sphinx looks at the docstrings in the unreal.py stub file during generation looking for reStructuredText markup. Many of the docstrings have things that Sphinx *thinks* are markup, though they are usually badly formed markup since people didn't know of the reStructuredText conventions when they wrote them. Such as: indentations without line gaps before and after, identifiers that end in an underscore_, and many other things. As of writing this, there are 159 warnings during the Sphinx generation process. Most can be safely ignored since they are accidental markup in the docstrings. None of them currently prevent the HTML generation.


Use the Python API HTML
---------------------------------------

[This step needs to be done for each refresh of the Unreal Python API whenever reflected engine bindings are changed.]

The files located in PythonScriptPlugin\SphinxDocs\build are relative and can be used without a web server. Just drag the index.htm file to your browser of choice. Even the search works locally via a generated JavaScript search index file.

The files can be zipped up and distributed to be used locally or placed on a web server.

The objects.inv and the associated .doctrees folder is used by other Python API web sites for linking to this site. It is only needed when placed on an online web server and not needed when just using the files locally.

Enjoy!
