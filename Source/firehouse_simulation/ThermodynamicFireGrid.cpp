#include "ThermodynamicFireGrid.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"

AThermodynamicFireGrid::AThermodynamicFireGrid() {
  PrimaryActorTick.bCanEverTick = true;

  USceneComponent* DummyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
  RootComponent = DummyRoot;

  GridWidth = 10;
  GridHeight = 10;
  CellSize = 100.0f;
  NumEnvelopLayers = 3;
  EnvelopFireScale = 1.0f;
  ConductionCoefficient = 0.01f;
  RadiativeCoefficient = 0.06f;
  BurnTemperatureRamp = 150.0f;
  FireTimeScale = 0.5f;
}

void AThermodynamicFireGrid::BeginPlay() {
  Super::BeginPlay();
  InitializeGrid();
}

void AThermodynamicFireGrid::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);
  PropagateHeat(DeltaTime);

  // ---- VISUAL DEBUG & PARTICLE SPAWNING ----
  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      int32 CurrentIndex = GetIndex(X, Y);
      if (CurrentIndex == INDEX_NONE) continue;

      // Notice the '&' - we are using a reference so we update the actual array!
      FFireCell& Cell = GridCells[CurrentIndex];
      FColor BoxColor = FColor::White;
      bool bShouldDraw = false;

      if (Cell.Fuel > 0.0f) {
          BoxColor = Cell.bIsOnFire ? FColor::Red : FColor::Green;
          bShouldDraw = true;
      } else if (Cell.Temperature > 25.0f) {
          BoxColor = FColor::Orange;
          bShouldDraw = true;
      }

      // --- SPAWN OR DESTROY PARTICLES ---
      if (Cell.bIsOnFire) {
          // Ground-level fire particle
          if (Cell.FireEffectComponent == nullptr && FireParticleSystem != nullptr) {
              Cell.FireEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                  GetWorld(),
                  FireParticleSystem,
                  Cell.Location,
                  FRotator::ZeroRotator
              );
          }

          // --- ENVELOPING FIRE: Spawn layered fire components attached to furniture ---
          if (Cell.EnvelopFireComponents.Num() == 0 && Cell.FurnitureActor != nullptr && FireParticleSystem != nullptr) {
              float Height = FMath::Max(Cell.FurnitureHeight, 50.0f);
              for (int32 Layer = 0; Layer < NumEnvelopLayers; ++Layer) {
                  // Evenly distribute layers from the base to the top of the furniture
                  float ZOffset = (Height / (float)NumEnvelopLayers) * (Layer + 0.5f);
                  FVector SpawnOffset = FVector(0.0f, 0.0f, ZOffset);

                  UNiagaraComponent* EnvelopComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
                      FireParticleSystem,
                      Cell.FurnitureActor->GetRootComponent(),
                      NAME_None,
                      SpawnOffset,
                      FRotator::ZeroRotator,
                      EAttachLocation::KeepRelativeOffset,
                      true  // bAutoDestroy
                  );

                  if (EnvelopComp) {
                      // Scale down upper layers initially — they'll intensify as temperature climbs
                      float LayerScale = EnvelopFireScale * (0.4f + 0.6f * ((float)(Layer + 1) / (float)NumEnvelopLayers));
                      EnvelopComp->SetWorldScale3D(FVector(LayerScale));

                      // Set initial FireIntensity to 0 so it fades in with temperature
                      EnvelopComp->SetFloatParameter(TEXT("FireIntensity"), 0.0f);

                      Cell.EnvelopFireComponents.Add(EnvelopComp);
                  }
              }
          }

          // --- ENVELOPING FIRE: Update FireIntensity each frame based on temperature ---
          if (Cell.EnvelopFireComponents.Num() > 0) {
              // Map temperature (150–800) to intensity (0.0–1.0)
              float BaseIntensity = FMath::Clamp((Cell.Temperature - 150.0f) / 650.0f, 0.0f, 1.0f);
              for (int32 Layer = 0; Layer < Cell.EnvelopFireComponents.Num(); ++Layer) {
                  if (Cell.EnvelopFireComponents[Layer] != nullptr) {
                      // Lower layers reach full intensity sooner; upper layers lag behind
                      float LayerFraction = (float)(Layer + 1) / (float)Cell.EnvelopFireComponents.Num();
                      float LayerIntensity = FMath::Clamp(BaseIntensity / LayerFraction, 0.0f, 1.0f);
                      Cell.EnvelopFireComponents[Layer]->SetFloatParameter(TEXT("FireIntensity"), LayerIntensity);
                  }
              }
          }

          // --- SPAWN CHAR DECAL ---
          if (Cell.CharDecalComponent == nullptr && CharDecalMaterial != nullptr) {
          
            // Dynamically calculate the decal width/length to slightly exceed the cell size for seamless coverage
            float DecalExtent = (CellSize / 2.0f) * 1.05f;

            Cell.CharDecalComponent = UGameplayStatics::SpawnDecalAtLocation(
                GetWorld(),
                CharDecalMaterial,
                FVector(500.0f, DecalExtent, DecalExtent), // Now it scales automatically!
                Cell.Location + FVector(0.0f, 0.0f, 400.0f),
                FRotator(-90.0f, 0.0f, 0.0f),
                0.0f
            );
          
            Cell.DecalMaterialInstance = Cell.CharDecalComponent->CreateDynamicMaterialInstance();
          }
      } else {
          // If it is no longer on fire (burned out), but still has particles running, destroy them!
          if (Cell.FireEffectComponent != nullptr) {
              Cell.FireEffectComponent->DestroyComponent();
              Cell.FireEffectComponent = nullptr;
          }

          // --- ENVELOPING FIRE: Destroy all enveloping fire components ---
          for (UNiagaraComponent* EnvelopComp : Cell.EnvelopFireComponents) {
              if (EnvelopComp != nullptr) {
                  EnvelopComp->DestroyComponent();
              }
          }
          Cell.EnvelopFireComponents.Empty();
      }

            // --- NEW: UPDATE DECAL INTENSITY ---
            if (Cell.DecalMaterialInstance != nullptr) {
                // Map the temperature (150 to 500) to a 0.0 to 1.0 percentage
                float FadePercentage = FMath::Clamp((Cell.Temperature - 150.0f) / (350.0f), 0.0f, 1.0f);
                  
                // Send that percentage directly into the Material graph!
                Cell.DecalMaterialInstance->SetScalarParameterValue(TEXT("BurnIntensity"), FadePercentage);
            }

            // --- DRAW DEBUG SHAPES ---
            if (bShouldDraw) {
                DrawDebugBox(GetWorld(), Cell.Location + FVector(0.0f, 0.0f, 5.0f),
                             FVector(CellSize * 0.48f, CellSize * 0.48f, 5.0f),
                             BoxColor, false, -1.0f, 0, 1.5f);

                FString IndexText = FString::FromInt(CurrentIndex);
                FVector TextLocation = Cell.Location + FVector(0.0f, 0.0f, 15.0f);
                
                DrawDebugString(GetWorld(), TextLocation, IndexText, nullptr, BoxColor, 0.0f, true, 1.2f);
            }
          } // End of X loop
        } // End of Y loop
      } // End of Tick function

void AThermodynamicFireGrid::InitializeGrid() {
  GridCells.Empty();
  FVector ActorLocation = GetActorLocation();

  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      FFireCell NewCell;
      NewCell.Location = ActorLocation + FVector(X * CellSize, Y * CellSize, 5.0f);

      NewCell.Fuel = 0.0f;
      NewCell.Temperature = 20.0f;
      NewCell.bIsOnFire = false;

      FHitResult HitResult;
      FVector TraceStart = NewCell.Location + FVector(0.0f, 0.0f, 200.0f);
      FVector TraceEnd = NewCell.Location - FVector(0.0f, 0.0f, 10.0f);
      FCollisionQueryParams QueryParams;
      QueryParams.AddIgnoredActor(this);

      if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams)) {
          AActor *HitActor = HitResult.GetActor();
          if (HitActor) {
            FString ActorName = HitActor->GetName().ToLower();
            if (!ActorName.Contains(TEXT("floor")) && !ActorName.Contains(TEXT("ground"))) {
              
                // 1. FAST BURN: Fabric (Sofa, Rugs) - Lower fuel pool, burns up fast
                if (HitActor->ActorHasTag(TEXT("Fabric"))) {
                    NewCell.Fuel = 150.0f;
                    NewCell.BurnRate = 2.5f;
                }
                // 2. SLOW BURN: Hardwood (Tables, Bookshelves) - Massive fuel pool, burns very slowly
                else if (HitActor->ActorHasTag(TEXT("Wood")) || HitActor->ActorHasTag(TEXT("Hardwood"))) {
                    NewCell.Fuel = 400.0f;
                    NewCell.BurnRate = 0.3f;
                }
                // 3. DEFAULT: Generic Flammable objects
                else if (HitActor->ActorHasTag(TEXT("Flammable"))) {
                    NewCell.Fuel = 200.0f;
                    NewCell.BurnRate = 1.0f;
                }

                // --- ENVELOPING FIRE: Store the furniture actor and compute its height ---
                if (NewCell.Fuel > 0.0f) {
                    NewCell.FurnitureActor = HitActor;
                    FVector Origin, BoxExtent;
                    HitActor->GetActorBounds(false, Origin, BoxExtent);
                    NewCell.FurnitureHeight = BoxExtent.Z * 2.0f;
                }
            }
          }
      }

      GridCells.Add(NewCell);
    }
  }
  // Ignite a cell near the middle (Index 55) so we can watch it spread
  if (GridCells.Num() > 31) {
    GridCells[31].Temperature = 500.0f;
    GridCells[31].bIsOnFire = true;
    GridCells[31].Fuel = 200.0f;
  }
}

void AThermodynamicFireGrid::TriggerIgnitionAtLocation(FVector WorldLocation, float IgnitionTemperature, float ForcedFuel) {
    FVector ActorLocation = GetActorLocation();
    
    int32 X = FMath::FloorToInt((WorldLocation.X - ActorLocation.X) / CellSize);
    int32 Y = FMath::FloorToInt((WorldLocation.Y - ActorLocation.Y) / CellSize);
    
    int32 TargetIndex = GetIndex(X, Y);
    
    if (TargetIndex != INDEX_NONE) {
        GridCells[TargetIndex].Temperature = IgnitionTemperature;
        GridCells[TargetIndex].bIsOnFire = true;
        
        if (GridCells[TargetIndex].Fuel <= 0.0f) {
            GridCells[TargetIndex].Fuel = ForcedFuel;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("Manual ignition triggered successfully at Cell Index: %d (X: %d, Y: %d)"), TargetIndex, X, Y);
    }
}

void AThermodynamicFireGrid::PropagateHeat(float DeltaTime) {
  DeltaTime *= FireTimeScale;
  TArray<FFireCell> NextGridCells = GridCells;

  for (int32 Y = 0; Y < GridHeight; ++Y) {
    for (int32 X = 0; X < GridWidth; ++X) {
      int32 CurrentIndex = GetIndex(X, Y);
      if (CurrentIndex == INDEX_NONE) continue;

      if (GridCells[CurrentIndex].Fuel <= 0.0f && GridCells[CurrentIndex].Temperature <= 20.1f)
        continue;

      // --- RADIATIVE HEAT LOGIC ---
      float ConductionHeat = 0.0f;
      float MaxRadiativeHeat = 0.0f;

      // Check a 9x9 grid around the cell (±4 tiles) for heat radiation
      for (int32 dy = -4; dy <= 4; ++dy) {
        for (int32 dx = -4; dx <= 4; ++dx) {
          if (dx == 0 && dy == 0) continue; // Skip self

          int32 NeighborIndex = GetIndex(X + dx, Y + dy);
          if (NeighborIndex != INDEX_NONE) {
            
            // Only absorb heat if the neighbor is HOTTER than this cell
            if (GridCells[NeighborIndex].Temperature > GridCells[CurrentIndex].Temperature) {
                float DistSq = (float)(dx * dx + dy * dy);
                float Distance = FMath::Sqrt(DistSq);
                
                // Base conduction: inverse-distance falloff (summed from all neighbors)
                float ConductionWeight = 1.0f / Distance;
                ConductionHeat += (GridCells[NeighborIndex].Temperature - GridCells[CurrentIndex].Temperature) * ConductionWeight * ConductionCoefficient * DeltaTime;
                
                // Radiative boost: only keep the STRONGEST source (prevents additive snowball)
                if (GridCells[NeighborIndex].bIsOnFire) {
                    float RadiativeWeight = 1.0f / DistSq;
                    float RadiativeHeat = GridCells[NeighborIndex].Temperature * RadiativeWeight * RadiativeCoefficient * DeltaTime;
                    MaxRadiativeHeat = FMath::Max(MaxRadiativeHeat, RadiativeHeat);
                }
            }
          }
        }
      }

      // Combine: conduction stacks naturally, radiation uses only the dominant source
      float HeatGained = ConductionHeat + MaxRadiativeHeat;

      // Cap heat gained per frame to prevent exponential snowball spread
      HeatGained = FMath::Min(HeatGained, 50.0f * DeltaTime);

      // Apply the accumulated heat directly
      NextGridCells[CurrentIndex].Temperature += HeatGained;
      // --- END RADIATIVE HEAT LOGIC ---

      if (GridCells[CurrentIndex].bIsOnFire) {
        NextGridCells[CurrentIndex].Temperature += BurnTemperatureRamp * DeltaTime;
        if (NextGridCells[CurrentIndex].Temperature > 800.0f) {
          NextGridCells[CurrentIndex].Temperature = 800.0f;
        }

          NextGridCells[CurrentIndex].Fuel -= GridCells[CurrentIndex].BurnRate * DeltaTime;
        if (NextGridCells[CurrentIndex].Fuel <= 0.0f) {
          NextGridCells[CurrentIndex].Fuel = 0.0f;
          NextGridCells[CurrentIndex].bIsOnFire = false;
        }
      } else {
        if (NextGridCells[CurrentIndex].Temperature > 20.0f) {
          NextGridCells[CurrentIndex].Temperature -= 2.0f * DeltaTime;
        }
        if (GridCells[CurrentIndex].Temperature > 150.0f && GridCells[CurrentIndex].Fuel > 0.0f) {
          NextGridCells[CurrentIndex].bIsOnFire = true;
        }
      }
    }
  }
  GridCells = NextGridCells;
}

int32 AThermodynamicFireGrid::GetIndex(int32 X, int32 Y) const {
  if (X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight) {
    return X + (Y * GridWidth);
  }
  return INDEX_NONE;
}
