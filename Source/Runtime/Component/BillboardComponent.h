#pragma once

#include "PrimitiveComponent.h"

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Sprite)
	REFLECT_END()

public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override = default;
	UTexture2D* GetSprite() const { return Sprite; }
	void SetSprite(UTexture2D* InSprite);
	FVector2 GetSpriteWorldSize() const;
	FBox CalcBounds() const override;
	void OnPropertyChanged(const FProperty& Property) override;
	static FMatrix BuildScreenAlignedMatrix(const FVector& Position, const FVector& Forward, const FVector& Right,
		const FVector& Up, float Width, float Height);

	virtual bool LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit) override;
	// 클릭한 View의 실제 렌더 행렬로 Quad Mesh 교차를 판정한다.
	bool LineTraceComponentForView(const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix);

	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override { return SlotIndex == 0 ? Material : nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override { if (SlotIndex == 0) Material = InMaterial; }

	virtual const FStaticMeshData* GetMeshData() const override { return QuadMesh ? &QuadMesh->GetMeshData() : nullptr; }

	void GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const;
	// 기본 카메라 기준 월드 행렬로 Billboard 렌더 패킷을 제출한다.
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;
	// View별 Adapter가 계산한 Billboard 행렬을 사용해 같은 렌더 패킷 형식으로 제출한다.
	void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix);

	virtual void Serialize(json& Handle, bool bIsLoading) override;

protected:
	void OnRegister() override;
	virtual bool UsesSpriteMaterial() const { return true; }
	UTexture2D* Sprite = nullptr;
	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;
private:
	void UpdateSpriteMaterial();
};
