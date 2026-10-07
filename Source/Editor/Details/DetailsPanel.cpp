#include "EnginePCH.h"
#include "Editor/Details/DetailsPanel.h"

#include "imgui_internal.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Component/ExponentialHeightFogComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/ParticleSubUVComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/RotatingMovementComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"
#include "Render/Texture2D.h"
#include "Text/Font.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/Actor.h"
#include <cfloat>

namespace
{
	bool DrawAxisControl(const FString& _label, float& _value, float _speed, float _minValue, float _maxValue, float _resetValue, FVector4 _color)
	{
		bool isValueChanged = false;

		float lineHeight = ImGui::GetFrameHeight();
		ImVec4 color(_color.X, _color.Y, _color.Z, _color.W);
		ImVec2 buttonSize = { lineHeight + 2.0f , lineHeight };

		// 각 버튼의 색상 설정
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ color.x - 0.1f, color.y - 0.1f, color.z - 0.1f, 1.0f });

		// 굵은 폰트 적용
		if (ImGui::Button(_label.c_str(), buttonSize))
		{
			_value = _resetValue;
			isValueChanged = true;
		}
		ImGui::PopStyleColor(3);

		// 드래그 슬라이더
		ImGui::SameLine();
		std::string dragID = "##" + _label;
		isValueChanged |= ImGui::DragFloat(dragID.c_str(), &_value, _speed, _minValue, _maxValue, "%.2f");

		return isValueChanged;

	}

	bool DrawVector3Controller(const FString& _label, float* _values, float _resetValue, float _columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

		bool isVectorChanged = false;

		ImGui::PushID(_label.c_str());
		if (ImGui::BeginTable(_label.c_str(), 2, flags)) // 고유 ID, 열 2개, 플래그
		{
			// ImGuiTableColumnFlags_WidthFixed: 초기 너비 고정
			// ImGuiTableColumnFlags_WidthStretch: 창 크기에 따라 너비 조절 (기본값)
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, _columnWidth);
			ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(_label.c_str()); // 왼쪽 열: 레이블

			ImGui::TableSetColumnIndex(1);
			// [컨트롤러 UI 코드] // 오른쪽 열: 컨트롤러
			ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2,0 });
			ImGui::PushFont(boldFont);

			isVectorChanged |= DrawAxisControl("X", _values[0], 0.1f, 0.0f, 0.0f, _resetValue, { 0.8f, 0.1f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Y", _values[1], 0.1f, 0.0f, 0.0f, _resetValue, { 0.1f, 0.8f, 0.1f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::SameLine();
			isVectorChanged |= DrawAxisControl("Z", _values[2], 0.1f, 0.0f, 0.0f, _resetValue, { 0.1f, 0.1f, 0.8f, 1.0f });

			ImGui::PopItemWidth();

			ImGui::PopFont();
			ImGui::PopStyleVar();
			ImGui::EndTable();
		}

		ImGui::Columns(1);

		ImGui::PopID();

		return isVectorChanged;
	}

	// FRotator의 V는 (Pitch, Yaw, Roll) 순서지만 엔진은 Roll=X, Pitch=Y, Yaw=Z축이므로
	// X/Y/Z 칸이 실제 회전축과 맞도록 재배열해 편집한다.
	bool DrawRotatorAsXYZ(const FString& _label, FRotator& _rotator)
	{
		float Euler[3] = { _rotator.Roll, _rotator.Pitch, _rotator.Yaw };

		// UI에서 -0.00으로 표시되는 미세한 회전값은 0으로 표시한다.
		for (float& Angle : Euler)
		{
			if (std::abs(Angle) < 0.005f)
				Angle = 0.0f;
		}

		const bool isChanged = DrawVector3Controller(_label, Euler, 0.0f, 55.0f);
		if (isChanged)
		{
			_rotator.Roll = Euler[0];
			_rotator.Pitch = Euler[1];
			_rotator.Yaw = Euler[2];
		}
		return isChanged;
	}

	FString GetAssetDisplayName(const FString& Path)
	{
		fs::path AssetPath(Path);
		if (AssetPath.stem() == "Atlas")
		{
			return AssetPath.parent_path().filename().string();
		}
		return AssetPath.stem().string();
	}

	void DrawTextureSlot(UMaterial** MaterialPtr, int32 SlotIndex)
	{
		const float ThumbnailSize = 64.0f;

		UMaterial* Material = *MaterialPtr;
		UTexture2D* Texture = nullptr;
		if (Material && SlotIndex < static_cast<int32>(Material->Textures.Num()))
		{
			Texture = Material->Textures[SlotIndex];
		}

		ImGui::PushID(SlotIndex);

		if (Texture && Texture->GetResource())
		{
			ImGui::Image(
				(ImTextureID)Texture->GetResource()->GetSRV(),
				{ ThumbnailSize, ThumbnailSize });
		}
		else
		{
			ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
		}

		// 바로 위 위젯이 드롭을 받는다
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
			{
				UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);

				// 머티리얼이 없으면 기본 머티리얼에서 시작한다
				if (!Material)
				{
					Material = UMaterial::CreateInstance(UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial"));
					*MaterialPtr = Material;
				}
				// 공유 에셋이면 지금 복제한다.
				// 이 시점까지 미루면 손대지 않은 오브젝트는 계속 공유 에셋을 쓴다.
				else if (Material->bIsInstance == false)
				{
					Material = UMaterial::CreateInstance(Material);
					*MaterialPtr = Material;
				}

				if (Material)
				{
					// 슬롯이 배열 범위 밖이면 그 자리까지 늘려 준다
					while (static_cast<int32>(Material->Textures.Num()) <= SlotIndex)
					{
						Material->Textures.Add(nullptr);
					}

					Material->Textures[SlotIndex] = DroppedTexture;
					Texture = DroppedTexture;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		ImGui::BeginGroup();
		ImGui::Text("Slot %d", SlotIndex);

		if (Texture)
		{
			ImGui::TextWrapped("%s", Texture->GetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(비어 있음)");
		}
		ImGui::EndGroup();

		ImGui::PopID();
	}
	void DrawFontSlot(UObject* Owner, UFont** FontPtr)
	{
		UFont* Current = *FontPtr;

		FString CurrentPath = Current ? Current->GetPath() : "";
		FString CurrentFileName = Current ? GetAssetDisplayName(CurrentPath) : "None";

		ImGui::SetNextItemWidth(-1.0f);

		bool bOpen = ImGui::BeginCombo("##Font", CurrentFileName.c_str());
		if (ImGui::IsItemHovered() && Current)
		{
			ImGui::SetTooltip("%s", CurrentPath.c_str());
		}

		if (bOpen)
		{
			for (TObjectIterator<UFont> Itr; Itr; ++Itr)
			{
				bool bSelected = (*Itr == Current);

				FString ItemPath = Itr->GetPath();
				FString ItemFileName = GetAssetDisplayName(ItemPath);

				ImGui::PushID(*Itr);

				if (ImGui::Selectable(ItemFileName.c_str(), bSelected))
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", ItemPath.c_str());
					}

					// 메모리를 직접 바꾸지 않고 SetFont를 거친다
					if (UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(Owner))
						TextComponent->SetFont(*Itr);
					else
						*FontPtr = *Itr;
				}
				ImGui::PopID();
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
	void DrawMeshSlot(UObject* Owner, UStaticMesh** StaticMeshPtr)
	{
		UStaticMesh* Current = *StaticMeshPtr;

		FString CurrentPath = Current ? Current->GetPath() : "";
		FString CurrentFileName = Current ? GetAssetDisplayName(CurrentPath) : "None";

		ImGui::SetNextItemWidth(-1.0f);

		bool bOpen = ImGui::BeginCombo("##StaticMesh", CurrentFileName.c_str());
		if (ImGui::IsItemHovered() && Current)
		{
			ImGui::SetTooltip("%s", CurrentPath.c_str());
		}

		if (bOpen)
		{
			for (TObjectIterator<UStaticMesh> Itr; Itr; ++Itr)
			{
				bool bSelected = (*Itr == Current);

				FString ItemPath = Itr->GetPath();
				FString ItemFileName = GetAssetDisplayName(ItemPath);

				ImGui::PushID(*Itr);

				if (ImGui::Selectable(ItemFileName.c_str(), bSelected))
				{
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", ItemPath.c_str());
					}

					// 메모리를 직접 바꾸지 않고 SetFont를 거친다
					if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Owner))
						MeshComponent->SetStaticMesh(*Itr);
					else
						*StaticMeshPtr = *Itr;
				}
				ImGui::PopID();
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	UMaterial* EnsureMaterialOverride(UMeshComponent* MeshComponent, int32 Slot, UMaterial* Effective, UMaterial* Override)
	{
		// 이 컴포넌트 전용 인스턴스가 아니면 지금 적용 중인 머티리얼을 복제해서 덮어쓰기로 만든다.
		// 메시의 머티리얼은 같은 메시를 쓰는 모든 액터가 공유하므로 직접 고치면 안 된다.
		if (Override && Override->bIsInstance)	return Override;

		UMaterial* Source = Effective ? Effective : UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");

		Override = UMaterial::CreateInstance(Source);
		MeshComponent->SetMaterial(Slot, Override);

		return Override;
	}

	// 메시 컴포넌트의 머티리얼 슬롯을 한 줄씩 그린다.
	// 텍스처를 드롭하면 그 슬롯의 덮어쓰기에 반영하고, 메시 에셋의 머티리얼은 건드리지 않는다.
	void DrawMaterialSlots(UMeshComponent* MeshComponent)
	{
		const int32 NumSlots = MeshComponent->GetNumMaterials();
		if (NumSlots <= 0)
		{
			return;
		}

		if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float ThumbnailSize = 64.0f;

		// 메쉬 내의 매터리얼 별 Slot 순회
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			ImGui::PushID(Slot);

			FString SlotLabel = std::format("[{}] {}", Slot, MeshComponent->GetMaterialSlotName(Slot));
			bool bOpen = ImGui::TreeNode(SlotLabel.c_str());

			UMaterial* Override = MeshComponent->GetOverrideMaterial(Slot);
			UMaterial* Effective = MeshComponent->GetMaterial(Slot);
			UTexture2D* Texture = (Effective && Effective->Textures.Num() > 0) ? Effective->Textures[0] : nullptr;

			ImGui::SameLine();

			if (Override)
			{
				ImGui::TextDisabled("Override");
				ImGui::SameLine();

				if (ImGui::SmallButton("Reset"))
				{
					MeshComponent->SetMaterial(Slot, nullptr);
					Override = nullptr;
					Effective = MeshComponent->GetMaterial(Slot);
				}
			}
			else
			{
				ImGui::TextDisabled("Default");
			}

			if (bOpen)
			{
				if (ImGui::BeginTable("MaterialProperties", 2))
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 110.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Texture");

					ImGui::TableSetColumnIndex(1);

					// 1: 텍스처 썸네일
					if (Texture && Texture->GetResource())
					{
						ImGui::Image((ImTextureID)Texture->GetResource()->GetSRV(), { ThumbnailSize, ThumbnailSize });
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::Button("Empty", { ThumbnailSize, ThumbnailSize });
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
						{
							UTexture2D* DroppedTexture = *static_cast<UTexture2D**>(Payload->Data);
							Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);

							if (Override)
							{
								if (Override->Textures.Num() == 0)
								{
									Override->Textures.Add(nullptr);
								}
								Override->Textures[0] = DroppedTexture;
								Texture = DroppedTexture;
								Effective = Override;
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::SameLine();

					// 2: 텍스처 경로
					if (Texture)
					{
						fs::path Path(Texture->GetPath());
						FString FileName = Path.filename().string();

						ImGui::TextWrapped("%s", FileName.c_str());
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s", Texture->GetPath().c_str());
						}
					}
					else
					{
						ImGui::TextDisabled("(Empty)");
					}

					ImGui::TableNextRow();

					// 3: Base Color
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Base Color");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					FVector4 TempColor = Effective ? Effective->BaseColor : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
					if (ImGui::ColorEdit4("##BaseColor", &TempColor.X))
					{
						Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
						if (Override)
						{
							Override->BaseColor = TempColor;
							Effective = Override;
						}
					}
					
					ImGui::TableNextRow();

					// 4: UV Scroll Speed
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("UV Scroll");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					FVector2 TempUVScrollSpeed = Effective ? Effective->UVScrollSpeed : FVector2();
					if (ImGui::DragFloat2("##UVScrollSpeed", &TempUVScrollSpeed.X, 0.01f))
					{
						Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
						if (Override)
						{
							Override->UVScrollSpeed = TempUVScrollSpeed;
							Effective = Override;
						}
					}

					ImGui::TableNextRow();

					// 5: Sampler State
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Sampler");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					const char* SamplerItems[] = { "Linear Clamp", "Linear Wrap" };
					int SamplerIndex = Effective ? static_cast<int>(Effective->SamplerState) : static_cast<int>(ESamplerState::LinearClamp);
					if (ImGui::BeginCombo("##SamplerState", SamplerItems[SamplerIndex]))
					{
						for (int i = 0; i < static_cast<int>(ESamplerState::Count); ++i)
						{
							if (ImGui::Selectable(SamplerItems[i], SamplerIndex == i))
							{
								Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
								if (Override)
								{
									Override->SamplerState = static_cast<ESamplerState>(i);
									Effective = Override;
								}
							}
						}
						ImGui::EndCombo();
					}

					ImGui::TableNextRow();

					// 6: Blend State
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Blend Mode");

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);

					const char* BlendItems[] = { "Opaque", "Alpha Blend" };
					int BlendIndex = Effective ? static_cast<int>(Effective->BlendState) : static_cast<int>(EBlendState::Opaque);
					if (ImGui::BeginCombo("##BlendState", BlendItems[BlendIndex]))
					{
						for (int i = 0; i < IM_ARRAYSIZE(BlendItems); ++i)
						{
							if (ImGui::Selectable(BlendItems[i], BlendIndex == i))
							{
								Override = EnsureMaterialOverride(MeshComponent, Slot, Effective, Override);
								if (Override)
								{
									Override->BlendState = static_cast<EBlendState>(i);
									Effective = Override;
								}
							}
						}
						ImGui::EndCombo();
					}

					ImGui::EndTable();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();

			ImGui::Separator();
		}
	}

	// UClass에 등록된 프로퍼티를 타입에 맞는 위젯으로 그린다
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont)
	{
		void* ValuePtr = reinterpret_cast<char*>(Object) + Property.Offset;
		const FString Label = "##" + Property.Name;
		bool bIsChanged = false;

		// 좁은 패널에서는 라벨과 입력을 두 줄로 배치한다. 고정 X 좌표는 사용하지 않는다.
		const char* DisplayName = Property.Name == "bTickInEditor" ? "Tick in Editor" : Property.Name.c_str();
		const float RowStart = ImGui::GetCursorPosX();
		const float AvailableWidth = ImGui::GetContentRegionAvail().x;
		const float LabelWidth = AvailableWidth * 0.45f;
		const bool bInline = Property.Type != EPropertyType::Transform && AvailableWidth >= 260.0f &&
							 ImGui::CalcTextSize(DisplayName).x < LabelWidth;
		ImGui::AlignTextToFramePadding();
		ImGui::TextWrapped("%s", DisplayName);
		if (bInline)
		ImGui::SameLine(RowStart + LabelWidth);
		ImGui::SetNextItemWidth(-1.0f);

		switch (Property.Type)
		{
		case EPropertyType::Float:
		{
			const bool bFog = Object->IsA(UExponentialHeightFogComponent::StaticClass());
			const bool bOpacity = Property.Name == "FogMaxOpacity";
			const bool bNonNegativeFogValue = Property.Name == "FogDensity" ||
				Property.Name == "FogHeightFalloff" || Property.Name == "StartDistance" ||
				Property.Name == "FogCutoffDistance";

			if (bFog && (bOpacity || bNonNegativeFogValue))
			{
				bIsChanged = ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr),
					0.1f, 0.0f, bOpacity ? 1.0f : FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			}
			else
			{
				bIsChanged = ImGui::DragFloat(Label.c_str(), static_cast<float*>(ValuePtr), 0.1f);
			}
			break;
		}

		case EPropertyType::Int:
			bIsChanged = ImGui::DragInt(Label.c_str(), static_cast<int*>(ValuePtr), 1.0f);
			break;

		case EPropertyType::Bool:
			bIsChanged = ImGui::Checkbox(Label.c_str(), static_cast<bool*>(ValuePtr));
			if (Property.Name == "bTickInEditor" && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Allow this Actor and its tick-capable components to tick in the Editor. Does not start BeginPlay.");
			}
			break;

		case EPropertyType::Vector:
		{
			FVector* Value = static_cast<FVector*>(ValuePtr);
			bIsChanged = ImGui::DragFloat3(Label.c_str(), Value->V, 0.1f);
			break;
		}
		case EPropertyType::Rotator:
		{
			FRotator* Value = static_cast<FRotator*>(ValuePtr);
			// Transform의 Rotation과 같은 X/Y/Z 축 순서로 보여준다.
			float Euler[3] = { Value->Roll, Value->Pitch, Value->Yaw };
			if (ImGui::DragFloat3(Label.c_str(), Euler, 0.1f))
			{
				Value->Roll = Euler[0];
				Value->Pitch = Euler[1];
				Value->Yaw = Euler[2];
			}
			break;
		}

		case EPropertyType::Vector4:
		{
			FVector4* Value = static_cast<FVector4*>(ValuePtr);
			bIsChanged = ImGui::DragFloat4(Label.c_str(), &Value->X, 0.1f);
			break;
		}

		case EPropertyType::Color:
		{
			// 타입은 Vector4와 같고 위젯만 색상 선택기다
			FVector4* Value = static_cast<FVector4*>(ValuePtr);
			bIsChanged = ImGui::ColorEdit4(Label.c_str(), &Value->X);
			break;
		}
		case EPropertyType::String:
		{
			FString* Value = static_cast<FString*>(ValuePtr);

			char Buffer[256] = {};
			strncpy_s(Buffer, Value->c_str(), sizeof(Buffer) - 1);
			if (CustomFont) ImGui::PushFont(CustomFont);
			if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
			{
				*Value = Buffer;
				bIsChanged = true;
			}
			if (CustomFont) ImGui::PopFont();
			break;
		}
		case EPropertyType::Transform:
		{
			FTransform* Value = static_cast<FTransform*>(ValuePtr);

			ImGui::NewLine();
			bIsChanged |= DrawVector3Controller("Location", Value->Location.V, 0.0f, 55.0f);
			bIsChanged |= DrawRotatorAsXYZ("Rotation", Value->Rotation);
			bIsChanged |= DrawVector3Controller("Scale", Value->Scale.V, 1.0f, 55.0f);
			break;
		}
		case EPropertyType::Object:
		{
			UObject** ObjPtr = static_cast<UObject**>(ValuePtr);

			if (Property.Class == UMaterial::StaticClass())
			{
				DrawTextureSlot(reinterpret_cast<UMaterial**>(ObjPtr), 0);
			}
			else if (Property.Class == UTexture2D::StaticClass())
			{
				UTexture2D** Texture = reinterpret_cast<UTexture2D**>(ObjPtr);
				ImGui::BeginGroup();

				// Content Drawer의 텍스처만 받는다. Sprite 변경은 아래 공통 변경 알림을 거친다.
				const float ThumbnailSize = std::min(64.0f, ImGui::GetContentRegionAvail().x);
				if (*Texture && (*Texture)->GetResource())
				{
					const float MaxDimension = static_cast<float>(std::max((*Texture)->GetWidth(), (*Texture)->GetHeight()));
					const ImVec2 Size(ThumbnailSize * (*Texture)->GetWidth() / MaxDimension,
						ThumbnailSize * (*Texture)->GetHeight() / MaxDimension);
					ImGui::Image((*Texture)->GetResource()->GetSRV(), Size);
				}
				else
					ImGui::Button("Drop Texture", ImVec2(ThumbnailSize, ThumbnailSize));

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::Texture))
					{
						if (Payload->DataSize == sizeof(UTexture2D*))
						{
							*Texture = *static_cast<UTexture2D* const*>(Payload->Data);
							bIsChanged = true;
						}
					}
					ImGui::EndDragDropTarget();
				}

				const FString Preview = *Texture ? GetAssetDisplayName((*Texture)->GetPath()) : "None";
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo(Label.c_str(), Preview.c_str()))
				{
					if (ImGui::Selectable("None", !*Texture))
					{
						*Texture = nullptr;
						bIsChanged = true;
					}
					for (TObjectIterator<UTexture2D> It; It; ++It)
					{
						if (It->GetPath().empty()) continue;
						ImGui::PushID(*It);
						if (ImGui::Selectable(GetAssetDisplayName(It->GetPath()).c_str(), *Texture == *It))
						{
							*Texture = *It;
							bIsChanged = true;
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
				if (*Texture && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", (*Texture)->GetPath().c_str());
				ImGui::EndGroup();
			}
			else if (Property.Class == UFont::StaticClass())
			{
				DrawFontSlot(Object, reinterpret_cast<UFont**>(ObjPtr));
			}
			else if (Property.Class == UStaticMesh::StaticClass())
			{
				DrawMeshSlot(Object, reinterpret_cast<UStaticMesh**>(ObjPtr));
			}
			break;
		}
		default:
			ImGui::TextDisabled("(Unsupported)");
			break;
		}

		if(bIsChanged)
			Object->OnPropertyChanged(Property);
	}

	// 클래스 계층을 따라 올라가며 각 단계의 프로퍼티를 표시
	void DrawProperties(UObject* Object, ImFont* CustomFont)
	{
		if (!Object)
		{
			return;
		}

		TArray<UClass*> ClassChain;
		for (UClass* Class = Object->GetClass(); Class; Class = Class->Super)
		{
			ClassChain.Add(Class);
		}

		const float GroupGap = ImGui::GetStyle().ItemSpacing.y;
		bool bFirstGroup = true;

		// 기반 클래스부터 표시
		for (auto It = ClassChain.rbegin(); It != ClassChain.rend(); ++It)
		{
			UClass* Class = *It;
			if (Class->GetProperties().IsEmpty())
			{
				continue;
			}

			// 상속 단계별 Property 묶음 사이에만 약간의 여백을 둔다.
			if (!bFirstGroup)
			{
				ImGui::Dummy(ImVec2(0.0f, GroupGap));
			}
			bFirstGroup = false;

			ImGui::PushID(Class->Name.c_str());
			for (const FProperty& Property : Class->GetProperties())
			{
				// SubUV 입자는 Sprite 대신 Atlas 머티리얼을 사용한다. 무효한 Sprite 속성은 표시하지 않는다.
				if (Object->IsA<UParticleSubUVComponent>() && Property.Name == "Sprite") continue;
				DrawProperty(Object, Property, CustomFont);
			}
			ImGui::PopID();
		}
	}
}

FDetailsPanel::~FDetailsPanel()
{
	//delete transform;
}

bool FDetailsPanel::Init()
{
	// Add 창을 열기 전에도 저장된 컴포넌트 클래스를 로더가 찾을 수 있어야 한다.
	UBillboardComponent::StaticClass();
	UParticleSubUVComponent::StaticClass();
	USpotLightComponent::StaticClass();
	UPointLightComponent::StaticClass();
	URotatingMovementComponent::StaticClass();
	UProjectileMovementComponent::StaticClass();
	UExponentialHeightFogComponent::StaticClass();

	ImGuiIO& io = ImGui::GetIO();

	io.Fonts->AddFontDefault();

	const char* fontPath = "HMKMRHD.ttf";
	float fontSize = 15.0f;
	const ImWchar* koreanRanges = io.Fonts->GetGlyphRangesKorean();

	CustomFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, nullptr, koreanRanges);
	assert(CustomFont != nullptr);

	return true;
}

void FDetailsPanel::Tick(float DeltaTime)
{
}

void FDetailsPanel::SetActor(AActor* InActor)
{
	TargetActor = InActor;
	SelectedComponent = nullptr;

	// 선택 전환 뒤 이전 Actor의 트리/드래그 요청을 사용하지 않는다.
	ComponentToExpandNextFrame = nullptr;
	PendingDraggedComponent = nullptr;
	PendingAttachParent = nullptr;
}

void FDetailsPanel::SelectComponent(UActorComponent* Component)
{
	if (Component && Component->GetOwner() != TargetActor) return;

	SelectedComponent = Component;
	if (ComponentSelectionCallback) ComponentSelectionCallback(Component);
}

void FDetailsPanel::OnRender()
{
	const float ContentGap = ImGui::GetStyle().ItemSpacing.y;
	const float SectionGap = ContentGap * 2.0f;

	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

	ImGui::Begin("Details");

	if (TargetActor)
	{
		// Component 목록 헤더와 Add Component 버튼
		ImGui::TextUnformatted("Components");
		ImGui::SameLine();

		const float ButtonWidth = 70.0f;
		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ButtonWidth);
		if (ImGui::Button("+ Add", ImVec2(ButtonWidth, 0)))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		ImGui::Separator();

		// 생성 가능한 UActorComponent 파생 클래스를 팝업에 나열한다.
		ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 0.0f),ImVec2(320.0f, 400.0f));
		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			// 클래스 등록이 최초 Actor 생성 여부에 좌우되지 않도록 지원 타입을 준비한다.
			TArray<UClass*> ComponentClasses;
			GetDerivedClasses(UActorComponent::StaticClass(), ComponentClasses);

			for (UClass* Class : ComponentClasses)
			{
				// 추상 클래스처럼 생성자를 제공하지 않는 타입은 제외한다.
				if (Class->Constructor == nullptr) continue;

				if (ImGui::Selectable(Class->Name.c_str()))
				{
					USceneComponent* AttachParent = Cast<USceneComponent>(SelectedComponent);

					if (AActor* Owner = TargetActor)
					{
						UActorComponent* NewComponent = Owner->AddComponentByClass(Class, AttachParent);

						if (NewComponent)
						{
							if (USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent)) 
							{
								// 실제 최종 부모를 펼침
								ComponentToExpandNextFrame = NewSceneComponent->GetAttachParent();
							}
							SelectComponent(NewComponent);
						}
						ImGui::CloseCurrentPopup();
					}
				}
			}
			ImGui::EndPopup();
		}

		ImGui::Dummy(ImVec2(0.0f, ContentGap));

		if (AActor* Owner = TargetActor)
		{
			// SceneComponent의 Attachment 관계를 기준으로 Component Tree를 표시한다.
			// SceneComponent가 아닌 Component는 최상위 항목으로 표시한다.
			if (ImGui::BeginChild("ComponentTree", ImVec2(0.0f, 240.0f), ImGuiChildFlags_Borders))
			{
				DrawComponentTree(Owner);
			}
			ImGui::EndChild();

			ImGui::Dummy(ImVec2(0.0f, SectionGap));
			ImGui::SeparatorText("Properties");
			ImGui::Dummy(ImVec2(0.0f, ContentGap));

			// Component Tree에서 선택된 Component의 프로퍼티를 표시한다.
			UActorComponent* PropertyTarget = SelectedComponent ? SelectedComponent : Owner->GetRootComponent();
			if (PropertyTarget)
			{
				DrawProperties(PropertyTarget, CustomFont);

				// MeshComponent는 Reflection 프로퍼티 외에 Material Slot UI도 추가로 표시한다.
				if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(PropertyTarget))
				{
					DrawMaterialSlots(MeshComponent);
				}
			}
			ImGui::Dummy(ImVec2(0.0f, SectionGap));

			// 컴포넌트 속성과 Actor 전체에 적용되는 속성을 구분한다.
			if (ImGui::CollapsingHeader("Actor", ImGuiTreeNodeFlags_DefaultOpen))
			{
				DrawProperties(Owner, CustomFont);
			}
		}
	}
	ImGui::End();
}

void FDetailsPanel::DrawComponentTree(AActor* Owner)
{
	if (!Owner) return;

	USceneComponent* Root = Owner->GetRootComponent();
	if (Root)
	{
		DrawSceneComponentNode(Root);
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		if (Component->HasAnyFlags(EObjectFlags::RF_Transient))
			continue;
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			// Root 재귀에서 못 잡는 독립 SceneComponent
			if (SceneComponent != Root && SceneComponent->GetAttachParent() == nullptr)
			{
				DrawSceneComponentNode(SceneComponent);
			}
		}
		else
		{
			// SceneComponent가 아닌 일반 ActorComponent
			ImGui::PushID(Component);

			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (Component == SelectedComponent)	Flags |= ImGuiTreeNodeFlags_Selected;

			FString ComponentName = Component->GetFName().ToString();
			bool bOpen = ImGui::TreeNodeEx(ComponentName.c_str(), Flags);
			if (ImGui::IsItemClicked()) SelectComponent(Component);

			ImGui::PopID();
		}
	}

	if (PendingDraggedComponent && PendingAttachParent)
	{
		USceneComponent* DraggedComponent = PendingDraggedComponent;
		USceneComponent* NewParent = PendingAttachParent;

		// 다음 프레임까지 요청이 남지 않도록 먼저 비운다.
		PendingDraggedComponent = nullptr;
		PendingAttachParent = nullptr;

		// Reparent 후에도 World Transform을 유지한다.
		FMatrix OldWorld = DraggedComponent->GetWorldMatrix();
		FMatrix NewParentWorld = NewParent->GetWorldMatrix();
		FMatrix NewLocalMatrix = OldWorld * NewParentWorld.Inverse();

		DraggedComponent->SetupAttachment(NewParent);

		// Self/Cycle 등으로 Attachment가 거부되지 않은 경우에만 Local을 갱신한다.
		if (DraggedComponent->GetAttachParent() == NewParent)
		{
			DraggedComponent->SetTransform(FTransform::FromMatrix(NewLocalMatrix));
		}
	}
}

void FDetailsPanel::DrawSceneComponentNode(USceneComponent* Component)
{
	ImGui::PushID(Component);

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (Component == SelectedComponent)	Flags |= ImGuiTreeNodeFlags_Selected;

	const bool bHasChildren = !Component->GetAttachChildren().IsEmpty();
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	if (ComponentToExpandNextFrame == Component)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		ComponentToExpandNextFrame = nullptr;
	}

	FString ComponentName = Component->GetFName().ToString();
	bool bOpen = ImGui::TreeNodeEx(ComponentName.c_str(), Flags);
	if (ImGui::IsItemClicked()) SelectComponent(Component);
	
	if (ImGui::BeginDragDropSource())
	{
		USceneComponent* DraggedComponent = Component;
		ImGui::SetDragDropPayload(EditorDragDrop::SceneComponent, &DraggedComponent, sizeof(USceneComponent*));

		ImGui::Text("%s", Component->GetName().c_str());

		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(EditorDragDrop::SceneComponent))
		{
			USceneComponent* DraggedComponent = *static_cast<USceneComponent**>(Payload->Data);

			// Tree 순회 중 hierarchy를 수정하지 않고,
			// 순회가 끝난 뒤 처리하도록 요청만 저장한다.
			PendingDraggedComponent = DraggedComponent;
			PendingAttachParent = Component;

			// 드롭된 자식이 바로 보이도록 새 부모를 다음 프레임에 펼친다.
			ComponentToExpandNextFrame = Component;
		}

		ImGui::EndDragDropTarget();
	}

	if (bOpen && bHasChildren)
	{
		for (USceneComponent* ChildComponent : Component->GetAttachChildren())
		{
			DrawSceneComponentNode(ChildComponent);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

