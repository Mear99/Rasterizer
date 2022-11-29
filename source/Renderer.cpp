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
	m_Camera.Initialize(45.f, { 0.0f,0.0f,0.0f }, float(m_Width)/ m_Height);

	// Load tuktuk mesh
	m_pObjectMesh = new Mesh();
	Utils::ParseOBJ("Resources/vehicle.obj", m_pObjectMesh->vertices, m_pObjectMesh->indices);
	m_pObjectMesh->primitiveTopology = PrimitiveTopology::TriangleList;

	// Initialize Textures
	m_pDiffuseColor = Texture::LoadFromFile("Resources/vehicle_diffuse.png");
	m_pNormalMap = Texture::LoadFromFile("Resources/vehicle_normal.png");
	m_pSpecularMap = Texture::LoadFromFile("Resources/vehicle_specular.png");
	m_pGlossyMap = Texture::LoadFromFile("Resources/vehicle_gloss.png");
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;

	delete m_pDiffuseColor;
	delete m_pNormalMap;
	delete m_pSpecularMap;
	delete m_pGlossyMap;
	delete m_pObjectMesh;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	if (m_DoRotation) {
		m_Angle += m_RotateSpeed * pTimer->GetElapsed();
		if (m_Angle > PI_2) {
			m_Angle -= PI_2;
		}

		m_pObjectMesh->worldMatrix = Matrix::CreateRotationY(m_Angle) * Matrix::CreateTranslation(0, 0, 50);
	}
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);
	
	// Clear buffers
	SDL_FillRect(m_pBackBuffer, NULL, SDL_MapRGB(m_pBackBuffer->format, 128, 128, 128));
	std::fill_n(m_pDepthBufferPixels, m_Width*m_Height, FLT_MAX);

	//RENDER LOGIC
	//W1_Rasterization();
	//W1_Perspective();
	//W1_BaryCentricCoords();
	//W1_DepthBuffer();
	//W1_BoundingBox();

	//W2_TriangleList();
	//W2_TriangleStrip();
	//W2_Textures();

	RenderMeshes();

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

void Renderer::VertexTransformationFunction(Mesh& mesh) const
{
	mesh.vertices_out.reserve(mesh.vertices.size());

	Matrix WVPMatrix{ mesh.worldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix };

	for (const Vertex& vertex : mesh.vertices) {

		// Init out vertex
		Vertex_Out vertexOut{};
		vertexOut.position = { vertex.position.x, vertex.position.y, vertex.position.z, 1 };
		vertexOut.color = vertex.color;
		vertexOut.uv = vertex.uv;
		vertexOut.normal = vertex.normal;
		vertexOut.tangent = vertex.tangent;

		// World to NDC space
		vertexOut.position = WVPMatrix.TransformPoint(vertexOut.position);

		// Perspective divide
		vertexOut.position.x /= vertexOut.position.w;
		vertexOut.position.y /= vertexOut.position.w;
		vertexOut.position.z /= vertexOut.position.w;

		// Transform normal and tangent To World space
		vertexOut.normal = mesh.worldMatrix.TransformVector(vertexOut.normal);
		vertexOut.tangent = mesh.worldMatrix.TransformVector(vertexOut.tangent);

		// Calculate viewDirection
		vertexOut.viewDirection = mesh.worldMatrix.TransformPoint(vertex.position) - m_Camera.origin;
		vertexOut.viewDirection.Normalize();

		mesh.vertices_out.emplace_back(vertexOut);
	}
}

void Renderer::ToggleMode() {
	m_RenderMode = RenderMode((int(m_RenderMode) + 1) % 4);
}

void Renderer::ToggleBoundingBoxes() {
	m_VisualizeBoundingBoxes = !m_VisualizeBoundingBoxes;
}

void Renderer::ToggleDepthBuffer() {
	m_VisualizeDepthBuffer = !m_VisualizeDepthBuffer;
}

void Renderer::ToggleRotation() {
	m_DoRotation = !m_DoRotation;
}

void Renderer::ToggleNormalMap() {
	m_UseNormalMap = !m_UseNormalMap;
}

float Renderer::Remap(float value, float min, float max) {
	return (value - min) / (max - min);
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

void Renderer::W2_TriangleList() {

	std::vector<Mesh> meshes_world{
		Mesh{
			{
				Vertex{{-3,3,-2}},
				Vertex{{0,3,-2}},
				Vertex{{3,3,-2}},
				Vertex{{-3,0,-2}},
				Vertex{{0,0,-2}},
				Vertex{{3,0,-2}},
				Vertex{{-3,-3,-2}},
				Vertex{{0,-3,-2}},
				Vertex{{3,-3,-2}},
			},
			{
				3,0,1,	1,4,3,	4,1,2,
				2,5,4,	6,3,4,	4,7,6,
				7,4,5,	5,8,7
			},
			PrimitiveTopology::TriangleList
		}
	};

	for (Mesh mesh : meshes_world) {

		VertexTransformationFunction(mesh);

		// Loop over triangles
		for (int triangleIndex{ 0 }; triangleIndex + 2 < mesh.indices.size(); ++triangleIndex) {

			int index0{}, index1{}, index2{};

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleList) {

				index0 = mesh.indices[triangleIndex + 0];
				index1 = mesh.indices[triangleIndex + 1];
				index2 = mesh.indices[triangleIndex + 2];
				triangleIndex += 2;
			}

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip) {

				index0 = mesh.indices[triangleIndex + 0];
				if (triangleIndex % 2 == 0) {
					index1 = mesh.indices[triangleIndex + 1];
					index2 = mesh.indices[triangleIndex + 2];
				}
				else {
					index1 = mesh.indices[triangleIndex + 2];
					index2 = mesh.indices[triangleIndex + 1];
				}


				if (index0 == index1 || index1 == index2 || index2 == index0) {
					continue;
				}
			}

			Vertex_Out v0 = mesh.vertices_out[index0];
			Vertex_Out v1 = mesh.vertices_out[index1];
			Vertex_Out v2 = mesh.vertices_out[index2];

			// Find bounding box
			Int2 pMin{}, pMax{};
			pMin.x = Clamp(int(std::min(v2.position.x, std::min(v0.position.x, v1.position.x))), 0, m_Width);
			pMin.y = Clamp(int(std::min(v2.position.y, std::min(v0.position.y, v1.position.y))), 0, m_Height);
			pMax.x = Clamp(int(std::max(v2.position.x, std::max(v0.position.x, v1.position.x))), 0, m_Width);
			pMax.y = Clamp(int(std::max(v2.position.y, std::max(v0.position.y, v1.position.y))), 0, m_Height);

			// Loop over pixels
			for (int px{ pMin.x }; px <= pMax.x; ++px) {
				for (int py{ pMin.y }; py <= pMax.y; ++py) {

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
}

void Renderer::W2_TriangleStrip() {

	std::vector<Mesh> meshes_world{
		Mesh{
			{
				Vertex{{-3,3,-2}},
				Vertex{{0,3,-2}},
				Vertex{{3,3,-2}},
				Vertex{{-3,0,-2}},
				Vertex{{0,0,-2}},
				Vertex{{3,0,-2}},
				Vertex{{-3,-3,-2}},
				Vertex{{0,-3,-2}},
				Vertex{{3,-3,-2}},
			},
			{
				3,0,4,1,5,2,
				2,6,
				6,3,7,4,8,5
			},
			PrimitiveTopology::TriangleStrip
		}
	};

	for (Mesh mesh : meshes_world) {

		VertexTransformationFunction(mesh);

		// Loop over triangles
		for (int triangleIndex{ 0 }; triangleIndex + 2 < mesh.indices.size(); ++triangleIndex) {

			// Get correct indexes based on the mesh's topology
			int index0{}, index1{}, index2{};
			if (mesh.primitiveTopology == PrimitiveTopology::TriangleList) {

				index0 = mesh.indices[triangleIndex + 0];
				index1 = mesh.indices[triangleIndex + 1];
				index2 = mesh.indices[triangleIndex + 2];
				triangleIndex += 2;
			}

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip) {

				index0 = mesh.indices[triangleIndex + 0];
				if (triangleIndex % 2 == 0) {
					index1 = mesh.indices[triangleIndex + 1];
					index2 = mesh.indices[triangleIndex + 2];
				}
				else {
					index1 = mesh.indices[triangleIndex + 2];
					index2 = mesh.indices[triangleIndex + 1];
				}

				if (index0 == index1 || index1 == index2 || index2 == index0) {
					continue;
				}
			}

			// Render triangle
			Vertex_Out v0 = mesh.vertices_out[index0];
			Vertex_Out v1 = mesh.vertices_out[index1];
			Vertex_Out v2 = mesh.vertices_out[index2];

			// Find bounding box
			Int2 pMin{}, pMax{};
			pMin.x = Clamp(int(std::min(v2.position.x, std::min(v0.position.x, v1.position.x))), 0, m_Width);
			pMin.y = Clamp(int(std::min(v2.position.y, std::min(v0.position.y, v1.position.y))), 0, m_Height);
			pMax.x = Clamp(int(std::max(v2.position.x, std::max(v0.position.x, v1.position.x))), 0, m_Width);
			pMax.y = Clamp(int(std::max(v2.position.y, std::max(v0.position.y, v1.position.y))), 0, m_Height);

			// Loop over pixels
			for (int px{ pMin.x }; px <= pMax.x; ++px) {
				for (int py{ pMin.y }; py <= pMax.y; ++py) {

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
}

void Renderer::W2_Textures() {

	std::vector<Mesh> meshes_world{
		Mesh{
			{
				Vertex{{-3,3,-2},{}, {0,0}},
				Vertex{{0,3,-2},{}, {0.5f,0}},
				Vertex{{3,3,-2},{}, {1,0}},
				Vertex{{-3,0,-2},{}, {0,0.5f}},
				Vertex{{0,0,-2},{}, {0.5f,0.5f}},
				Vertex{{3,0,-2},{}, {1,0.5f}},
				Vertex{{-3,-3,-2},{}, {0,1}},
				Vertex{{0,-3,-2},{}, {0.5f,1}},
				Vertex{{3,-3,-2},{}, {1,1}},
			},
			{
				3,0,4,1,5,2,
				2,6,
				6,3,7,4,8,5
			},
			PrimitiveTopology::TriangleStrip
		}
	};

	for (Mesh mesh : meshes_world) {

		VertexTransformationFunction(mesh);

		// Loop over triangles
		for (int triangleIndex{ 0 }; triangleIndex + 2 < mesh.indices.size(); ++triangleIndex) {

			// Get correct indexes based on the mesh's topology
			int index0{}, index1{}, index2{};
			if (mesh.primitiveTopology == PrimitiveTopology::TriangleList) {

				index0 = mesh.indices[triangleIndex + 0];
				index1 = mesh.indices[triangleIndex + 1];
				index2 = mesh.indices[triangleIndex + 2];
				triangleIndex += 2;
			}

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip) {

				index0 = mesh.indices[triangleIndex + 0];
				if (triangleIndex % 2 == 0) {
					index1 = mesh.indices[triangleIndex + 1];
					index2 = mesh.indices[triangleIndex + 2];
				}
				else {
					index1 = mesh.indices[triangleIndex + 2];
					index2 = mesh.indices[triangleIndex + 1];
				}

				if (index0 == index1 || index1 == index2 || index2 == index0) {
					continue;
				}
			}

			// Render triangle
			Vertex_Out v0 = mesh.vertices_out[index0];
			Vertex_Out v1 = mesh.vertices_out[index1];
			Vertex_Out v2 = mesh.vertices_out[index2];

			// Find bounding box
			Int2 pMin{}, pMax{};
			pMin.x = Clamp(int(std::min(v2.position.x, std::min(v0.position.x, v1.position.x))), 0, m_Width);
			pMin.y = Clamp(int(std::min(v2.position.y, std::min(v0.position.y, v1.position.y))), 0, m_Height);
			pMax.x = Clamp(int(std::max(v2.position.x, std::max(v0.position.x, v1.position.x))), 0, m_Width);
			pMax.y = Clamp(int(std::max(v2.position.y, std::max(v0.position.y, v1.position.y))), 0, m_Height);

			// Loop over pixels
			for (int px{ pMin.x }; px <= pMax.x; ++px) {
				for (int py{ pMin.y }; py <= pMax.y; ++py) {

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

							// Interpolated UV
							Vector2 interpolatedUV{ w0 * v0.uv + w1 * v1.uv + w2 * v2.uv };
							finalColor = m_pDiffuseColor->Sample(interpolatedUV);

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
}

void Renderer::RenderMeshes() {

	std::vector<Mesh> meshes_world{
		/*Mesh{
			{
				Vertex{{-3,3,-2},{}, {0,0}},
				Vertex{{0,3,-2},{}, {0.5f,0}},
				Vertex{{3,3,-2},{}, {1,0}},
				Vertex{{-3,0,-2},{}, {0,0.5f}},
				Vertex{{0,0,-2},{}, {0.5f,0.5f}},
				Vertex{{3,0,-2},{}, {1,0.5f}},
				Vertex{{-3,-3,-2},{}, {0,1}},
				Vertex{{0,-3,-2},{}, {0.5f,1}},
				Vertex{{3,-3,-2},{}, {1,1}},
			},
			{
				3,0,4,1,5,2,
				2,6,
				6,3,7,4,8,5
			},
			PrimitiveTopology::TriangleStrip
		}*/
		* m_pObjectMesh,
	};

	for (Mesh mesh : meshes_world) {

		VertexTransformationFunction(mesh);

		// Loop over triangles
		for (int triangleIndex{ 0 }; triangleIndex + 2 < mesh.indices.size(); ++triangleIndex) {

			// Get correct indexes based on the mesh's topology
			int index0{}, index1{}, index2{};
			if (mesh.primitiveTopology == PrimitiveTopology::TriangleList) {

				index0 = mesh.indices[triangleIndex + 0];
				index1 = mesh.indices[triangleIndex + 1];
				index2 = mesh.indices[triangleIndex + 2];
				triangleIndex += 2;
			}

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip) {

				index0 = mesh.indices[triangleIndex + 0];
				if (triangleIndex % 2 == 0) {
					index1 = mesh.indices[triangleIndex + 1];
					index2 = mesh.indices[triangleIndex + 2];
				}
				else {
					index1 = mesh.indices[triangleIndex + 2];
					index2 = mesh.indices[triangleIndex + 1];
				}

				if (index0 == index1 || index1 == index2 || index2 == index0) {
					continue;
				}
			}

			// Render triangle
			Vertex_Out v0 = mesh.vertices_out[index0];
			Vertex_Out v1 = mesh.vertices_out[index1];
			Vertex_Out v2 = mesh.vertices_out[index2];

			// Culling triangles
			if (abs(v0.position.x) > 1 || abs(v0.position.y) > 1 || v0.position.z < 0 || v0.position.z > 1) {
				continue;
			}
			if (abs(v1.position.x) > 1 || abs(v1.position.y) > 1 || v1.position.z < 0 || v1.position.z > 1) {
				continue;
			}
			if (abs(v2.position.x) > 1 || abs(v2.position.y) > 1 || v2.position.z < 0 || v2.position.z > 1) {
				continue;
			}

			// To Screen Space
			v0.position.x = (v0.position.x + 1) * m_Width / 2;
			v0.position.y = (-v0.position.y + 1) * m_Height / 2;
			v1.position.x = (v1.position.x + 1) * m_Width / 2;
			v1.position.y = (-v1.position.y + 1) * m_Height / 2;
			v2.position.x = (v2.position.x + 1) * m_Width / 2;
			v2.position.y = (-v2.position.y + 1) * m_Height / 2;

			// Find bounding box
			Int2 pMin{}, pMax{};
			pMin.x = Clamp(int(std::min(v2.position.x, std::min(v0.position.x, v1.position.x))), 0, m_Width);
			pMin.y = Clamp(int(std::min(v2.position.y, std::min(v0.position.y, v1.position.y))), 0, m_Height);
			pMax.x = Clamp(int(std::max(v2.position.x, std::max(v0.position.x, v1.position.x))), 0, m_Width);
			pMax.y = Clamp(int(std::max(v2.position.y, std::max(v0.position.y, v1.position.y))), 0, m_Height);

			// Loop over pixels
			for (int px{ pMin.x }; px <= pMax.x; ++px) {
				for (int py{ pMin.y }; py <= pMax.y; ++py) {

					// Visualize the bouding boxes
					if (m_VisualizeBoundingBoxes) {
						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(255),
							static_cast<uint8_t>(255),
							static_cast<uint8_t>(255));
						continue;
					}

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

						float interpolatedDepth{ 1.0f / (w0 * (1 / v0.position.z) + w1 * (1 / v1.position.z) + w2 * (1 / v2.position.z)) };
						bool depthTestPassed{ interpolatedDepth < m_pDepthBufferPixels[px + (py * m_Width)] };

						if (depthTestPassed) {

							// Update Depth Buffer
							m_pDepthBufferPixels[px + (py * m_Width)] = interpolatedDepth;

							// Visualize the depth buffer
							if (m_VisualizeDepthBuffer) {

								float depthColor{ Remap(interpolatedDepth, 0.997f, 1.0f) };

								m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
									static_cast<uint8_t>(depthColor * 255),
									static_cast<uint8_t>(depthColor * 255),
									static_cast<uint8_t>(depthColor * 255));
								continue;
							}



							// InterpolatedW
							float interpolatedW{ 1.0f / (w0 * (1 / v0.position.w) + w1 * (1 / v1.position.w) + w2 * (1 / v2.position.w)) };

							// Interpolated UV
							Vector2 interpolatedUV{ w0 * (v0.uv / v0.position.w) + w1 * (v1.uv / v1.position.w) + w2 * (v2.uv / v2.position.w) };
							interpolatedUV *= interpolatedW;

							// Interpolated Normal
							Vector3 InterpolatedNormal{ w0 * (v0.normal / v0.position.w) + w1 * (v1.normal / v1.position.w) + w2 * (v2.normal / v2.position.w) };
							InterpolatedNormal *= interpolatedW;

							// Interpolated Tangent
							Vector3 InterpolatedTangent{ w0 * (v0.tangent / v0.position.w) + w1 * (v1.tangent / v1.position.w) + w2 * (v2.tangent / v2.position.w) };
							InterpolatedTangent *= interpolatedW;

							// Interpolated view direction
							Vector3 InterpolatedViewDirection{ w0 * (v0.viewDirection / v0.position.w) + w1 * (v1.viewDirection / v1.position.w) + w2 * (v2.viewDirection / v2.position.w) };
							InterpolatedViewDirection *= interpolatedW;

							Vertex_Out pixelVertex{};
							pixelVertex.position = { pixel.x, pixel.y, interpolatedDepth, interpolatedW };
							pixelVertex.uv = interpolatedUV;
							pixelVertex.normal = InterpolatedNormal.Normalized();
							pixelVertex.tangent = InterpolatedTangent.Normalized();
							pixelVertex.viewDirection = InterpolatedViewDirection.Normalized();

							ColorRGB finalColor{ PixelShading(pixelVertex) };

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
}

ColorRGB Renderer::PixelShading(const Vertex_Out& v) {
	
	const Vector3 lightDirection{ 0.577f, -0.577f, 0.577f };
	const float lightIntensity{ 7.0f };
	const float shininess{ 25.0f };
	ColorRGB finalColor{ 0,0,0 };
	ColorRGB ambientColor{ 0.025f, 0.025f, 0.025f };

	Vector3 sampledNomal{ v.normal };

	// Normal map calculations
	if (m_UseNormalMap) {
		Vector3 binormal{ Vector3::Cross(v.normal, v.tangent) };
		Matrix tangentSpaceAxis{ Matrix{v.tangent, binormal, v.normal, Vector3::Zero} };

		ColorRGB normalColor{ m_pNormalMap->Sample(v.uv) };
		sampledNomal = {normalColor.r, normalColor.g, normalColor.b };
		sampledNomal = (2.0f * sampledNomal) - Vector3{ 1.0f, 1.0f, 1.0f };
		sampledNomal = tangentSpaceAxis.TransformVector(sampledNomal);
	}

	// Cosine law
	float observedArea{Vector3::Dot(sampledNomal, -lightDirection)};

	if (observedArea > 0) {

		// Diffuse lambert color
		ColorRGB diffuseColor = lightIntensity * m_pDiffuseColor->Sample(v.uv) / PI;

		// Specular Color
		const ColorRGB ks{ m_pSpecularMap->Sample(v.uv) };
		const float exp{ m_pGlossyMap->Sample(v.uv).r * shininess };

		float dotproduct{ std::max(Vector3::Dot(sampledNomal,-lightDirection),0.0f) };
		Vector3 r{ (- lightDirection) - 2 * (dotproduct * sampledNomal)};
		float cosine{ std::max(Vector3::Dot(r,v.viewDirection),0.0f) };
		ColorRGB specularPhong{ ks * powf(cosine,exp) };

		// Final color
		switch (m_RenderMode) {
			case RenderMode::observerdArea:
				finalColor = ColorRGB{ observedArea, observedArea,observedArea };
				break;

			case RenderMode::diffuse:
				finalColor = diffuseColor * observedArea;
				break;

			case RenderMode::specular:
				finalColor = specularPhong * observedArea;
				break;

			case RenderMode::combined:
				finalColor = (diffuseColor + specularPhong + ambientColor) * observedArea;
					break;
		}
	}
	
	return finalColor;

}