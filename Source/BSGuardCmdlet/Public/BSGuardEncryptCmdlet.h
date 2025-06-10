// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BSGuardEncryptCmdlet.generated.h"

/**
 * 
 */
UCLASS()
class BSGUARDCMDLET_API UBSGuardEncryptCmdlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UBSGuardEncryptCmdlet();
	~UBSGuardEncryptCmdlet();
	// 主执行函数
	virtual int32 Main(const FString& Params) override;
	TSharedPtr<class FBSGuardSettings> BSGuardSettings;
};
