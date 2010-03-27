Overview
========

This is a modified SciTE editor, which includes the following:

* [Scintilla][] and [SciTE][] version 2.03 - the original SciTE editor and Scintilla text editor control.
* [Scintillua][] by Mitchell Foral - modification of Scintilla to use [LPEG][] based parser.
* [SciTE-tools][] by Mitchell Foral - useful text-editing utilities and modifications to SciTE.
* [SciteDebug][] by Steve Donovan - allows to debug [Lua][] and C/C++ programs in SciTE
* [Incremental Autocomplete patch][incautcomp] by Sergey Kishchenko - autocomplete shows up without while editing the text without the need to press any special key.

The editor is built using the latest version of these projects by Mar 27, 2010.

I got tired of assembling it separately on every computer I use, so I put them together
in one project.

The latest version can be found at [the repository](http://github.com/mkottman/scite).
	
[scintilla]: http://www.scintilla.org/						"Scintilla"
[scite]: http://www.scintilla.org/SciTE.html				"SciTE"
[scintillua]: http://code.google.com/p/scintillua/			"Scintillua"
[lpeg]: http://www.inf.puc-rio.br/~roberto/lpeg/lpeg.html	"LPEG"
[scite-tools]: http://caladbolg.net/scite.php				"SciTE-tools"
[scitedebug]: http://scitedebug.luaforge.net/				"SciteDebug"
[incautcomp]: http://groups.google.com/group/scite-interest/browse_thread/thread/87ba9fd13989ae84

Dependencies and modifications
==============================

I removed the [Lua][] interpreter from SciTE source and modified the makefile to use existing Lua interpreter.
You can also use [LuaJIT][] by compilig SciTE with `make USE_LUAJIT=1`. Also, the [LPEG][] library is
removed froum source, you need to have it installed in your system in a way accessible to Lua (ie. 
in package.cpath).

I also modified some default values in SciTEGlobal.properties, if you do not like them, feel free to
edit/remove them.

[Extman.lua][extman] from [SciteDebug][] is modified to call modules in [SciTE-tools][], and modules
in [SciTE-tools][] are modified to co-exist with [extman.lua][extman].

You can add your custom Lua scripts for extman to the folder `$(SciteDefaultHome)/scite-debug/scite_lua`,
like the ones from [this repository](http://github.com/mkottman/scite_scripts).

[lua]: http://www.lua.org/						"Lua"
[luajit]: http://luajit.org/					"LuaJIT"
[extman]: http://lua-users.org/wiki/SciteExtMan "Extman"

Installation
============

Currently only Linux installation is tested, and it's not yet fully automated (to be done soon),
so run these commands in the project directory:

	cd scintilla/gtk
	make
	cd ../../scite/gtk
	make
	make install # as root

It will install SciTE and all required files into /usr/share/scite/.

Manual installation
-------------------

If you want to install SciTE into some other place, skip the last 'make install'
and copy scite/bin/SciTE executable, scite/src/\*.properties into destination folder
and then copy and overwrite all files and directories from scite/custom/\* into destination folder.
