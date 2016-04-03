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

#pragma once

#include "../maths/vector2d.h"
#include "../text/halleystring.h"

namespace Halley {
	class Image {
	public:
		Image(unsigned int w=0, unsigned int h=0);
		Image(String filename, const Byte* bytes, size_t nBytes, bool preMultiply);

		void load(String filename, const Byte* bytes, size_t nBytes, bool preMultiply);
		void savePNG(String filename="");

		bool isPremultiplied() const { return preMultiplied; }

		int getPixel(Vector2i pos) const;
		int getPixelAlpha(Vector2i pos) const;
		static int getRGBA(int r, int g, int b, int a=255);
		char* getPixels() { return px.get(); }
		const char* getPixels() const { return px.get(); }
		unsigned int getWidth() const { return w; }
		unsigned int getHeight() const { return h; }
		int getNComponents() const { return nComponents; }

	private:
		String filename;

		std::unique_ptr<char, void(*)(void*)> px;
		size_t dataLen;
		unsigned int w;
		unsigned int h;
		int nComponents;
		bool preMultiplied;
		
		void preMultiply();
	};
}