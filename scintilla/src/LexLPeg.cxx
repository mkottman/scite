/*
	Mitchell's Lua-powered dynamic lexer
	Copyright (c) 2006-2007 Mitchell Foral. All rights reserved.

	SciTE-tools homepage: http://caladbolg.net/scite.php
	Send email to: mitchell<att>caladbolg<dott>net

	Permission to use, copy, modify, and distribute this file is granted,
	provided credit is given to Mitchell.

	Implementation:
	Only one lexer is used: SCLEX_LPEG. Any Scintilla calls to SetLexer or
	SetLexerLanguage create a new Lua state and pass the desired lexer name to
	LexLPeg's InitDoc function where it is loaded. Afterwards, the Scintilla
	styles are read and set. The dynamic lexer is now ready to style the text
	when Scintilla calls ColouriseDoc.
	Folding works similarly.
	Lexer documentation can be read in /lexers/lexer.lua
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "Platform.h"

#include "PropSet.h"
#include "Accessor.h"
#include "DocumentAccessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#if PLAT_WIN || PLAT_GTK_WIN32
#define strcasecmp _stricmp
#endif

#define streq(s1, s2) (strcasecmp((s1), (s2)) == 0)

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

static WindowID windowID;
static DocumentAccessor *accessor;
static bool multilang_lexer = false;

/**
 * Prints a Lua error message.
 * If an error message is not specified, the Lua error message at the top of the
 * stack is used and the stack is subsequently cleared.
 * @return false
 */
static bool l_handle_error(lua_State *L, const char *errstr=NULL) {
	Platform::DebugPrintf(
		"Lua Error: %s.\n", errstr ? errstr : lua_tostring(L, -1));
	lua_settop(L, 0);
	return false;
}

/**
 * The Lua panic function.
 * Print the error message and close the Lua state.
 * Note: This function should never be called.
 */
static int lua_panic(lua_State *L) {
	l_handle_error(L);
	lua_close(L);
	L = NULL;
	return 0;
}

/**
 * Retrieves the style at a position.
 * Lua interface: StyleAt(pos)
 * @param pos The position to get the style for.
 */
static int lua_style_at(lua_State *L) {
	lua_pushnumber(L, accessor->StyleAt(luaL_checkinteger(L, 1) - 1));
	return 1;
}

/**
 * Gets an integer property value.
 * Lua interface: GetProperty(key [, default])
 * @param key The property key.
 * @param default Optional default value.
 * @return integer value of the property.
 */
static int lua_get_property(lua_State *L) {
	lua_pushnumber(L, accessor->GetPropertyInt(luaL_checkstring(L, 1),
	               (lua_gettop(L) > 1) ? luaL_checkinteger(L, 2) : 0));
	return 1;
}

/**
 * Gets the fold level of a line number.
 * Lua interface: GetFoldLevel(line_number)
 * @param line_number The line number to get the fold level of.
 * @return the integer fold level.
 */
static int lua_get_fold_level(lua_State *L) {
	lua_pushnumber(L, accessor->LevelAt(luaL_checkinteger(L, 1)));
	return 1;
}

/**
 * Gets the indent amount of text on a specified line.
 * Lua interface: GetIndentAmount(line_number)
 * @param line_number The line number to get the indent amount of.
 */
static int lua_get_indent_amount(lua_State *L) {
	int f = 0;
	lua_pushnumber(L, accessor->IndentAmount(luaL_checkinteger(L, 1), &f,	NULL));
	return 1;
}

/**
 * Gets a range of text in a Scintilla document.
 * @param windowID The Scintilla window.
 * @param startPos The start position of the text region to get.
 * @param endPos The end position of text region to get.
 * @return text
 */
static char *textRange(WindowID windowID, unsigned int startPos,
	                     unsigned int endPos) {
	char *text = new char[endPos - startPos + 1];
	Sci_TextRange tr;
	tr.chrg.cpMin = startPos;
	tr.chrg.cpMax = endPos;
	tr.lpstrText = text;
	Platform::SendScintillaPointer(windowID, SCI_GETTEXTRANGE, 0, &tr);
	return tr.lpstrText;
}

#define l_openlib(f, s) \
	{ lua_pushcfunction(L, f); lua_pushstring(L, s); lua_call(L, 1, 0); }
#define l_setconst(c, s) \
	{ lua_pushnumber(L, c); lua_setfield(L, LUA_GLOBALSINDEX, s); }
#define SS(m, l) Platform::SendScintilla(windowID, m, style_num, l)

/**
 * Initializes the document for styling.
 * Part of the Lexer interface.
 * ScintillaBase.cxx opens a new Lua state and passes it here to finish loading,
 * Then the lexer.lua script is loaded, and Scintilla styles set up.
 * The path to lexer.lua is the value of 'lexer_script' in LUA_REGISTRYINDEX
 * (previously set in ScintillaBase.cxx). lexer.lua's 'InitLexer' is called with
 * languageName as a parameter which then calls 'LoadStyles', populating a
 * 'Styles' table with the Scintilla styles to use for each style number. That
 * table is iterated over here, setting the styles in Scintilla. Style properties
 * are case insensitive.
 * e.g.
 *   Styles = {
 *     [1] = { fore = color, back = other_color, ... },
 *     [2] = { bold = true, EOLFilled = true, ... }
 *   }
 * @param L The newly created Lua state.
 * @param languageName The name of the lexer to load.
 * @param styler The document accessor used to style the document, get
 *   properties from it, etc.
 * @return true if successful.
 */
static bool InitDoc(lua_State *L, const char *languageName, Accessor &styler) {
	DocumentAccessor &da = static_cast<DocumentAccessor &>(styler);
	windowID = da.GetWindow();

	lua_atpanic(L, lua_panic);
	/*
	l_openlib(luaopen_base, "");
	l_openlib(luaopen_table, LUA_TABLIBNAME);
	l_openlib(luaopen_string, LUA_STRLIBNAME);
	l_openlib(luaopen_package, LUA_LOADLIBNAME);
	*/
	luaL_openlibs(L);
	
	lua_getfield(L, LUA_GLOBALSINDEX, "require");
	lua_pushstring(L, "lpeg");
	lua_call(L, 1, 0);

	lua_getfield(L, LUA_REGISTRYINDEX, "lexer_script");
	const char *lexerScript = lua_tostring(L, -1);
	lua_pop(L, 1); // lexer_script
	if (strlen(lexerScript) == 0) return false;
	if (luaL_dofile(L, lexerScript) != 0) return l_handle_error(L);

	lua_pushstring(L, languageName);
	lua_setfield(L, LUA_REGISTRYINDEX, "languageName");

	lua_getglobal(L, "InitLexer");
	if (lua_isfunction(L, -1)) {
		lua_pushstring(L, languageName);
		if (lua_pcall(L, 1, 0, 0) != 0) return l_handle_error(L);
	} else return l_handle_error(L, "'InitLexer' function not found");

	// register functions
	lua_register(L, "GetStyleAt", lua_style_at);
	lua_register(L, "GetProperty", lua_get_property);
	lua_register(L, "GetFoldLevel", lua_get_fold_level);
	lua_register(L, "GetIndentAmount", lua_get_indent_amount);

	// register constants
	l_setconst(SC_FOLDLEVELBASE, "SC_FOLDLEVELBASE");
	l_setconst(SC_FOLDLEVELWHITEFLAG, "SC_FOLDLEVELWHITEFLAG");
	l_setconst(SC_FOLDLEVELHEADERFLAG, "SC_FOLDLEVELHEADERFLAG");
	l_setconst(SC_FOLDLEVELNUMBERMASK, "SC_FOLDLEVELNUMBERMASK");

	// setup Scintilla styles
	Platform::SendScintilla(windowID, SCI_STYLECLEARALL);
	lua_getglobal(L, "Lexer");
	if (!lua_istable(L, -1))
		return l_handle_error(L, "'Lexer' table not found");
	lua_getfield(L, -1, "Styles");
	if (!lua_istable(L, -1))
		return l_handle_error(L, "'Lexer.Styles' table not found");
	lua_pushnil(L);
	while (lua_next(L, -2)) { // Styles table
		if (lua_isnumber(L, -2) && lua_istable(L, -1)) {
			int style_num = lua_tointeger(L, -2); // [num] = { properties }
			lua_pushnil(L);
			while (lua_next(L, -2)) { // properties table
				const char *prop = lua_tostring(L, -2);
				if (streq(prop, "font"))
					SS(SCI_STYLESETFONT, reinterpret_cast<long>(lua_tostring(L, -1)));
				else if (streq(prop, "size"))
					SS(SCI_STYLESETSIZE, static_cast<int>(lua_tointeger(L, -1)));
				else if (streq(prop, "bold"))
					SS(SCI_STYLESETBOLD, lua_toboolean(L, -1));
				else if (streq(prop, "italic"))
					SS(SCI_STYLESETITALIC, lua_toboolean(L, -1));
				else if (streq(prop, "underline"))
					SS(SCI_STYLESETUNDERLINE, lua_toboolean(L, -1));
				else if (streq(prop, "fore"))
					SS(SCI_STYLESETFORE, static_cast<int>(lua_tointeger(L, -1)));
				else if (streq(prop, "back"))
					SS(SCI_STYLESETBACK, static_cast<int>(lua_tointeger(L, -1)));
				else if (streq(prop, "eolfilled"))
					SS(SCI_STYLESETEOLFILLED, lua_toboolean(L, -1));
				else if (streq(prop, "characterset"))
					SS(SCI_STYLESETCHARACTERSET, static_cast<int>(lua_tointeger(L, -1)));
				else if (streq(prop, "case"))
					SS(SCI_STYLESETCASE, static_cast<int>(lua_tointeger(L, -1)));
				else if (streq(prop, "visible"))
					SS(SCI_STYLESETVISIBLE, lua_toboolean(L, -1));
				else if (streq(prop, "changeable"))
					SS(SCI_STYLESETCHANGEABLE, lua_toboolean(L, -1));
				else if (streq(prop, "hotspot"))
					SS(SCI_STYLESETHOTSPOT, lua_toboolean(L, -1));
				lua_pop(L, 1); // value
			}
		} lua_pop(L, 1); // value
	} lua_pop(L, 2); // Styles and Lexer

	// if the lexer is a child, it will have a parent in its 'EmbeddedIn' field
	lua_getglobal(L, "Lexer");
	lua_getfield(L, -1, "EmbeddedIn");
	multilang_lexer = lua_istable(L, -1);
	lua_pop(L, 2); // Lexer.EmbeddedIn and Lexer

	return true;
}

/**
 * Styles the document.
 * Part of the Lexer interface.
 * When Scintilla requires the document to be styled, this function does it.
 * It calls lexer.lua's 'RunLexer' function which returns a table of styling
 * information. That table is iterated over, styling the document.
 * @param startPos The beginning position of the region needing to be styled.
 * @param length The length of the region needing to be styled.
 * @param initstyle The initial style of the region needing to be styled.
 * @param L The Lua state associated with this document.
 * @param keywordLists [Unused].
 * @param styler The document accessor used to style the document, get
 *   properties from it, etc.
 */
static void ColouriseDoc(unsigned int startPos, int length, int initStyle,
                         lua_State *L, WordList**, Accessor &styler) {
	if (!L) return;
	DocumentAccessor &da = static_cast<DocumentAccessor &>(styler);
	windowID = da.GetWindow();
	accessor = &da;

	// start from the beginning of the current style so it is matched by the LPeg
	// lexer; applies only to single-language lexers
	if (!multilang_lexer && startPos > 0) {
		int i = startPos;
		while (i > 0 && da.StyleAt(i - 1) == initStyle) i--;
		length += startPos - i;
		startPos = i;
	}

	unsigned int startSeg = startPos, endSeg = startPos + length;
	int style = 0;

	accessor->StartAt(startPos, static_cast<char>(STYLE_MAX)); // all 8 style bits
	accessor->StartSegment(startPos);
	lua_getglobal(L, "RunLexer");
	if (lua_isfunction(L, -1)) {
		char *text = multilang_lexer ? textRange(windowID, 0, da.Length())
		                             : textRange(windowID, startSeg, endSeg);
		lua_pushstring(L, text);
		if (lua_pcall(L, 1, 1, 0) != 0) l_handle_error(L);
		delete []text;
		// style the text from the token table returned
		if (lua_istable(L, -1)) {
			lua_getglobal(L, "Lexer");
			lua_pushstring(L, "Types");
			lua_rawget(L, -2);
			lua_remove(L, lua_gettop(L) - 1); // Lexer
			lua_pushnil(L);
			while (lua_next(L, -3)) { // token (tokens[i])
				if (!lua_istable(L, -1)) {
					l_handle_error(L, "Table of tokens expected from 'RunLexer'");
					break;
				}
				lua_rawgeti(L, -1, 1); // token[1]
				lua_rawget(L, -4); // Lexer.Types[token[1]]
				style = 32;
				if (!lua_isnil(L, -1)) style = lua_tointeger(L, -1);
				lua_pop(L, 1); // Lexer.Types[token[1]]
				lua_rawgeti(L, -1, 2); // token[2]
				unsigned int position = lua_tointeger(L, -1) - 1;
				lua_pop(L, 1); // token[2]
				lua_pop(L, 1); // token (tokens[i])
				if (style >= 0 && style <= STYLE_MAX) {
					if (multilang_lexer) {
						if (position > startSeg && position <= endSeg)
							accessor->ColourTo(position - 1, style);
						else if (position > endSeg && accessor->GetStartSegment() < endSeg)
							accessor->ColourTo(endSeg - 1, style); // style remaining length
					} else accessor->ColourTo(startSeg + position - 1, style);
				} else l_handle_error(L, "Bad style number");
				if (position > endSeg) break;
			}
			lua_pop(L, 2); // Lexer.Types and token table returned
		} else l_handle_error(L, "Table of tokens expected from 'RunLexer'");
	} else l_handle_error(L, "'RunLexer' function not found");
	accessor->ColourTo(endSeg - 1, style);
}

/** Folds the document.
 *  Part of the Lexer interface.
 *  When Scintilla requires the document to be folded, this function does it.
 *  It calls lexer.lua's 'RunFolder' function which performs the folding via
 *  the 'SetFoldLevel' Lua interface.
 *  @param startPos The beginning position of the region needing to be folded.
 *  @param length The length of the region needing to be folded.
 *  @param initstyle [Unused].
 *  @param L The Lua state associated with this document.
 *  @param keywordLists [Unused].
 *  @param styler The document accessor used to style the document, get
 *    properties from it, etc.
 */
static void FoldDoc(unsigned int startPos, int length, int, lua_State *L,
                    WordList *[], Accessor &styler) {
	if (!L) return;
	DocumentAccessor &da = static_cast<DocumentAccessor &>(styler);
	windowID = da.GetWindow();

	lua_getglobal(L, "RunFolder");
	if (lua_isfunction(L, -1)) {
		int currentLine = styler.GetLine(startPos);
		char *text = textRange(windowID, startPos, startPos + length);
		lua_pushstring(L, text);
		lua_pushnumber(L, startPos);
		lua_pushnumber(L, currentLine);
		lua_pushnumber(L, styler.LevelAt(currentLine) & SC_FOLDLEVELNUMBERMASK);
		if (lua_pcall(L, 4, 1, 0) != 0) l_handle_error(L);
		delete []text;
		// fold the text from the fold table returned
		if (lua_istable(L, -1)) {
			lua_pushnil(L);
			int line = 0, level = 0;
			while (lua_next(L, -2)) { // fold (folds[i])
				if (!lua_istable(L, -1)) {
					l_handle_error(L, "Table of folds expected from 'RunFolder'");
					break;
				}
				line = lua_tointeger(L, -2);
				lua_rawgeti(L, -1, 1); // fold[1]
				level = lua_tointeger(L, -1);
				lua_pop(L, 1); // fold[1]
				if (lua_objlen(L, -1) > 1) {
					lua_rawgeti(L, -1, 2); // fold[2]
					int flag = lua_tointeger(L, -1);
					level |= flag;
					lua_pop(L, 1); // fold[2]
				}
				styler.SetLevel(line, level);
				lua_pop(L, 1); // fold
			}
			lua_pop(L, 1); // fold table returned
			// Mask off the level number, leaving only the previous flags.
			int flagsNext = styler.LevelAt(line + 1);
			flagsNext &= ~SC_FOLDLEVELNUMBERMASK;
			styler.SetLevel(line + 1, level | flagsNext);
		} else l_handle_error(L, "Table of folds expected from 'RunFolder'");
	} else l_handle_error(L, "'RunFolder' function not found");
}

LexerModule lmLPeg(SCLEX_LPEG, InitDoc, ColouriseDoc, "llpeg", FoldDoc, 0, 7);
LexerModule lmNull(SCLEX_NULL, InitDoc, ColouriseDoc, "null");
