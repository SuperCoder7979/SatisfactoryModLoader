#pragma once

// Satisfactory SDK (V0.1.13 - Build 99427)

#ifdef _MSC_VER
	#pragma pack(push, 0x8)
#endif

#include "FG_Attack_StingerSwipeStrong_structs.hpp"

namespace SDK
{
//---------------------------------------------------------------------------
//Classes
//---------------------------------------------------------------------------

// BlueprintGeneratedClass Attack_StingerSwipeStrong.Attack_StingerSwipeStrong_C
// 0x0000 (0x0050 - 0x0050)
class UAttack_StingerSwipeStrong_C : public UFGAttackMelee
{
public:

	static UClass* StaticClass()
	{
		static auto ptr = UObject::FindClass("BlueprintGeneratedClass Attack_StingerSwipeStrong.Attack_StingerSwipeStrong_C");
		return ptr;
	}

};


}

#ifdef _MSC_VER
	#pragma pack(pop)
#endif