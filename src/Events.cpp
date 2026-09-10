#include "Events.h"
#include "ActorManager.h"
#include "hdtSkyrimPhysicsWorld.h"

namespace Events
{
	namespace LoadGuard
	{
		namespace
		{
			constexpr std::uint32_t kPostLoadFrameDelay = 3;

			std::mutex g_stateLock;
			std::atomic_bool g_loading{ false };
			std::atomic_bool g_worldReady{ true };
			std::atomic_bool g_loadingMenuOpen{ false };
			std::atomic_bool g_preLoadGame{ false };
			std::atomic_bool g_postLoadGame{ false };
			std::atomic_uint32_t g_framesUntilRelease{ 0 };
			std::atomic_bool g_suppressedFrameLogEmitted{ false };

			void beginLoadLocked(std::string_view reason)
			{
				if (!g_loading.exchange(true)) {
					g_worldReady.store(false);
					g_loadingMenuOpen.store(false);
					g_preLoadGame.store(false);
					g_postLoadGame.store(false);
					g_framesUntilRelease.store(0);
					g_suppressedFrameLogEmitted.store(false);
					logger::debug("HDT load guard entered ({})", reason);
				}
			}

			void armReleaseLocked()
			{
				if (!g_loadingMenuOpen.load() && (!g_preLoadGame.load() || g_postLoadGame.load())) {
					g_framesUntilRelease.store(kPostLoadFrameDelay);
				}
			}
		}

		void LoadingMenuOpened()
		{
			std::lock_guard lock(g_stateLock);
			beginLoadLocked("Loading Menu");
			g_loadingMenuOpen.store(true);
			g_framesUntilRelease.store(0);
		}

		void LoadingMenuClosed()
		{
			std::lock_guard lock(g_stateLock);
			if (!g_loading.load()) {
				return;
			}

			g_loadingMenuOpen.store(false);
			armReleaseLocked();
		}

		void PreLoadGame()
		{
			std::lock_guard lock(g_stateLock);
			beginLoadLocked("kPreLoadGame");
			g_preLoadGame.store(true);
			g_postLoadGame.store(false);
			g_framesUntilRelease.store(0);
		}

		void PostLoadGame()
		{
			std::lock_guard lock(g_stateLock);
			beginLoadLocked("kPostLoadGame");
			g_postLoadGame.store(true);
			logger::debug("HDT post-load received; waiting for a safe main-thread point");
			armReleaseLocked();
		}

		bool IsBlocked()
		{
			return g_loading.load() || !g_worldReady.load();
		}

		bool ConsumeSuppressedFrameLog()
		{
			return !g_suppressedFrameLogEmitted.exchange(true);
		}

		void AdvanceMainThreadFrame()
		{
			std::lock_guard lock(g_stateLock);
			if (!g_loading.load()) {
				return;
			}

			auto frames = g_framesUntilRelease.load();
			if (frames == 0 || g_loadingMenuOpen.load() || (g_preLoadGame.load() && !g_postLoadGame.load())) {
				return;
			}

			if (g_framesUntilRelease.fetch_sub(1) == 1) {
				g_worldReady.store(true);
				g_loading.store(false);
				logger::debug("HDT load guard released after deferred post-load frames");
			}
		}

	}

	namespace Sources
	{
		FrameEventSource* FrameEventSource::GetSingleton()
		{
			static FrameEventSource singleton;
			return std::addressof(singleton);
		}

		FrameSyncEventSource* FrameSyncEventSource::GetSingleton()
		{
			static FrameSyncEventSource singleton;
			return std::addressof(singleton);
		}

		ShutdownEventEventSource* ShutdownEventEventSource::GetSingleton()
		{
			static ShutdownEventEventSource singleton;
			return std::addressof(singleton);
		}

		SkinAllHeadGeometryEventSource* SkinAllHeadGeometryEventSource::GetSingleton()
		{
			static SkinAllHeadGeometryEventSource singleton;
			return std::addressof(singleton);
		}

		SkinSingleHeadGeometryEventSource* SkinSingleHeadGeometryEventSource::GetSingleton()
		{
			static SkinSingleHeadGeometryEventSource singleton;
			return std::addressof(singleton);
		}

		ArmorAttachEventSource* ArmorAttachEventSource::GetSingleton()
		{
			static ArmorAttachEventSource singleton;
			return std::addressof(singleton);
		}

		ArmorDetachEventSource* ArmorDetachEventSource::GetSingleton()
		{
			static ArmorDetachEventSource singleton;
			return std::addressof(singleton);
		}
	}

	namespace Sinks
	{
		FreezeEventHandler* FreezeEventHandler::GetSingleton()
		{
			static FreezeEventHandler singleton;
			return std::addressof(singleton);
		}

		void FreezeEventHandler::Register()
		{
			RE::UI::GetSingleton()->AddEventSink(GetSingleton());
		}

		void FreezeEventHandler::Unregister()
		{
			RE::UI::GetSingleton()->RemoveEventSink(GetSingleton());
		}

		RE::BSEventNotifyControl FreezeEventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource)
		{
			if (a_event && a_event->opening && a_event->menuName == "Loading Menu") {
				LoadGuard::LoadingMenuOpened();
				hdt::SkyrimPhysicsWorld::get()->suspend(true);
			}

			if (a_event && !a_event->opening && a_event->menuName == "Loading Menu") {
				LoadGuard::LoadingMenuClosed();
			}

			if (a_event && a_event->opening && a_event->menuName == "RaceSex Menu") {
				logger::debug("{} detected, scheduling physics reset on world un-suspend.", a_event->menuName);
				hdt::SkyrimPhysicsWorld::get()->suspend(true);
			}

			if (a_event && !a_event->opening && (a_event->menuName == "RaceSex Menu")) {
				logger::debug("Racemenu closed, reloading meshes.");
				hdt::ActorManager::instance()->ProcessEvent(a_event, a_eventSource);
			}

			return RE::BSEventNotifyControl::kContinue;
		}
	}

	void Register()
	{
		Sinks::FreezeEventHandler::Register();
	}

	void Unregister()
	{
		Sinks::FreezeEventHandler::Unregister();
	}
}
