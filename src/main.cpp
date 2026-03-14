/**
 * Include the Geode headers.
 */
// ReSharper disable once CppUnusedIncludeDirective
#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
/**
 * Required to modify the MenuLayer class
 */
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include <camila314.geode-uri/include/GeodeURI.hpp>

#include <charconv>

#include "RatingsManager.h"
#include "Utils.h"
#include "modified/GDDLRobtopLevelsLayer.h"

/**
 * Brings cocos2d and all Geode namespaces
 * to the current scope.
 */
using namespace geode::prelude;

namespace {
    std::string_view trimSlashes(std::string_view path) {
        while (!path.empty() && path.front() == '/') {
            path.remove_prefix(1);
        }
        while (!path.empty() && path.back() == '/') {
            path.remove_suffix(1);
        }
        return path;
    }

    bool parsePositiveLevelID(std::string_view idPart, int& outID) {
        idPart = trimSlashes(idPart);
        if (idPart.empty()) {
            return false;
        }

        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(idPart.data(), idPart.data() + idPart.size(), parsed);
        if (ec != std::errc() || ptr != idPart.data() + idPart.size() || parsed <= 0) {
            return false;
        }

        outID = parsed;
        return true;
    }

    bool openLevelInfoByID(int levelID) {
        auto level = GameLevelManager::get()->getSavedLevel(levelID);
        if (!level) {
            level = GJGameLevel::create();
            if (!level) {
                return false;
            }
            level->m_levelID = levelID;
        }

        auto scene = LevelInfoLayer::scene(level, false);
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
        return true;
    }

    bool handleGDDLPlayURI(std::string_view path) {
        path = trimSlashes(path);

        if (path.rfind("play/", 0) == 0) {
            path.remove_prefix(5);
        }

        int levelID = 0;
        if (!parsePositiveLevelID(path, levelID)) {
            return false;
        }

        return openLevelInfoByID(levelID);
    }
}

/**
 * `$modify` lets you extend and modify GD's
 * classes; to hook a function in Geode,
 * simply $modify the class and write a new
 * function definition with the signature of
 * the one you want to hook.
 */
class $modify(MenuLayer) {
    /**
     * MenuLayer::onMoreGames is a GD function,
     * so we hook it by simply writing its name
     * inside a $modified MenuLayer class.
     *
     * Note that for all hooks, your signature
     * has to match exactly - `bool onMoreGames`
     * would not place a hook!
     */

    struct Fields {
        async::TaskHolder<web::WebResponse> cacheEventListener;
    };


    bool init() override {
        if (!MenuLayer::init()) return false;

        if (!RatingsManager::readCache) {
            // populate from save
            RatingsManager::readCache = true;
            RatingsManager::populateFromSave();
        }
        if (RatingsManager::readCache && RatingsManager::cacheEmpty() && !RatingsManager::triedToDownloadCache) {
            RatingsManager::triedToDownloadCache = true;
            auto req = web::WebRequest();
            req.header("User-Agent", Utils::getUserAgent());
            // if you're reading this because you treat this as an example of how to use the gddl api
            // cache
            // for the love of god
            // please
            m_fields->cacheEventListener.spawn(req.get(RatingsManager::gddlSheetUrl), Utils::getCacheDownloadLambda());
        }
        return true;
    }

    void onQuit(cocos2d::CCObject* sender) {
        MenuLayer::onQuit(sender);
        RatingsManager::cacheList(true); // cache modified list on every exit
    }
};

$on_mod(Loaded) {
    URIEvent("gddl").listen([](std::string_view path) {
        return handleGDDLPlayURI(path);
    }).leak();

    URIEvent("gddl/play").listen([](std::string_view path) {
        return handleGDDLPlayURI(path);
    }).leak();
}
