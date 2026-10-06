class AActor;
class UWorld;

class FObjectDuplicator
{
public:
	// 원본 Actor의 클래스와 상태를 대상 World에 복제한다.
	static AActor* DuplicateActorToWorld(const AActor* SourceActor, UWorld* DestinationWorld);

	// 원본 World의 Level Actor들을 복제한 새로운 World를 생성한다.
	static UWorld* DuplicateWorld(const UWorld* SourceWorld);
};