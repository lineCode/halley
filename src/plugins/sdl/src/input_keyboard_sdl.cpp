/*****************************************************************\
           __
          / /
		 / /                     __  __
		/ /______    _______    / / / / ________   __       __
	   / ______  \  /_____  \  / / / / / _____  | / /      / /
	  / /      | / _______| / / / / / / /____/ / / /      / /
	 / /      / / / _____  / / / / / / _______/ / /      / /
	/ /      / / / /____/ / / / / / / |______  / |______/ /
   /_/      /_/ |________/ / / / /  \_______/  \_______  /
                          /_/ /_/                     / /
			                                         / /
		       High Level Game Framework            /_/

  ---------------------------------------------------------------

  Copyright (c) 2007-2011 - Rodrigo Braz Monteiro.
  This file is subject to the terms of halley_license.txt.

\*****************************************************************/

#include <SDL.h>
#include "input_keyboard_sdl.h"
#include "halley/core/input/input_keys.h"
using namespace Halley;

#if (SDL_MAJOR_VERSION >= 1 && SDL_MINOR_VERSION >= 3) || SDL_MAJOR_VERSION >= 2
#define SDL_KEYS SDL_NUM_SCANCODES
#else
#define SDL_KEYS SDLK_LAST
#endif

InputKeyboardSDL::InputKeyboardSDL()
	: InputButtonBase(SDL_KEYS)
{
	SDL_StartTextInput();
}

void InputKeyboardSDL::processEvent(const SDL_Event &_event)
{
	if (_event.type == SDL_TEXTINPUT) {
		const SDL_TextInputEvent& event = _event.text;
		onTextEntered(event.text);
	} else if (_event.type == SDL_TEXTEDITING) {
		//const SDL_TextEditingEvent& event = _event.edit;
	} else {
		const SDL_KeyboardEvent& event = _event.key;
		switch (event.type) {
			case SDL_KEYUP:
				onButtonReleased(event.keysym.scancode);
				break;
			case SDL_KEYDOWN:
			{
				int scancode = event.keysym.scancode;
				onButtonPressed(scancode);
				if (scancode == Keys::Backspace) {
					letters.push_back('\b');
				}
				if (scancode == Keys::Enter || scancode == Keys::KP_Enter) {
					letters.push_back('\n');
				}
				if (scancode == Keys::Tab) {
					letters.push_back('\t');
				}
				break;
			}
		}
	}
}


void InputKeyboardSDL::onTextEntered(const char* text)
{
	auto str = String(text).getUTF32();
	for (size_t i=0; i<str.size(); i++) {
		letters.push_back(str[i]);
	}
}

int InputKeyboardSDL::getNextLetter()
{
	if (letters.size()) {
		int letter = letters.front();
		letters.pop_front();
		return letter;
	} else {
		return 0;
	}
}

String InputKeyboardSDL::getButtonName(int code)
{
	switch (code) {
	case Keys::Esc:
		return "Esc";
	case Keys::Delete:
		return "Del";
	default:
		if (code >= Keys::A && code <= Keys::Z) {
			return String(wchar_t(code - Keys::A + 'A'));
		} else {
			return SDL_GetKeyName(SDL_Keycode(code));
		}
	}
}
