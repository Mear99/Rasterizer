#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"
#include "DataTypes.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	class Timer;
	class Scene;

	enum class RenderMode{normal, depth, bounding};

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render();

		bool SaveBufferToImage() const;

		void ToggleMode();

	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		int m_Width{};
		int m_Height{};

		//Functions that transforms the vertices from the mesh from World space to Screen space
		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const; //W1 Version
		void VertexTransformationFunction(Mesh& mesh) const; //W2 Version

		// Textures
		Texture* m_pTexture;

		// W1 Render stages
		void W1_Rasterization();
		void W1_Perspective();
		void W1_BaryCentricCoords();
		void W1_DepthBuffer();
		void W1_BoundingBox();

		// W2 Steps
		void W2_TriangleList();
		void W2_TriangleStrip();
		void W2_Textures();

		// Final Render loop
		void RenderMeshes();

		float Remap(float value, float min, float max);
		RenderMode m_RenderMode{ RenderMode::normal };

		// Tuktuk
		Mesh* m_pTuktukMesh = nullptr;
		float m_Angle{ 0.0f };
		float m_RotateSpeed{ PI_DIV_4 };
	};
}
