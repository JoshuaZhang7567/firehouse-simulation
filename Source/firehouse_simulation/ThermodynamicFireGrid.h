#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "ThermodynamicFireGrid.generated.h"

USTRUCT(BlueprintType)
struct FFireCell {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  FVector Location;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float Temperature;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float Fuel;

  // NEW: Every cell tracks how fast it should burn
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  float BurnRate;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermodynamics")
  bool bIsOnFire;

  UPROPERTY()
  UNiagaraComponent* FireEffectComponent;
    
  UPROPERTY()
  UDecalComponent* CharDecalComponent;

  // --- NEW: Store the dynamic material ---
  UPROPERTY()
  UMaterialInstanceDynamic* DecalMaterialInstance;

  // --- Enveloping fire: reference to the furniture actor this cell overlaps ---
  UPROPERTY()
  AActor* FurnitureActor;

  // --- Enveloping fire: Niagara components attached at different heights ---
  UPROPERTY()
  TArray<UNiagaraComponent*> EnvelopFireComponents;

  // --- Enveloping fire: cached bounding box height of the furniture ---
  UPROPERTY()
  float FurnitureHeight;

  // Don't forget to add it to the constructor to prevent the nullptr trap!
  FFireCell()
      : Location(FVector::ZeroVector), Temperature(20.0f), Fuel(0.0f), BurnRate(0.0f),
        bIsOnFire(false), FireEffectComponent(nullptr), CharDecalComponent(nullptr),
        DecalMaterialInstance(nullptr), FurnitureActor(nullptr), FurnitureHeight(0.0f) {}
};

UCLASS(Blueprintable, BlueprintType)
class FIREHOUSE_SIMULATION_API AThermodynamicFireGrid : public AActor {
  GENERATED_BODY()

public:
  AThermodynamicFireGrid();

protected:
  virtual void BeginPlay() override;

public:
  virtual void Tick(float DeltaTime) override;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  int32 GridWidth;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  int32 GridHeight;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  float CellSize;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  UNiagaraSystem *FireParticleSystem;
    
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  UMaterialInterface* CharDecalMaterial;

  // --- Enveloping fire config ---
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "1", ClampMax = "10"))
  int32 NumEnvelopLayers;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "0.1", ClampMax = "5.0"))
  float EnvelopFireScale;

  // --- Heat transfer tuning ---
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "0.001", ClampMax = "0.1"))
  float ConductionCoefficient;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "0.01", ClampMax = "0.5"))
  float RadiativeCoefficient;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "10.0", ClampMax = "500.0"))
  float BurnTemperatureRamp;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config", meta = (ClampMin = "0.1", ClampMax = "2.0"))
  float FireTimeScale;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire Grid Config")
  TArray<FFireCell> GridCells;

  // Manual gameplay/testing trigger to spark cell indices by 3D coordinates
  UFUNCTION(BlueprintCallable, Category = "Fire Grid Interaction")
  void TriggerIgnitionAtLocation(FVector WorldLocation, float IgnitionTemperature = 500.0f, float ForcedFuel = 150.0f);

private:
  void InitializeGrid();
  void PropagateHeat(float DeltaTime);
  int32 GetIndex(int32 X, int32 Y) const;
};
