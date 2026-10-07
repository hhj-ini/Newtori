#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "Asset/AssetManager.h"
#include "Serialization/TypeSerializer.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	Sprite = UAssetManager::GetAssetByPath<UTexture2D>("Assets/Editor/Icon/Pawn_64x.png");
}

void UBillboardComponent::SetSprite(UTexture2D* InSprite)
{
	Sprite = InSprite;
	UpdateSpriteMaterial();
	MarkTransformDirty();
}

FVector2 UBillboardComponent::GetSpriteWorldSize() const
{
	const FVector Scale = GetWorldScale3D();
	if (!UsesSpriteMaterial() || !Sprite)
	{
		return FVector2(std::abs(Scale.Y), std::abs(Scale.Z));
	}

	// 원본 해상도는 화면 크기가 아니다. 긴 변을 1 월드 단위로 정규화하고 비율만 유지한다.
	// 부모를 포함한 최대 축 스케일은 적용하되, 고해상도 텍스처로 바꿔도 크기는 같게 한다.
	const float UniformScale = std::max({std::abs(Scale.X), std::abs(Scale.Y), std::abs(Scale.Z)});
	const float MaxDimension = static_cast<float>(std::max({Sprite->GetWidth(), Sprite->GetHeight(), 1u}));
	return FVector2(Sprite->GetWidth() / MaxDimension, Sprite->GetHeight() / MaxDimension) * UniformScale;
}

FBox UBillboardComponent::CalcBounds() const
{
	// View마다 면 방향이 달라지므로 모든 카메라 방향을 포함하는 경계를 사용한다.
	const FVector2 Size = GetSpriteWorldSize();
	const float Radius = std::sqrt(Size.X * Size.X + Size.Y * Size.Y) * 0.5f;
	const FVector Extent(Radius, Radius, Radius);
	return FBox{GetWorldLocation() - Extent, GetWorldLocation() + Extent};
}

void UBillboardComponent::OnRegister()
{
	UpdateSpriteMaterial();
	Super::OnRegister();
}

void UBillboardComponent::OnPropertyChanged(const FProperty& Property)
{
	Super::OnPropertyChanged(Property);
	if (Property.Name == "Sprite")
	{
		UpdateSpriteMaterial();
		MarkTransformDirty();
	}
}

void UBillboardComponent::UpdateSpriteMaterial()
{
	if (!UsesSpriteMaterial())
		return;
	UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	if (!Base)
		return;
	// 공유 머티리얼을 변경하지 않고 컴포넌트별 Sprite를 적용한다.
	if (!Material || !Material->bIsInstance || Material->Shader != Base->Shader)
	{
		const FVector4 Color = Material ? Material->BaseColor : FVector4(1, 1, 1, 1);
		Material = UMaterial::CreateInstance(Base);
		Material->BaseColor = Color;
	}
	if (Material->Textures.IsEmpty())
		Material->Textures.Add(Sprite);
	else
		Material->Textures[0] = Sprite;
}

bool UBillboardComponent::LineTraceComponent(const FRay& WorldRay, FHitResult& OutHit)
{
	if (!QuadMesh || (UsesSpriteMaterial() && !Sprite)) return false;

	FMatrix BillboardMatrix;
	GetWorldTransformedMatrix(&BillboardMatrix);   // 카메라를 향하는, 실제로 그려지는 행 렬
	return TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardMatrix, OutHit);
}

// View별 렌더 행렬을 그대로 사용해 메인 카메라와 다른 방향에서도 같은 면을 선택한다.
bool UBillboardComponent::LineTraceComponentForView(
	const FRay& WorldRay, FHitResult& OutHit, const FMatrix& BillboardWorldMatrix)
{
	return QuadMesh && (!UsesSpriteMaterial() || Sprite) &&
		   TraceMesh(WorldRay, QuadMesh->GetMeshData(), BillboardWorldMatrix, OutHit);
}

// 기본 카메라용 행렬을 구해 공통 렌더 패킷 제출 경로로 전달한다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	// 렌더러가 역참조하므로 둘 중 하나라도 없으면 보내지 않는다
	if (QuadMesh == nullptr || Material == nullptr || (UsesSpriteMaterial() && !Sprite))
	{
		return;
	}

	FMatrix BillboardWorldMatrix;
	GetWorldTransformedMatrix(&BillboardWorldMatrix);
	SubmitToRenderQueue(RenderQueue, BillboardWorldMatrix);
}

// View별 Billboard 행렬과 Material을 렌더 패킷에 담는다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr || Material == nullptr || (UsesSpriteMaterial() && !Sprite))
		return;

	FRenderPacket Packet;
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.Model = RenderQueue.StoreWorldMatrix(BillboardWorldMatrix);
	RenderQueue.Add(Packet);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading && Handle.contains("Sprite") && Handle["Sprite"].is_null())
		Sprite = nullptr;

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (Handle.contains("Material") && Handle["Material"].is_object())
		{
			if (UMaterial* Loaded = UMaterial::LoadMaterial(Handle["Material"]))
			{
				Material = Loaded;   // 못 만들었으면 생성자 기본값 유지
			}
		}
		UpdateSpriteMaterial();
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}

// Quad의 Y/Z 평면을 화면 오른쪽/위쪽에 맞춘다. 렌더링과 피킹이 공유한다.
FMatrix UBillboardComponent::BuildScreenAlignedMatrix(
	const FVector& Position, const FVector& Forward, const FVector& Right, const FVector& Up, float Width, float Height)
{
	FMatrix Matrix;
	Matrix.SetIdentity();
	const FVector Axes[] = {Forward * -1.0f, Right * Width, Up * Height};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Matrix.M[Row][0] = Axes[Row].X;
		Matrix.M[Row][1] = Axes[Row].Y;
		Matrix.M[Row][2] = Axes[Row].Z;
	}
	Matrix.M[3][0] = Position.X;
	Matrix.M[3][1] = Position.Y;
	Matrix.M[3][2] = Position.Z;
	return Matrix;
}

void UBillboardComponent::GetWorldTransformedMatrix(FMatrix* OutWorldMatrix) const
{
	if (!OutWorldMatrix)
		return;
	const UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
	if (!World || !World->GetMainCamera())
	{
		*OutWorldMatrix = GetWorldMatrix();
		return;
	}
	const FTransform& Camera = World->GetMainCamera()->GetCameraComponent()->GetTransform();
	const FVector2 Size = GetSpriteWorldSize();
	*OutWorldMatrix = BuildScreenAlignedMatrix(GetWorldLocation(), Camera.GetForward().Normalized(),
		Camera.GetRight().Normalized(), Camera.GetUp().Normalized(), Size.X, Size.Y);
}
