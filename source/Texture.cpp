#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		//TODO
		//Load SDL_Surface using IMG_LOAD
		//Create & Return a new Texture Object (using SDL_Surface)
		SDL_Surface* surface = IMG_Load(path.c_str());
		return new Texture(surface);
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		//TODO
		//Sample the correct texel for the given uv
		int width = m_pSurface->w;
		int height = m_pSurface->h;

		Vector2 pixel{ uv.x * width, uv.y * height };

		Uint8 r{}, g{}, b{};
		SDL_GetRGB(m_pSurfacePixels[int(pixel.x) + int(pixel.y) * width], m_pSurface->format, &r, &g, &b);
		ColorRGB sampledColor{ r,g,b };

		return (sampledColor / 255.0f);
	}
}