﻿#pragma once

//
// 結果画面
//

#include "Task.hpp"
#include "CountExec.hpp"
#include "UICanvas.hpp"
#include "TweenUtil.hpp"
#include "Score.hpp"
#include "Share.h"
#include "Capture.h"
#include "EventSupport.hpp"
#include "UISupport.hpp" 


namespace ngs {

class Result
  : public Task
{

public:
  Result(const ci::JsonTree& params, Event<Arguments>& event, UI::Drawer& drawer, TweenCommon& tween_common,
         const Arguments& args) noexcept
    : event_(event),
      ranking_text_(Json::getArray<std::string>(params["result.ranking"])),
      effect_speed_(Json::getVec<glm::vec3>(params["result.effect_speed"])),
      score_interval_(params.getValueForKey<double>("result.score-interval")),
      timeline_(ci::Timeline::create()),
      canvas_(event, drawer, tween_common,
              params["ui.camera"],
              Params::load(params.getValueForKey<std::string>("result.canvas")),
              Params::load(params.getValueForKey<std::string>("result.tweens")))
  {
    startTimelineSound(event_, params, "result.se");

    rank_in_      = getValue<bool>(args,  "rank_in");
    ranking_      = getValue<u_int>(args, "ranking");
    high_score_   = getValue<bool>(args,  "high_score");
    auto tutorial = getValue<bool>(args,  "tutorial"); 

    const auto& score = boost::any_cast<const Score&>(args.at("score"));

    total_score_ = score.total_score;
    total_rank_  = score.total_ranking;
    perfect_     = score.perfect;

#if defined (DEBUG)
    rank_in_     = Json::getValue(params, "result.force_rank_in",    rank_in_);
    high_score_  = Json::getValue(params, "result.force_high_score", high_score_);
    total_score_ = Json::getValue(params, "result.force_score",      total_score_);
    total_rank_  = Json::getValue(params, "result.force_rank",       total_rank_);
    perfect_     = Json::getValue(params, "result.force_perfect",    perfect_);
#endif

    share_text_ = replaceString(AppText::get(params.getValueForKey<std::string>("result.share")),
                                "%1",
                                std::to_string(score.total_score));

    auto wipe_delay    = params.getValueForKey<double>("ui.wipe.delay");
    auto wipe_duration = params.getValueForKey<double>("ui.wipe.duration");

    holder_ += event_.connect("agree:touch_ended",
                              [this, wipe_delay, wipe_duration](const Connection&, const Arguments&) noexcept
                              {
                                DOUT << "Agree." << std::endl;
                                canvas_.active(false);
                                canvas_.startCommonTween("root", "out-to-right");
                                count_exec_.add(wipe_delay,
                                                [this]() noexcept
                                                {
                                                  Arguments args {
                                                    { "rank_in", rank_in_ },
                                                    { "ranking", ranking_ },
                                                  };
                                                  event_.signal("Result:Finished", args);
                                                });
                                count_exec_.add(wipe_duration,
                                                [this]() noexcept
                                                {
                                                  active_ = false;
                                                });
                              });
    
    holder_ += event_.connect("share:touch_ended",
                              [this, wipe_delay](const Connection&, const Arguments&) noexcept
                              {
                                DOUT << "Share." << std::endl;

                                canvas_.active(false);
                                count_exec_.add(wipe_delay,
                                                [this]() noexcept
                                                {
                                                  auto* image = Capture::execute();

                                                  event_.signal("App:pending-update", Arguments());

                                                  Share::post(share_text_, image,
                                                              [this](bool completed) noexcept
                                                              {
                                                                if (completed)
                                                                {
                                                                  // 記録につけとく
                                                                  DOUT << "Share: completed." << std::endl;
                                                                  event_.signal("Share:completed", Arguments());
                                                                }
                                                                event_.signal("App:resume-update", Arguments());
                                                                canvas_.active(true);
                                                              });
                                                });
                              });

    // 画面Tapで演出をスキップ
    count_exec_.add(params.getValueForKey<double>("result.skip-delay"),
                    [this]()
                    {
                      holder_ += event_.connect("single_touch_ended",
                                                [this](const Connection& c, const Arguments&)
                                                {
                                                  if (!active_input_)
                                                  {
                                                    // 強制的に時間を進める
                                                    count_exec_.update(10);
                                                    timeline_->step(10);
                                                  }
                                                  c.disconnect();
                                                });
                    });


    if (Share::canPost() && Capture::canExec())
    {
      // Share機能と画面キャプチャが有効ならUIも有効
      canvas_.enableWidget("share");
      
      // ボタンのレイアウト変更
      auto p = canvas_.getWidgetParam("share", "offset");
      glm::vec2 ofs = *(boost::any_cast<glm::vec2*>(p));
      ofs.x = -ofs.x;
      canvas_.setWidgetParam("touch", "offset", ofs);
    }

    setupCommonTweens(event_, holder_, canvas_, "agree");
    setupCommonTweens(event_, holder_, canvas_, "share");

    double delay = 0.8;
    delay = applyScore(score, delay, params);
    auto duration = tweenTotalScore(params, delay);
    // ランクイン時の演出
    auto disp_delay_2 = duration + params.getValueForKey<float>("result.disp_delay_2");
    if (high_score_ || rank_in_)
    {
      count_exec_.add(disp_delay_2,
                      [this]() noexcept
                      {
                        effect_ = true;
                        if (high_score_)
                        {
                          canvas_.enableWidget("score:high-score");
                        }
                        else if (rank_in_)
                        {
                          canvas_.enableWidget("score:rank-in");
                        }

                        {
                          // SE
                          using namespace std::literals;

                          Arguments args{
                            { "name"s, "rank-in"s }
                          };
                          event_.signal("UI:sound"s, args);
                        }
                      });
    }
    if (perfect_ && !tutorial)
    {
      count_exec_.add(disp_delay_2,
                      [this]() noexcept
                      {
                        effect_ = true;
                        canvas_.enableWidget("score:perfect");
                      });
    }

    canvas_.startCommonTween("root", "in-from-left");

    // ボタン演出
    std::vector<std::pair<std::string, std::string>> widgets{
      { "touch", "touch:icon" },
      { "share", "share:icon" }
    };
    UI::startButtonTween(count_exec_, canvas_, disp_delay_2 + 0.25, 0.2, widgets);
    count_exec_.add(disp_delay_2,
                    [this]()
                    {
                      active_input_ = true;
                    });
  }

  ~Result() = default;


private:
  bool update(double current_time, double delta_time) noexcept override
  {
    count_exec_.update(delta_time);
    timeline_->step(delta_time);

    if (effect_)
    {
      if (high_score_ || rank_in_)
      {
        auto color = ci::hsvToRgb({ std::fmod(current_time * effect_speed_.x, 1.0), 0.75f, 1 });
        canvas_.setWidgetParam("score:20", "color", color);
        current_time += effect_speed_.y * delta_time;

        if (total_rank_ > 0)
        {
          color = ci::hsvToRgb({ std::fmod(current_time * effect_speed_.x, 1.0), 0.75f, 1 });
          for (int i = 0; i < 5; ++i)
          {
            char id[16];
            sprintf(id, "score:21-%d", i);
            canvas_.setWidgetParam(id, "color", color);
          }
        }
        current_time += effect_speed_.y * delta_time;

        color = ci::hsvToRgb({ std::fmod(current_time * effect_speed_.x, 1.0), 0.75f, 1 });
        canvas_.setWidgetParam("score:high-score", "color", color);
        canvas_.setWidgetParam("score:rank-in",    "color", color);
      }
      if (perfect_)
      {
        auto color = ci::hsvToRgb({ std::fmod(current_time * effect_speed_.x, 1.0), 0.75f, 1 });
        canvas_.setWidgetParam("score:perfect", "color", color);
      }
    }

    return active_;
  }

  
  double applyScore(const Score& score, double delay, const ci::JsonTree& params) noexcept
  {
    auto func = getEaseFunc(params.getValueForKey<std::string>("result.disp_ease"));

    // 森
    delay = panelScore(score.forest, "score:forest%d", delay, func);
    // 道
    delay = panelScore(score.path, "score:path%d", delay, func);

    // 街
    // canvas_.setWidgetText("score:1",  std::to_string(score.scores[5]));
    {
      // 教会
      const char* id = "score:2"; 
      canvas_.setWidgetText(id, std::to_string(score.scores[6]));
      count_exec_.add(delay,
                      [this, id]()
                      {
                        canvas_.setTweenTarget(id, "score", 0);
                        canvas_.startTween("score");
                        canvas_.enableWidget(id);

                        scoreSe();
                      });
      delay += score_interval_;
    }
    {
      // パネル数
      const char* id = "score:3"; 
      canvas_.setWidgetText(id, std::to_string(score.total_panels));
      count_exec_.add(delay,
                      [this, id]()
                      {
                        canvas_.setTweenTarget(id, "score", 0);
                        canvas_.startTween("score");
                        canvas_.enableWidget(id);

                        scoreSe();
                      });
      delay += score_interval_;
    }
    return delay;
  }

  double panelScore(const std::vector<u_int>& scores, const char* id_text, double delay, const ci::EaseFn& func)
  {
    if (scores.empty())
    {
      // スコア無し
      char id[16];
      sprintf(id, id_text, 0);
      count_exec_.add(delay,
                      [this, id]()
                      {
                        canvas_.setTweenTarget(id, "score", 0);
                        canvas_.startTween("score");
                        canvas_.enableWidget(id);

                        scoreSe();
                      });
      return delay + score_interval_;
    }

    int i = 0;
    float offset = 0.0f;
    for (auto f : scores)
    {
      if (i == 15) break;

      char id[16];
      sprintf(id, id_text, i);

      canvas_.setWidgetParam(id, "offset", glm::vec2(offset, 0));
      auto s = std::to_string(f);
      canvas_.setWidgetText(id, s);
      count_exec_.add(delay,
                      [this, id, f, func]()
                      {
                        canvas_.setTweenTarget(id, "score", 0);
                        canvas_.startTween("score");
                        canvas_.enableWidget(id);

                        scoreSe();
                      });
      delay += 0.15;
      i += 1;
      offset += 6.0f + 5.0f * s.size();
    }

    return delay;
  }


  // 演出時間を返却
  double tweenTotalScore(const ci::JsonTree& params, double delay) noexcept
  {
    // スコアのカウントアップ
    double duration = params.getValueForKey<float>("result.disp_duration");
    auto func = getEaseFunc(params.getValueForKey<std::string>("result.disp_ease"));
    setCountupTween("score:20", delay, duration, total_score_, func);

    {
      // ランク演出
      delay = duration + delay + 0.1;
      auto rank_icon = Json::getArray<std::string>(params["result.rank_icon"]);
      auto num = total_rank_ / 2;
      // 「半分の星」を下で表示している
      int i = 0;
      for (; i < num; ++i)
      {
        char id[16];
        sprintf(id, "score:21-%d", i);

        char se[16];
        sprintf(se, "rank-%d", i + 1);

        delay += 0.1f;

        count_exec_.add(delay,
                        [this, id, rank_icon, se]()
                        {
                          canvas_.setWidgetText(id, rank_icon[0]);
                          canvas_.setTweenTarget(id, "rank", 0);
                          canvas_.startTween("rank");

                          {
                            // SE
                            using namespace std::literals;

                            Arguments args{
                              { "name"s, std::string(se) }
                            };
                            event_.signal("UI:sound"s, args);
                          }
                        });
      }
      if (total_rank_ & 1)
      {
        char id[16];
        sprintf(id, "score:21-%d", i);

        char se[16];
        sprintf(se, "rank-%d", i + 1);

        delay += 0.1f;

        count_exec_.add(delay,
                        [this, id, rank_icon, se]()
                        {
                          canvas_.setWidgetText(id, rank_icon[1]);
                          canvas_.setTweenTarget(id, "rank", 0);
                          canvas_.startTween("rank");

                          {
                            // SE
                            using namespace std::literals;

                            Arguments args{
                              { "name"s, std::string(se) }
                            };
                            event_.signal("UI:sound"s, args);
                          }
                        });
      }

      // 最終的な演出時間
      duration = delay + 0.3;
    }
    return duration;
  }

  // 数値のカウントアップ演出
  void setCountupTween(const std::string& id, double delay, float duration, int score, const ci::EaseFn& func)
  {
    if (!score) return;

    disp_scores_.insert({ id, 0 });
    se_scores_.insert({ id, 0 });

    count_exec_.add(delay,
                    [this, id, score, duration, func]()
                    {
                      // Tweenでカウントアップ
                      auto option = timeline_->apply(&disp_scores_[id], score,
                                                     duration,
                                                     func);
                      option.updateFn([this, id]() noexcept
                                      {
                                        canvas_.setWidgetText(id, std::to_string(disp_scores_[id]));
                                        if (!((se_scores_[id] += 1) & 0b11))
                                        {
                                          scoreSe();
                                        }
                                      });
                    });
  }

  void scoreSe()
  {
    using namespace std::literals;

    char text[16];
    sprintf(text, "drum-roll-%d", drum_index_);
    Arguments args{
      { "name"s, std::string(text) }
    };
    event_.signal("UI:sound"s, args);
    drum_index_ = std::min(drum_index_ + 1, 16);
  }


  Event<Arguments>& event_;
  ConnectionHolder holder_;

  CountExec count_exec_;

  bool rank_in_;
  u_int ranking_;
  bool high_score_;
  bool perfect_;

  std::vector<std::string> ranking_text_;

  int total_score_;
  int total_rank_;
  bool effect_ = false;
  glm::vec3 effect_speed_;

  bool active_input_ = false;

  double score_interval_;

  int drum_index_ = 1;

  std::map<std::string, ci::Anim<int>> disp_scores_;
  std::map<std::string, int> se_scores_;

  // Share機能用の文章
  std::string share_text_;

  ci::TimelineRef timeline_;

  UI::Canvas canvas_;
  bool active_ = true;
};

}
