#include "../../CSEL/runetime/Internal/Interfaces/QueryInterface/QueryInterface.h"
#include "../../CSEL/runetime/Internal/Plugin/PluginInfo.h"
#include "../../CSEL/runetime/Internal/Plugin/API.h"

#include "../../CSEL/source/A/Actor.h"
#include "../../CSEL/source/B/BSFixedString.h"
#include "../../CSEL/source/B/BSTEvent.h"
#include "../../CSEL/source/T/TESMagicEffectApplyEvent.h"
#include "../../CSEL/source/T/TESDataHandler.h"
#include "../../CSEL/source/S/ScriptEventSourceHolder.h"

#include "../../OrbisUtil/include/Logger.h"
#include "../../OrbisUtil/include/Relocation.h"

#include "../../OrbisUtil/Third-Party/brofield/1.0/SimpleIni.h"
#include "../../OrbisUtil/Third-Party/herumi/xbayk/6.00/xbyak.h"

// required by OrbisUtil.
void SetPath(const char** a_log, const char** a_mira, const char** a_data, const char** a_appdata) { }

static bool isActivate = false;
static bool isParagliding = false;
static float progression = 0.0f;
static float start = 0.0f;

class MagicEffectApplyEventHandler : public ConsoleRE::BSTEventSink<ConsoleRE::TESMagicEffectApplyEvent> 
{
public:
	static MagicEffectApplyEventHandler* GetSingleton() 
	{
		static MagicEffectApplyEventHandler singleton;
		return &singleton;
	}

	ConsoleRE::BSEventNotifyControl ProcessEvent(ConsoleRE::TESMagicEffectApplyEvent* a_event, ConsoleRE::BSTEventSource<ConsoleRE::TESMagicEffectApplyEvent>* a_eventSource) override
	{
		static ConsoleRE::EffectSetting* notRevalisGale = NULL;
		static ConsoleRE::TESDataHandler* dataHandle = NULL;

		if (!dataHandle) 
		{  
			dataHandle = ConsoleRE::TESDataHandler::GetSingleton();
			if (dataHandle) 
			{
				notRevalisGale = dataHandle->LookupForm<ConsoleRE::EffectSetting>(0x10C68, "Paragliding.esp");
			}
		};

		if (!a_event) 
		{
			return ConsoleRE::BSEventNotifyControl::kContinue;
		}

		if (a_event->magicEffect == notRevalisGale->FormID) 
		{
			start = 0.00f;
			progression = 0.00f;
		}

		return ConsoleRE::BSEventNotifyControl::kContinue;
	}
protected:
	MagicEffectApplyEventHandler() = default;
	MagicEffectApplyEventHandler(const MagicEffectApplyEventHandler&) = delete;
	MagicEffectApplyEventHandler(MagicEffectApplyEventHandler&&) = delete;
	virtual ~MagicEffectApplyEventHandler() = default;
	auto operator=(const MagicEffectApplyEventHandler&)->MagicEffectApplyEventHandler & = delete;
	auto operator=(MagicEffectApplyEventHandler&&)->MagicEffectApplyEventHandler & = delete;
};

class Loki_Paraglider
{
public:
	float FallSpeed, GaleSpeed;

	Loki_Paraglider()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile("/app0/data/CSEL/Paraglider.ini");

		FallSpeed = (float)ini.GetDoubleValue("SETTINGS", "fFallSpeed", 0.00f);
		GaleSpeed = (float)ini.GetDoubleValue("SETTINGS", "fGaleSpeed", 0.00f);
	}

	static void* CodeAllocation(Xbyak::CodeGenerator& a_code, Trampoline::Trampoline* t_ptr)
	{
		auto result = t_ptr->Take(a_code.getSize());		
		memcpy(result, a_code.getCode(), a_code.getSize());
		return result;
	}

	static float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	static void InstallActivateTrue()
	{
		Relocation<uintptr_t> target("ActivateHandler:3", 0x866C50);
		target += 0x87;

		Relocation<uintptr_t>   addr("ActivateHandler:3", 0x866C50);
		addr += 0x166;

		{
			struct Patch : Xbyak::CodeGenerator
			{
				Patch(uintptr_t a_var, uintptr_t a_target)
				{

					Xbyak::Label ourJmp;
					Xbyak::Label ActivateIsTrue;

					mov(byte[r14 + 0xB], 0x1);

					push(rax);
					mov(rax, (uintptr_t)&isActivate);
					cmp(byte[rax], 0x1);

					je(ActivateIsTrue);
					mov(byte[rax], 0x1);
					pop(rax);
					jmp(ptr[rip + ourJmp]);

					L(ActivateIsTrue);
					mov(byte[rax], 0x0);
					pop(rax);
					jmp(ptr[rip + ourJmp]);

					L(ourJmp);
					dq(a_var);

				};

			};

			Patch patch(addr.address(), target.address());
			patch.ready();

			auto& trampoline = API::GetTrampoline();
			trampoline.WriteJMP<5>(target.address(), Loki_Paraglider::CodeAllocation(patch, &trampoline));
		}
	};

	static void InstallParagliderWatcher()
	{
		Relocation<uintptr_t> ActorUpdate("", 0x7E1910);

		auto& trampoline = API::GetTrampoline();
		_Paraglider = trampoline.WriteCall<5>(ActorUpdate.address() + 0xEC0, Paraglider);
	};

	static void AddMGEFApplyEventSink() 
	{
		auto sourceHolder = ConsoleRE::ScriptEventSourceHolder::GetSingleton();
		if (sourceHolder) 
		{ 
			sourceHolder->AddEventSink(MagicEffectApplyEventHandler::GetSingleton()); 
		}
	}

private:
	static void Paraglider(ConsoleRE::Actor* a_this)
	{
		_Paraglider(a_this);

		static Loki_Paraglider* lp = NULL;
		if (!lp)
		{
			lp = new Loki_Paraglider();
		}

		static ConsoleRE::EffectSetting* notRevalisGale = NULL;
		static ConsoleRE::TESDataHandler* dataHandle = NULL;
		if (!dataHandle)
		{
			dataHandle = ConsoleRE::TESDataHandler::GetSingleton();
			if (dataHandle) 
			{
				notRevalisGale = dataHandle->LookupForm<ConsoleRE::EffectSetting>(0x10C68, "Paragliding.esp");
			}
		};

		if (isActivate)
		{
			int hasIt;
			const ConsoleRE::BSFixedString startPara = "StartPara";
			a_this->GetGraphVariableImpl("hasparaglider", hasIt);
			if (hasIt)
			{
				if (a_this->NotifyAnimationGraph(startPara))
				{
					isParagliding = true;
				}

				if (isParagliding)
				{
					ConsoleRE::hkVector4 hkv;

					a_this->GetCharController()->GetLinearVelocityImpl(hkv);
					if (start == 0.0f)
					{
						start = hkv.quad[2];
					}

					float dest = lp->FallSpeed;
					if (a_this->HasMagicEffect(notRevalisGale)) 
					{
						dest = lp->GaleSpeed;
					}

					auto a_result = Loki_Paraglider::lerp(start, dest, progression);
					if (progression < 1.00f)
					{
						(false) ? progression += 0.01f : progression += 0.025f;
						(a_this->HasMagicEffect(notRevalisGale)) ? progression += 0.01f : progression += 0.025f;
					}

					hkv.quad[2] = a_result;
					a_this->GetCharController()->SetLinearVelocityImpl(hkv);
				}
				
				if (a_this->GetCharController()->context.currentState == 0)
				{
					isParagliding = false;
					isActivate = false;
				}
			}
		}
		else
		{
			isParagliding = false;
			const ConsoleRE::BSFixedString endPara = "EndPara";
			if (a_this->NotifyAnimationGraph(endPara))
			{
				ConsoleRE::hkVector4 hkv;
				a_this->GetCharController()->GetPositionImpl(hkv, false);
				hkv.quad[2] /= 0.0142875f;
				a_this->GetCharController()->fallStartHeight = hkv.quad[2];
				a_this->GetCharController()->fallTime = 0.00f;
			}

			progression = 0.00f;
			start = 0.00f;
			return;
		}
		return;
	};

	static inline Relocation<decltype(&Paraglider)> _Paraglider;
};

EXPORT bool Query(const Interface::QueryInterface* a_interface, PluginInfo* a_info)
{
	auto* g_log = Log::Log::GetSingleton();
	g_log->OpenRelitive(OrbisFileSystem::Download, "paraglider/Paraglider.log");
	g_log->Write("Paraglider v1.0.0");

	a_info->SetPluginName("Paraglider");
	a_info->SetPluginVersion(1);
	
	return true;
}

EXPORT bool Load(Interface::QueryInterface* a_interface)
{
	Log::Log::GetSingleton()->Write("Paraglider loaded");
	Log::Log::GetSingleton()->Close();

	API::initialize(a_interface);
	API::AllocateTrampoline(128);

	Loki_Paraglider::InstallActivateTrue();
	Loki_Paraglider::InstallParagliderWatcher();
	Loki_Paraglider::AddMGEFApplyEventSink();
	
	return true;
}

EXPORT bool Revert()
{
	return false;
}