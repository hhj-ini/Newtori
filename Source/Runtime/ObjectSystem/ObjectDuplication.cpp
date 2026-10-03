#include "EnginePCH.h"
#include "ObjectDuplication.h"

AActor* FObjectDuplicator::DuplicateActorToWorld(const AActor* SourceActor, UWorld* DestinationWorld)
{
    if (!SourceActor || !DestinationWorld) return nullptr;

    // 동일한 클래스의 새 Actor를 생성해 소유 관계와 기본 Subobject 구성을 새 World 기준으로 만든다.
    UClass* Class = SourceActor->GetClass();
    FTransform SpawnTransform = SourceActor->GetActorTransform();

    AActor* Actor = DestinationWorld->SpawnActor(Class, NAME_None, &SpawnTransform);
    if (!Actor)
        return nullptr;

    // StaticMeshComponent는 별도 인스턴스를 유지하되 StaticMesh Asset은 원본과 공유한다.
    USceneComponent* SourceRoot = SourceActor->GetRootComponent();
    USceneComponent* DestRoot = Actor->GetRootComponent();

    if (SourceRoot && SourceRoot->IsA(UStaticMeshComponent::StaticClass())
        && DestRoot && DestRoot->IsA(UStaticMeshComponent::StaticClass()))
    {
        UStaticMeshComponent* SourceSMC = static_cast<UStaticMeshComponent*>(SourceRoot);
        UStaticMeshComponent* DestSMC = static_cast<UStaticMeshComponent*>(DestRoot);

        UStaticMesh* SourceMesh = SourceSMC->GetStaticMesh();
        DestSMC->SetStaticMesh(SourceMesh);
    }

    return Actor;
}

UWorld* FObjectDuplicator::DuplicateWorld(const UWorld* SourceWorld)
{
    if (!SourceWorld) return nullptr;

    // 새 World를 초기화한 뒤 SourceWorld의 Level Actor들을 새 World에 각각 복제한다.
    UWorld* DestWorld = FObjectFactory::ConstructObject<UWorld>();
    if (!DestWorld || !DestWorld->Init())
    {
        delete DestWorld;
        return nullptr;
    }

    for (AActor* SourceActor : SourceWorld->GetPersistentLevel()->GetActors())
    {
        if (DuplicateActorToWorld(SourceActor, DestWorld) == nullptr)
        {
            HTR_LOG(Error, "Failed to duplicate Actor while duplicating World");
            delete DestWorld;
            return nullptr;
        }
    }

    return DestWorld;
}