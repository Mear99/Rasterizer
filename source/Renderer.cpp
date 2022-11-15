//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Math.h"
#include "Matrix.h"
#include "Texture.h"
#include "Utils.h"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);
	
	// Clear buffers
	SDL_FillRect(m_pBackBuffer, NULL,0);
	std::fill_n(m_pDepthBufferPixels, m_Width*m_Height, FLT_MAX);

	//RENDER LOGIC
	//W1_Rasterization();
	//W1_Perspective();
	//W1_BaryCentricCoords();
	//W1_DepthBuffer();
	W1_BoundingBox();

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	vertices_out.reserve(vertices_in.size());

	for (Vertex vertex : vertices_in) {

		// World to view space
		vertex.position = m_Camera.viewMatrix.TransformPoint(vertex.position);

		// Perspective divide
		vertex.position.x /= vertex.position.z;
		vertex.position.y /= vertex.position.z;

		// Camera settings
		const float aspectRatio = float(m_Width) / m_Height;
		vertex.position.x /= (aspectRatio * m_Camera.fov);
		vertex.position.y /= (m_Camera.fov);

		// To Screen Space
		vertex.position.x = (vertex.position.x + 1) * m_Width / 2;
		vertex.position.y = (-vertex.position.y + 1) * m_Height / 2;

		vertices_out.emplace_back(vertex);
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}

void Renderer::W1_Rasterization() {
	//Triangel in NDC Space
	std::vector<Vector3> vertices_ndc{
		{0.0f,0.5f,1.0f},
		{0.5f,-0.5f,1.0f},
		{-0.5f,-0.5f,1.0f},
	};

	// Loop over triangles
	for (int triangleIndex{ 0 }; triangleIndex * 3 < vertices_ndc.size(); ++triangleIndex) {
		Vector2 v0{ vertices_ndc[triangleIndex * 3 + 0].GetXY() };
		Vector2 v1{ vertices_ndc[triangleIndex * 3 + 1].GetXY() };
		Vector2 v2{ vertices_ndc[triangleIndex * 3 + 2].GetXY() };

		v0.x = (v0.x + 1) * m_Width / 2;
		v1.x = (v1.x + 1) * m_Width / 2;
		v2.x = (v2.x + 1) * m_Width / 2;
		v0.y = (1 - v0.y) * m_Height / 2;
		v1.y = (1 - v1.y) * m_Height / 2;
		v2.y = (1 - v2.y) * m_Height / 2;


		// Loop over pixels
		for (int px{}; px < m_Width; ++px) {
			for (int py{}; py < m_Height; ++py) {

				Vector2 pixel{ float(px),float(py) };

				//Side A
				Vector2 side{ v1 - v0 };
				Vector2 pointToSide{ pixel - v0 };
				float v2Weight{ Vector2::Cross(side,pointToSide) };

				//Side B
				side = { v2 - v1 };
				pointToSide = { pixel - v1 };
				float v0Weight{ Vector2::Cross(side,pointToSide) };

				//Side C
				side = { v0 - v2 };
				pointToSide = { pixel - v2 };
				float v1Weight{ Vector2::Cross(side,pointToSide) };


				if (v2Weight >= 0 && v1Weight >= 0 && v0Weight >= 0) {
					ColorRGB finalColor{ colors::White };

					//Update Color in Buffer
					finalColor.MaxToOne();

					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}
			}
		}
	}
}

void Renderer::W1_Perspective() {

	std::vector<Vertex> vertices_world{
		{{0.0f,2.0f,0.0f}},
		{{1.0f,0.0f,0.0f}},
		{{-1.0f,0.0f,0.0f}},
	};
	std::vector<Vertex> vertices_screen{};

	VertexTransformationFunction(vertices_world, vertices_screen);

	// Loop over triangles
	for (int triangleIndex{ 0 }; triangleIndex * 3 < vertices_screen.size(); ++triangleIndex) {

		Vector2 v0{ vertices_screen[triangleIndex * 3 + 0].position.GetXY() };
		Vector2 v1{ vertices_screen[triangleIndex * 3 + 1].position.GetXY() };
		Vector2 v2{ vertices_screen[triangleIndex * 3 + 2].position.GetXY() };

		// Loop over pixels
		for (int px{}; px < m_Width; ++px) {
			for (int py{}; py < m_Height; ++py) {

				Vector2 pixel{ float(px),float(py) };

				//Side A
				Vector2 side{ v1 - v0 };
				Vector2 pointToSide{ pixel - v0 };
				float v2Weight{ Vector2::Cross(side,pointToSide) };

				//Side B
				side = { v2 - v1 };
				pointToSide = { pixel - v1 };
				float v0Weight{ Vector2::Cross(side,pointToSide) };

				//Side C
				side = { v0 - v2 };
				pointToSide = { pixel - v2 };
				float v1Weight{ Vector2::Cross(side,pointToSide) };

				if (v2Weight >= 0 && v1Weight >= 0 && v0Weight >= 0) {
					ColorRGB finalColor{ colors::White };

					//Update Color in Buffer
					finalColor.MaxToOne();

					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}
			}
		}
	}
}

void Renderer::W1_BaryCentricCoords() {

	std::vector<Vertex> vertices_world{
		{{0.0f,4.0f,2.0f}, {1,0,0}},
		{{3.0f,-2.0f,2.0f}, {0,1,0}},
		{{-3.0f,-2.0f,2.0f}, {0,0,1}},
	};
	std::vector<Vertex> vertices_screen{};

	VertexTransformationFunction(vertices_world, vertices_screen);

	// Loop over triangles
	for (int triangleIndex{ 0 }; triangleIndex * 3 < vertices_screen.size(); ++triangleIndex) {

		Vertex v0{ vertices_screen[triangleIndex * 3 + 0] };
		Vertex v1{ vertices_screen[triangleIndex * 3 + 1] };
		Vertex v2{ vertices_screen[triangleIndex * 3 + 2] };

		// Loop over pixels
		for (int px{}; px < m_Width; ++px) {
			for (int py{}; py < m_Height; ++py) {

				Vector2 pixel{ float(px),float(py) };

				//Side A
				Vector2 side{ v1.position.GetXY() - v0.position.GetXY() };
				Vector2 pointToSide{ pixel - v0.position.GetXY() };
				float w2{ Vector2::Cross(side,pointToSide) };

				//Side B
				side = { v2.position.GetXY() - v1.position.GetXY() };
				pointToSide = { pixel - v1.position.GetXY() };
				float w0{ Vector2::Cross(side,pointToSide) };

				//Side C
				side = { v0.position.GetXY() - v2.position.GetXY() };
				pointToSide = { pixel - v2.position.GetXY() };
				float w1{ Vector2::Cross(side,pointToSide) };

				if (w0 >= 0 && w1 >= 0 && w2 >= 0) {

					// Calculate Barycentric weights
					const float totalArea = w0 + w1 + w2;
					w0 /= totalArea;
					w1 /= totalArea;
					w2 /= totalArea;

					ColorRGB finalColor{ w0 * v0.color + w1 * v1.color + w2 * v2.color };

					//Update Color in Buffer
					finalColor.MaxToOne();

					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}
			}
		}
	}
}

void Renderer::W1_DepthBuffer() {

	std::vector<Vertex> vertices_world{

		// Triangle 1
		{{0.0f,2.0f,0.0f}, {1,0,0}},
		{{1.5f,-1.0f,0.0f}, {1,0,0}},
		{{-1.5f,-1.0f,0.0f}, {1,0,0}},

		// Triangle 2
		{{0.0f,4.0f,2.0f}, {1,0,0}},
		{{3.0f,-2.0f,2.0f}, {0,1,0}},
		{{-3.0f,-2.0f,2.0f}, {0,0,1}},
	};
	std::vector<Vertex> vertices_screen{};

	VertexTransformationFunction(vertices_world, vertices_screen);

	// Loop over triangles
	for (int triangleIndex{ 0 }; triangleIndex * 3 < vertices_screen.size(); ++triangleIndex) {

		Vertex v0{ vertices_screen[triangleIndex * 3 + 0] };
		Vertex v1{ vertices_screen[triangleIndex * 3 + 1] };
		Vertex v2{ vertices_screen[triangleIndex * 3 + 2] };

		// Loop over pixels
		for (int px{}; px < m_Width; ++px) {
			for (int py{}; py < m_Height; ++py) {

				Vector2 pixel{ float(px),float(py) };

				//Side A
				Vector2 side{ v1.position.GetXY() - v0.position.GetXY() };
				Vector2 pointToSide{ pixel - v0.position.GetXY() };
				float w2{ Vector2::Cross(side,pointToSide) };

				//Side B
				side = { v2.position.GetXY() - v1.position.GetXY() };
				pointToSide = { pixel - v1.position.GetXY() };
				float w0{ Vector2::Cross(side,pointToSide) };

				//Side C
				side = { v0.position.GetXY() - v2.position.GetXY() };
				pointToSide = { pixel - v2.position.GetXY() };
				float w1{ Vector2::Cross(side,pointToSide) };

				if (w0 >= 0 && w1 >= 0 && w2 >= 0) {

					// Calculate Barycentric weights
					const float totalArea = w0 + w1 + w2;
					w0 /= totalArea;
					w1 /= totalArea;
					w2 /= totalArea;

					float interpolatedDepth{ w0 * v0.position.z + w1 * v1.position.z + w2 * v2.position.z };
					bool depthTestPassed{ interpolatedDepth < m_pDepthBufferPixels[px + (py * m_Width)] };

					if (depthTestPassed) {

						// Update Depth Buffer
						m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedDepth;

						// Interpolated color
						ColorRGB finalColor{ w0 * v0.color + w1 * v1.color + w2 * v2.color };

						//Update Color in Buffer
						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
	}
}

void Renderer::W1_BoundingBox() {

	std::vector<Vertex> vertices_world{

		// Triangle 1
		{{0.0f,2.0f,0.0f}, {1,0,0}},
		{{1.5f,-1.0f,0.0f}, {1,0,0}},
		{{-1.5f,-1.0f,0.0f}, {1,0,0}},

		// Triangle 2
		{{0.0f,4.0f,2.0f}, {1,0,0}},
		{{3.0f,-2.0f,2.0f}, {0,1,0}},
		{{-3.0f,-2.0f,2.0f}, {0,0,1}},
	};
	std::vector<Vertex> vertices_screen{};

	VertexTransformationFunction(vertices_world, vertices_screen);

	// Loop over triangles
	for (int triangleIndex{ 0 }; triangleIndex * 3 < vertices_screen.size(); ++triangleIndex) {

		Vertex v0{ vertices_screen[triangleIndex * 3 + 0] };
		Vertex v1{ vertices_screen[triangleIndex * 3 + 1] };
		Vertex v2{ vertices_screen[triangleIndex * 3 + 2] };

		// Find bounding box
		Int2 pMin{}, pMax{};
		pMin.x = Clamp(int(std::min(v2.position.x, std::min(v0.position.x, v1.position.x))), 0, m_Width);
		pMin.y = Clamp(int(std::min(v2.position.y, std::min(v0.position.y, v1.position.y))), 0, m_Height);
		pMax.x = Clamp(int(std::max(v2.position.x, std::max(v0.position.x, v1.position.x))), 0, m_Width);
		pMax.y = Clamp(int(std::max(v2.position.y, std::max(v0.position.y, v1.position.y))), 0, m_Height);

		// Loop over pixels
		for (int px{pMin.x}; px <= pMax.x; ++px) {
			for (int py{pMin.y}; py <= pMax.y; ++py) {

				Vector2 pixel{ float(px),float(py) };

				//Side A
				Vector2 side{ v1.position.GetXY() - v0.position.GetXY() };
				Vector2 pointToSide{ pixel - v0.position.GetXY() };
				float w2{ Vector2::Cross(side,pointToSide) };

				//Side B
				side = { v2.position.GetXY() - v1.position.GetXY() };
				pointToSide = { pixel - v1.position.GetXY() };
				float w0{ Vector2::Cross(side,pointToSide) };

				//Side C
				side = { v0.position.GetXY() - v2.position.GetXY() };
				pointToSide = { pixel - v2.position.GetXY() };
				float w1{ Vector2::Cross(side,pointToSide) };

				if (w0 >= 0 && w1 >= 0 && w2 >= 0) {

					// Calculate Barycentric weights
					const float totalArea = w0 + w1 + w2;
					w0 /= totalArea;
					w1 /= totalArea;
					w2 /= totalArea;

					float interpolatedDepth{ w0 * v0.position.z + w1 * v1.position.z + w2 * v2.position.z };
					bool depthTestPassed{ interpolatedDepth < m_pDepthBufferPixels[px + (py * m_Width)] };

					if (depthTestPassed) {

						// Update Depth Buffer
						m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedDepth;

						// Interpolated color
						ColorRGB finalColor{ w0 * v0.color + w1 * v1.color + w2 * v2.color };

						//Update Color in Buffer
						finalColor.MaxToOne();

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));
					}
				}
			}
		}
	}
}