#pragma once
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

namespace GeometryShape {
	typedef DirectX::XMFLOAT3 Vec;
	float dot(Vec a, Vec b)
	{
		DirectX::XMVECTOR va = DirectX::XMLoadFloat3(&a);
		DirectX::XMVECTOR vb = DirectX::XMLoadFloat3(&b);
		DirectX::XMVECTOR vd = DirectX::XMVector3Dot(va, vb);
		Vec d;
		DirectX::XMStoreFloat3(&d,vd);
		return d.x;
	}

	enum Refl_t { DIFF, SPEC, REFR };  // material types, used in radiance()

	struct PureGeometryMaterial
	{
		Vec diffuseAlbedo;
		Vec emission = Vec(0,0,0);
		float fPad0;
		float fPad1;
		int refl;  // 0 for DIFF, 1 for SPEC, 2 for REFR
		int pad0;
		int pad1;
		int pad2;
		PureGeometryMaterial() = default;
		PureGeometryMaterial(Vec c_, int refl_, Vec e_ = Vec(0, 0, 0)) :
			diffuseAlbedo(c_), refl(refl_), emission(e_) {}
	};

	struct Sphere
	{
		float rad;            // radius 
		Vec p;				  // position
		int matIdx;
		int pad0;
		int pad1;
		int pad2;
		Sphere(float rad_, Vec p_, int mi_) :
			rad(rad_), p(p_), matIdx(mi_) {}
	};

	struct Plane
	{
		Vec   n;
		Vec   p0;
		float d;
		int matIdx;
		Plane(Vec n_, Vec p0_, int mi_) : 
			n(n_), p0(p0_), matIdx(mi_)
		{
			d = -(dot(n, p0));
		}
	};
}