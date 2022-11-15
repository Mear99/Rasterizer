#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Math.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle):
			origin{_origin},
			fovAngle{_fovAngle}
		{
		}


		Vector3 origin{};
		float fovAngle{90.f};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		Matrix invViewMatrix{};
		Matrix viewMatrix{};

		const float movementSpeed{ 10 };
		const float rotationSpeed{ 5 * TO_RADIANS };

		void Initialize(float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;
		}

		void CalculateViewMatrix()
		{
			Vector3 worldUp{ 0,1,0 };
			right = Vector3::Cross(worldUp, forward).Normalized();
			up = Vector3::Cross(forward, right); // Cross of two normalizes perpendicular vectors is still a normalized vector
			invViewMatrix = Matrix{ right, up, forward, origin };
			viewMatrix = Matrix::Inverse(invViewMatrix);

			//ViewMatrix => Matrix::CreateLookAtLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
		}

		void CalculateProjectionMatrix()
		{
			//TODO W2

			//ProjectionMatrix => Matrix::CreatePerspectiveFovLH(...) [not implemented yet]
			//DirectX Implementation => https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			//Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);

			// WASD movement
			// Forwards
			if (pKeyboardState[SDL_SCANCODE_W] || pKeyboardState[SDL_SCANCODE_UP]) {
				origin += pTimer->GetElapsed() * movementSpeed * forward;
			}
			// Backwards
			if (pKeyboardState[SDL_SCANCODE_S] || pKeyboardState[SDL_SCANCODE_DOWN]) {
				origin -= pTimer->GetElapsed() * movementSpeed * forward;
			}
			// Right
			if (pKeyboardState[SDL_SCANCODE_D] || pKeyboardState[SDL_SCANCODE_RIGHT]) {
				origin += pTimer->GetElapsed() * movementSpeed * right;
			}
			// Left
			if (pKeyboardState[SDL_SCANCODE_A] || pKeyboardState[SDL_SCANCODE_LEFT]) {
				origin -= pTimer->GetElapsed() * movementSpeed * right;
			}



			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			if ((SDL_BUTTON(1) & mouseState) != 0 && (SDL_BUTTON(3) & mouseState) != 0) {
				if (mouseY != 0) {
					origin -= pTimer->GetElapsed() * movementSpeed * up * (float(mouseY) / abs(mouseY));
				}
			}

			else if ((SDL_BUTTON(1) & mouseState) != 0) {
				totalYaw += mouseX * rotationSpeed * pTimer->GetElapsed();

				if (mouseY != 0) {
					origin -= pTimer->GetElapsed() * movementSpeed * forward * (float(mouseY) / abs(mouseY));
				}
			}

			else if ((SDL_BUTTON(3) & mouseState) != 0) {
				totalYaw += mouseX * rotationSpeed * pTimer->GetElapsed();
				totalPitch -= mouseY * rotationSpeed * pTimer->GetElapsed();
			}

			Matrix totalRotation{ Matrix::CreateRotation(totalPitch,totalYaw,0) };
			forward = totalRotation.TransformVector(Vector3::UnitZ);
			forward.Normalize();

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix(); //Try to optimize this - should only be called once or when fov/aspectRatio changes
		}
	};
}
