﻿#pragma once

//
// アプリの中核
//   ソフトリセットはこのクラスを再インスタンスすれば良い
//   画面遷移担当
//

#include <boost/noncopyable.hpp>
#include "ConnectionHolder.hpp"
#include "UIDrawer.hpp"
#include "TweenCommon.hpp"
#include "TaskContainer.hpp"
#include "MainPart.hpp"
#include "Intro.hpp"
#include "Title.hpp"
#include "Tutorial.hpp"
#include "GameMain.hpp"
#include "Result.hpp"
#include "Credits.hpp"
#include "Settings.hpp"
#include "Records.hpp"
#include "Ranking.hpp"
#include "Archive.hpp"
#include "Sound.hpp"
#include "DebugTask.hpp"


namespace ngs {

class Core
  : private boost::noncopyable
{
  void update(const Connection&, const Arguments& args) noexcept
  {
    auto current_time = boost::any_cast<double>(args.at("current_time"));
    auto delta_time   = boost::any_cast<double>(args.at("delta_time"));

    tasks_.update(current_time, delta_time);
  }


public:
  Core(const ci::JsonTree& params, Event<Arguments>& event) noexcept
    : params_(params),
      event_(event),
      archive_("records.json", params.getValueForKey<std::string>("app.version")),
      drawer_(params["ui"]),
      tween_common_(Params::load("tw_common.json"))
  {
    // 各種イベント登録
    // Intro→Title
    holder_ += event_.connect("Intro:finished",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       true, archive_.isSaved());
                              });

    // Title→GameMain
    holder_ += event_.connect("Title:finished",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<GameMain>(params_, event_, drawer_, tween_common_);

                                if (!archive_.getRecord<bool>("tutorial-finish")
#if defined (DEBUG)
                                    || Json::getValue(params_, "game.force_tutorial", false)
#endif
                                   )
                                {
                                  DOUT << "Tutorial started." << std::endl;
                                  tasks_.pushBack<Tutorial>(params_, event_, drawer_, tween_common_);
                                }
                              });
    // Title→Credits
    holder_ += event_.connect("Credits:begin",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Credits>(params_, event_, drawer_, tween_common_);
                              });
    // Credits→Title
    holder_ += event_.connect("Credits:Finished",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       false, archive_.isSaved());
                              });
    // Title→Settings
    holder_ += event_.connect("Settings:begin",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                Settings::Detail detail = {
                                  archive_.getRecord<bool>("bgm-enable"),
                                  archive_.getRecord<bool>("se-enable")
                                };

                                tasks_.pushBack<Settings>(params_, event_, drawer_, tween_common_, detail);
                              });
    // Settings→Title
    holder_ += event_.connect("Settings:Finished",
                              [this](const Connection&, const Arguments& args) noexcept
                              {
                                // Settingsの変更内容を記録
                                archive_.setRecord("bgm-enable", boost::any_cast<bool>(args.at("bgm-enable")));
                                archive_.setRecord("se-enable",  boost::any_cast<bool>(args.at("se-enable")));
                                archive_.save();

                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       false, archive_.isSaved());
                              });
    // Title→Records
    holder_ += event_.connect("Records:begin",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                Records::Detail detail = {
                                  archive_.getRecord<u_int>("play-times"),
                                  archive_.getRecord<u_int>("total-panels"),
                                  archive_.getRecord<u_int>("panel-turned-times"),
                                  archive_.getRecord<u_int>("panel-moved-times"),
                                  archive_.getRecord<u_int>("share-times"),
                                  archive_.getRecord<u_int>("startup-times"),
                                  archive_.getRecord<u_int>("abort-times"),

                                  archive_.getRecord<double>("average-score"),
                                  archive_.getRecord<double>("average-put-panels"),
                                  archive_.getRecord<double>("average-moved-times"),
                                  archive_.getRecord<double>("average-turn-times"),
                                  archive_.getRecord<double>("average-put-time"),
                                };

                                tasks_.pushBack<Records>(params_, event_, drawer_, tween_common_, detail);
                              });
    // Records→Title
    holder_ += event_.connect("Records:Finished",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       false, archive_.isSaved());
                              });
    // Title→Ranking
    holder_ += event_.connect("Ranking:begin",
                              [this](const Connection&, const Arguments& args) noexcept
                              {
                                Arguments ranking_args {
                                  { "games",   archive_.getRecordArray("games") },
                                  { "records", archive_.existsRanking() },
                                  { "view",    true }
                                };

                                tasks_.pushBack<Ranking>(params_, event_, drawer_, tween_common_, ranking_args);
                              });
    // Ranking→Title
    holder_ += event_.connect("Ranking:Finished",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       false, archive_.isSaved());
                              });

    // 本編開始
    holder_ += event_.connect("Game:Start",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                              });

    // ゲーム中断
    holder_ += event_.connect("Game:Aborted",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                       false, archive_.isSaved());
                              });
    // GameMain→Result
    holder_ += event_.connect("Result:begin",
                              [this](const Connection&, const Arguments& args) noexcept
                              {
                                tasks_.pushBack<Result>(params_, event_, drawer_, tween_common_, args);
                              });
    // Result→Title
    holder_ += event_.connect("Result:Finished",
                              [this](const Connection&, const Arguments& args) noexcept
                              {
                                bool rank_in = boost::any_cast<bool>(args.at("rank_in"));
                                if (rank_in)
                                {
                                  // TOP10に入っていたらRankingを起動
                                  Arguments ranking_args {
                                    { "games",   archive_.getRecordArray("games") },
                                    { "rank_in", rank_in },
                                    { "ranking", boost::any_cast<u_int>(args.at("ranking")) },
                                  };

                                  tasks_.pushBack<Ranking>(params_, event_, drawer_, tween_common_, ranking_args);
                                }
                                else
                                {
                                  tasks_.pushBack<Title>(params_, event_, drawer_, tween_common_,
                                                                           false, archive_.isSaved());
                                }
                              });

    holder_ += event_.connect("Tutorial:Complete",
                              [this](const Connection&, const Arguments&)
                              {
                                // Tutorialコンプリート
                                archive_.setRecord("tutorial-finish", true);
                                DOUT << "Tutorial completed." << std::endl;
                              });

    // ???
    holder_ += event_.connect("Share:completed",
                              [this](const Connection&, const Arguments&) noexcept
                              {
                                archive_.addRecord("share-times", uint32_t(1));
                                archive_.save();
                              });

    // system
    holder_ += event_.connect("update",
                              std::bind(&Core::update,
                                        this, std::placeholders::_1, std::placeholders::_2));

    // アプリの起動回数を更新して保存
    archive_.addRecord("startup-times", uint32_t(1));
    archive_.save();

    {
      // Sound初期設定
      auto bgm_enable = archive_.getRecord<bool>("bgm-enable");
      auto se_enable  = archive_.getRecord<bool>("se-enable");
      Arguments args = {
        { "bgm-enable", bgm_enable },
        { "se-enable",  se_enable }
      };
      event_.signal("Settings:Changed", args);
    }
    
    // 最初のタスクを登録
    tasks_.pushBack<Sound>(params_, event_);
    tasks_.pushBack<MainPart>(params_, event_, archive_);
    tasks_.pushBack<Intro>(params_, event_, drawer_, tween_common_);

#if defined (DEBUG)
    tasks_.pushBack<DebugTask>(params_, event_, drawer_);
#endif
  }

  ~Core() = default;


private:
  // メンバ変数を最後尾で定義する実験
  const ci::JsonTree& params_;

  Event<Arguments>& event_;
  ConnectionHolder holder_;

  TaskContainer tasks_;

  Archive archive_;


  // UI
  UI::Drawer drawer_;

  TweenCommon tween_common_;
};

}
