#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace Graphics
{
	class RadialBlurManager
	{
	public:
		static RadialBlurManager* GetSingleton()
		{
			static RadialBlurManager instance;
			return &instance;
		}

		void Initialize();
		void Shutdown();
		void SetBlurStrength(float a_strength);
		void SetBlurCenter(float a_x, float a_y);
		void ApplyRadialBlur();
		
		bool IsInitialized() const { return initialized; }

	private:
		RadialBlurManager() = default;
		~RadialBlurManager() = default;
		RadialBlurManager(const RadialBlurManager&) = delete;
		RadialBlurManager& operator=(const RadialBlurManager&) = delete;

		bool CreateShaders();
		bool CreateResources();
		void ReleaseResources();

		// D3D11 resources
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		
		Microsoft::WRL::ComPtr<ID3D11PixelShader> radialBlurPS;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVS;
		Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBufferCopy;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> backBufferCopySRV;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
		
		// Blur parameters
		struct BlurConstants
		{
			float strength;        // 0-1 blur intensity
			float centerX;         // Center X (0.5 = screen center)
			float centerY;         // Center Y (0.5 = screen center) 
			float sampleCount;     // Number of samples (8-16 recommended)
		};
		BlurConstants blurParams{ 0.0f, 0.5f, 0.5f, 8.0f };
		
		bool initialized{ false };
		UINT backBufferWidth{ 0 };
		UINT backBufferHeight{ 0 };
	};

	// Hook installation
	void InstallHooks();
}

